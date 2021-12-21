#include "StdAfx.h"
#include "AmigaFileSystem.h"
#include "InfoIcon.h"
#include "../NUTS/Defs.h"
#include "../NUTS/PluginDescriptor.h"

FSHint AmigaFileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.FSID       = MYFSID;
	hint.Confidence = 0;

	BYTE Sector[512];

	pSource->ReadSectorLBA( 0, Sector, 512 );

	if ( rstrncmp( Sector, (BYTE *) "DOS", 3 ) )
	{
		if ( ( Sector[ 3 ] & 1 ) && ( MYFSID == FSID_AMIGAF ) )
		{
			hint.Confidence = 20;
		}

		if ( ( ( Sector[ 3 ] & 1 ) == 0 ) && ( MYFSID == FSID_AMIGAO ) )
		{
			hint.Confidence = 20;
		}		

		if ( MYFSID ==  FSID_AMIGADMS )
		{
			/* If we're here then the DMS extract worked. If the first 3 bytes are "DOS" then that worked too */

			/* But let's not be toooo hasty - bahrum */
			hint.Confidence = 15;

			if ( ( Extension != nullptr ) && ( rstrncmp( Extension, (BYTE *) "DMS", 3 ) ) )
			{
				hint.Confidence += 15;
			}
		}
	}

	return hint;
}

int	AmigaFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	DWORD fheader = pDirectory->Files[ FileID ].Attributes[ 0 ];

	/* What we start with, is a file header. 

	   "Strap in tight, Lassie - It gets bumpy f'here." -- Scotty, Star Trek IV
	*/

	ReadingLength = (DWORD) pDirectory->Files[ FileID ].Length;

	return ReadFileBlocks( fheader, store );
}

int AmigaFileSystem::ReadFileBlocks( DWORD FileHeaderBlock, CTempFile &store )
{
	BYTE Sector[512];
	BYTE DataSector[512];

	pSource->ReadSectorLBA( FileHeaderBlock, Sector, 512 );

	DWORD BlockType = BEDWORD( &Sector[ 0 ] );

	if ( BlockType == 2 ) /* File header. */
	{
		DWORD *pBlocks = (DWORD *) &Sector[ 0x24 ];

		DWORD FileBlock = 71;

		while ( ( ReadingLength > 0 ) && ( FileBlock > 0 ) )
		{
			DWORD BlockNum = BEDWORD( (BYTE *) &pBlocks[ FileBlock ] );

			if ( BlockNum == 0 )
			{
				FileBlock--;

				continue;
			}

			pSource->ReadSectorLBA( BlockNum, DataSector, 512 );

			FileBlock--;

			DWORD DataBlockType = BEDWORD( &DataSector[ 0 ] );

			if ( DataBlockType == 16 ) /* Extension block - pass to self */
			{
				ReadFileBlocks( BlockNum, store );
			}
			else if ( DataBlockType == 8 ) /* Data Block */
			{
				/* Finally some fecking data */
				DWORD DataSize = ReadingLength;

				if ( DataSize > 0x1E8 )
				{
					DataSize = 0x1E8;
				}

				store.Write( &DataSector[ 0x18 ], DataSize );

				ReadingLength -= DataSize;
			}
		}

		return 0;
	}
	else if ( BlockType == 16 ) /* Extension block */
	{
		DWORD *pBlocks = (DWORD *) &Sector[ 0x24 ];

		DWORD ExtnBlock = 71;

		while ( ExtnBlock > 0 )
		{
			DWORD BlockNum = BEDWORD( (BYTE *) &pBlocks[ ExtnBlock ] );

			if ( BlockNum == 0 )
			{
				ExtnBlock--;

				continue;
			}

			pSource->ReadSectorLBA( BlockNum, DataSector, 512 );

			ExtnBlock--;

			DWORD DataBlockType = BEDWORD( &DataSector[ 0 ] );

			if ( DataBlockType == 16 ) /* Extension block - pass to self */
			{
				ReadFileBlocks( BlockNum, store );
			}
			else if ( DataBlockType == 8 ) /* Data Block */
			{
				/* Finally some fecking data */
				DWORD DataSize = ReadingLength;

				if ( DataSize > 0x1E8 )
				{
					DataSize = 0x1E8;
				}

				store.Write( &DataSector[ 0x18 ], DataSize );

				ReadingLength -= DataSize;
			}
		}

		return 0;
	}

	return 0;
}

int AmigaFileSystem::ChangeDirectory( DWORD FileID ) {
	if ( !IsRoot() )
	{
		path += "/";
	}
	
	path += std::string( (char *) pDirectory->Files[ FileID ].Filename );

	pAmigaDirectory->DirSector = pDirectory->Files[ FileID ].Attributes[ 0 ];

	pDirectory->ReadDirectory();

	ResolveIcons();

	return 0;
}

int AmigaFileSystem::Parent() {
	pAmigaDirectory->DirSector = pAmigaDirectory->ParentSector;

	pDirectory->ReadDirectory();

	ResolveIcons();

	if ( IsRoot() )
	{
		path = "/";
	}
	else
	{
		size_t spos = path.find_last_of( '/' );

		if ( spos != std::string::npos )
		{
			path = path.substr( 0, spos );
		}
	}

	return 0;
}

void AmigaFileSystem::ResolveIcons( void )
{
	NativeFileIterator iFile;

	std::vector<DWORD> RemoveIDs;

	for ( iFile=pDirectory->Files.begin(); iFile!=pDirectory->Files.end(); iFile++ )
	{
		bool Removed = false;

		WORD fl = rstrnlen( iFile->Filename, 256 );

		/* Skip if this is a .info file */
		if ( fl>=6 )
		{
			if ( rstrnicmp( &iFile->Filename[ fl - 5 ], (BYTE *) ".info", 255 ) )
			{
				RemoveIDs.push_back( iFile->fileID );

				continue;
			}
		}

		BYTE PredictedIconName[ 256 ];

		rstrncpy( PredictedIconName, iFile->Filename, 256 );
		rstrncat( PredictedIconName, (BYTE *) ".info", 256 );

		NativeFileIterator iIcon;

		for ( iIcon=pDirectory->Files.begin(); iIcon!=pDirectory->Files.end(); iIcon++)
		{
			if ( rstrnicmp( iIcon->Filename, PredictedIconName, 255 ) )
			{
				CTempFile fileObj;

				ReadFile( iIcon->fileID, fileObj );

				InfoIcon icon( fileObj );

				if ( icon.HasIcon )
				{
					IconDef ResolvedIcon;

					ResolvedIcon.Aspect = AspectRatio( 640, 256 );

					icon.GetNaturalBitmap( &ResolvedIcon.bmi, (BYTE **) &ResolvedIcon.pImage );
					
					if ( UseResolvedIcons )
					{
						pDirectory->ResolvedIcons[ iFile->fileID ] = ResolvedIcon;

						iFile->HasResolvedIcon = true;
					}

					iFile->Attributes[ 1 ] = iIcon->Attributes[ 0 ];
				}

				if ( rstrnicmp( icon.pIconTool, (BYTE *) ":AmigaBASIC", 255 ) )
				{
					iFile->Type     = FT_BASIC;
					iFile->Icon     = FT_BASIC;
					iFile->XlatorID = TUID_TEXT;
				}

				for ( std::vector<BYTE *>::iterator iT = icon.ToolTypes.begin(); iT != icon.ToolTypes.end(); iT++ )
				{
					if ( rstrcmp( *iT, (BYTE *) "FILETYPE=ILBM", true ) )
					{
						iFile->Type     = FT_Graphic;
						iFile->Icon     = FT_Graphic;
						iFile->XlatorID = TUID_ILBM;
					}
				}
			}
		}
	}

	if ( HideSidecars )
	{
		/* Re-assign FileIDs or remove unwated files */
		DWORD FileID = 0;

		std::map<DWORD, IconDef> RemappedIcons;

		for ( iFile=pDirectory->Files.begin(); iFile!=pDirectory->Files.end(); )
		{
			std::vector<DWORD>::iterator iRemoved;

			bool RemoveThis = false;

			for ( iRemoved = RemoveIDs.begin(); iRemoved != RemoveIDs.end(); iRemoved++ )
			{
				if ( iFile->fileID == *iRemoved )
				{
					RemoveThis = true;
				}
			}
		
			if ( RemoveThis )
			{
				iFile = pDirectory->Files.erase( iFile );
			}
			else
			{
				RemappedIcons[ FileID ] = pDirectory->ResolvedIcons[ iFile->fileID ];

				iFile->fileID = FileID++;

				iFile++;
			}
		}

		pDirectory->ResolvedIcons = RemappedIcons;
	}
}

BYTE *AmigaFileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE status[64];

	if ( pDirectory->Files[ FileIndex ].Flags & FF_Directory )
	{
		rsprintf( status, "Directory" );
	}
	else
	{
		rsprintf( status, "File - %d bytes", (DWORD) pDirectory->Files[ FileIndex ].Length );
	}

	return status;
}

BYTE *AmigaFileSystem::GetStatusString( int FileIndex, int SelectedItems )
{
	static BYTE status[64];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "%d File System Objects", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d Items Selected", SelectedItems );
	}
	else if ( pDirectory->Files[ FileIndex ].Flags & FF_Directory )
	{
		rsprintf( status, "%s - Directory", (BYTE *) pDirectory->Files[ FileIndex ].Filename );
	}
	else
	{
		rsprintf( status, "%s - File - %d bytes", (BYTE *) pDirectory->Files[ FileIndex ].Filename, (DWORD) pDirectory->Files[ FileIndex ].Length );
	}

	return status;
}

BYTE *AmigaFileSystem::GetTitleString( NativeFile *pFile, DWORD Flags )
{
	static BYTE title[ 512 ];

	if ( FSID == FSID_AMIGAO )
	{
		rsprintf( title, "OFS:%s%s", VolumeName, path.c_str() );
	}
	else
	{
		rsprintf( title, "FFS:%s%s", VolumeName, path.c_str() );
	}

	if ( pFile != nullptr )
	{
		rstrncat( title, (BYTE *) "/", 512 );
		rstrncat( title, (BYTE *) pFile->Filename, 512 );
	}

	if ( Flags & TF_Titlebar )
	{
		if ( !(Flags & TF_Final ) )
		{
			rstrncat( title, (BYTE *) " > ", 511 );
		}
	}

	return title;
}

AttrDescriptors AmigaFileSystem::GetAttributeDescriptions( NativeFile *pFile )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Start block. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile | AttrDir;
	Attr.Name  = L"Object Block";
	Attrs.push_back( Attr );

	/* Sidecar block. Hex, visible, disabled */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile | AttrDir;
	Attr.Name  = L"Sidecar Block";
	Attrs.push_back( Attr );

	return Attrs;
}