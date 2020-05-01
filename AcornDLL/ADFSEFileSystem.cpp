#include "StdAfx.h"
#include "ADFSEFileSystem.h"
#include "RISCOSIcons.h"
#include "../nuts/libfuncs.h"
#include "../NUTS/NestedImageSource.h"
#include "SpriteFile.h"
#include "Sprite.h"

#include <assert.h>

FSHint ADFSEFileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FS_Null;

	BYTE SectorBuf[1024];

	/* Use ReadRaw because the sector size isn't known */
	pSource->ReadRaw( 0, 1024, SectorBuf );

	BYTE CheckByte = pFSMap->ZoneCheck( SectorBuf );

	if ( CheckByte == SectorBuf[ 0 ] )
	{
		pSource->ReadRaw( 0x800, 256, SectorBuf );

		if ( rstrncmp( &SectorBuf[ 1 ], (BYTE *) "Nick", 4 ) )
		{
			hint.Confidence = 25;
			hint.FSID       = FSID_ADFS_E;

			return hint;
		}
	}

	// Sector 0 can be damaged, because there's a backup on Sector 1.
	
	pSource->ReadRaw( 1024, 1024, SectorBuf );

	CheckByte = pFSMap->ZoneCheck( SectorBuf );

	if ( CheckByte == SectorBuf[ 0 ] )
	{
		pSource->ReadRaw( 0x800, 256, SectorBuf );

		if ( rstrncmp( &SectorBuf[ 1 ], (BYTE *) "Nick", 4 ) )
		{
			hint.Confidence = 25;
			hint.FSID       = FSID_ADFS_E;

			return hint;
		}
	}
	
	/* Going to need to read the Boot Block and locate the map base. */

	pSource->ReadRaw( 0x0C00, 1024, SectorBuf );

	BYTE *pRecord   = &SectorBuf[ 0x1C0 ]; // Bit o' boot block.
	DWORD BPMB      = 1 << pRecord[ 0x05 ];
	WORD  ZoneSpare = * (WORD *) &pRecord[ 0x0A ];
	WORD  Zones     = pRecord[ 0x09 ];

	if ( Zones < 2 )
	{
		/* We're beyond E format and into the multi-zone F/F+/G/G+ territory now,
		   so if Zones is less than 2, this ain't one o' them. */
		return hint;
	}

	/* Calculate location of root zone */
	DWORD DR_Size = 60;
	DWORD ZZ      = DR_Size * 8;

	DWORD MapAddr = ((Zones / 2) * (8*1024-ZoneSpare)-ZZ)*BPMB;
	DWORD Bits    = 0;

	pSource->ReadRaw( MapAddr, 1024, SectorBuf );

	CheckByte = pFSMap->ZoneCheck( SectorBuf );

	if ( CheckByte == SectorBuf[ 0 ] )
	{
		hint.Confidence = 25;
		hint.FSID       = FSID_ADFS_F;

		return hint;
	}
	
	return hint;
}


int ADFSEFileSystem::ChangeDirectory( DWORD FileID )
{
	if ( FileID > pDirectory->Files.size() )
	{
		return -1;
	}

	NativeFile *file = &pDirectory->Files[ FileID ];

	pEDirectory->DirSector = file->SSector;

	rstrncat(path, (BYTE *) ".",   512 );
	rstrncat(path, file->Filename, 512 );

	pDirectory->ReadDirectory();

	ResolveAppIcons();

	return 0;
}


int	ADFSEFileSystem::Parent() {	
	if ( IsRoot() )
	{
		return -1;
	}

	pEDirectory->DirSector = pEDirectory->ParentSector;

	pDirectory->ReadDirectory();

	ResolveAppIcons();

	BYTE *p = rstrrchr( path, (BYTE) '.', 512 );

	if (p)
		*p	= 0;

	return 0;
}

int	ADFSEFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	BYTE Sector[ 1024 ];

	DWORD fileSize = (DWORD) pFile->Length;

	FileFragments Frags = pFSMap->GetFileFragments( pFile->SSector );

	FileFragment_iter iFrag = Frags.begin();

	DWORD FragSectors = 0;
	DWORD FragSector  = 0;

	while ( iFrag != Frags.end() ) {
		FragSectors = iFrag->Length / 1024;

		for ( FragSector = 0; FragSector < FragSectors; FragSector++ )
		{
			if( fileSize == 0 )
			{
				return 0;
			}

			pSource->ReadSector( iFrag->Sector + FragSector, Sector, 1024 );

			if (fileSize > 1024 )
			{
				store.Write( Sector, 1024 );

				fileSize -= 1024;
			}
			else
			{
				store.Write( Sector, fileSize );

				fileSize = 0;
			}
		}

		iFrag++;

		if ( fileSize == 0 )
		{
			return 0;
		}
	}

	return 0;
}


BYTE *ADFSEFileSystem::DescribeFile(DWORD FileIndex) {
	static BYTE status[ 128 ];

	NativeFile	*pFile	= &pDirectory->Files[FileIndex];

	DWORD Type = ( pFile->LoadAddr & 0x000FFF00 ) >> 8;

	std::string FileTypeName = RISCOSIcons::GetNameForType( Type );

	const char *pTypeName = FileTypeName.c_str();

	if ( pFile->Flags & FF_Directory )
	{
		rsprintf( status, "[%s%s%s%s] - Directory",
			(pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-"
		);
	}
	else
	{
		sprintf_s( (char *) status, 128, "[%s%s%s%s] %08X bytes, %s/%03X",
			(pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-",
			(DWORD) pFile->Length, pTypeName, Type
		);
	}
		
	return status;
}

BYTE *ADFSEFileSystem::GetStatusString( int FileIndex, int SelectedItems ) {
	static BYTE status[128];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "%d File System Objects", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d Items Selected", SelectedItems );
	}
	else 
	{
		NativeFile	*pFile	= &pDirectory->Files[FileIndex];

		DWORD Type = ( pFile->LoadAddr & 0x000FFF00 ) >> 8;

		std::string FileTypeName = RISCOSIcons::GetNameForType( Type );

		const char *pTypeName = FileTypeName.c_str();

		if ( pFile->Flags & FF_Directory )
		{
			rsprintf( status, "%s | [%s%s%s%s] - Directory",
				pFile->Filename, (pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-"
			);
		}
		else
		{
			rsprintf( status, "%s | [%s%s%s%s] - %0X bytes - %s/%03X",
				pFile->Filename, (pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-",
				(DWORD) pFile->Length, (char *) pTypeName, Type
			);
		}
	}
		
	return status;
}

BYTE *ADFSEFileSystem::GetTitleString( NativeFile *pFile )
{
	static BYTE ADFSPath[512];

	std::string sPath = "ADFS::" + std::string( (char *) pFSMap->DiscName ) + "." + std::string( (char *) path );

	if ( pFile != nullptr )
	{
		sPath += "." + std::string( (char *) pFile->Filename ) + " > ";
	}

	rstrncpy( ADFSPath, (BYTE *) sPath.c_str(), 511 );

	return ADFSPath;
}

DataSource *ADFSEFileSystem::FileDataSource( DWORD FileID )
{
	CTempFile FileObj;

	ReadFile( FileID, FileObj );

	FileObj.Keep();

	std::string sName = FileObj.Name();

	const char *pName = sName.c_str();

	return new NestedImageSource( this, &pDirectory->Files[ FileID ], UString( (char *) pName ) );
}

int ADFSEFileSystem::ResolveAppIcons( void )
{
	if ( !UseResolvedIcons )
	{
		return 0;
	}

	DWORD MaskColour = GetSysColor( COLOR_WINDOW );

	NativeFileIterator iFile;

	for (iFile=pDirectory->Files.begin(); iFile!=pDirectory->Files.end(); iFile++)
	{
		if ( iFile->Flags & FF_Directory )
		{
			ADFSEFileSystem TempFS( *this );

			TempFS.ChangeDirectory( iFile->fileID );

			NativeFileIterator iSpriteFile;

			for ( iSpriteFile = TempFS.pDirectory->Files.begin(); iSpriteFile != TempFS.pDirectory->Files.end(); iSpriteFile++)
			{
				if ( rstricmp( iSpriteFile->Filename, (BYTE *) "!Sprites" ) )
				{
					DataSource *pSpriteSource = TempFS.FileDataSource( iSpriteFile->fileID );

					SpriteFile spriteFile( pSpriteSource );

					spriteFile.Init();

					NativeFileIterator iSprite;

					for ( iSprite = spriteFile.pDirectory->Files.begin(); iSprite != spriteFile.pDirectory->Files.end(); iSprite++ )
					{
						if ( rstricmp( iSprite->Filename, iFile->Filename ) )
						{
							CTempFile FileObj;

							spriteFile.ReadFile( iSprite->fileID, FileObj );

							Sprite sprite( FileObj );

							IconDef icon;

							sprite.GetNaturalBitmap( &icon.bmi, &icon.pImage, MaskColour );

							icon.Aspect = sprite.SpriteAspect;

							pDirectory->ResolvedIcons[ iFile->fileID ] = icon;

							iFile->HasResolvedIcon = true;
						}
						/*
						if ( rstrnicmp( iSprite->Filename, (BYTE *) "file_", 5 ) )
						{
							ResolveAuxFileType( &*iSprite, &*iFile, spriteFile );
						}
						*/
					}

					pSpriteSource->Release();
				}
			}
		}
	}

	return 0;
}


int ADFSEFileSystem::ResolveAuxFileType( NativeFile *pSprite, NativeFile *pFile, SpriteFile &spriteFile )
{
	DWORD AuxTypeID  = 0x0000;
	DWORD MaskColour = GetSysColor( COLOR_WINDOW );

	for (BYTE i=0; i<3; i++)
	{
		AuxTypeID <<= 4;

		BYTE *pC = &pSprite->Filename[ 5 + i ];

		if ( ( *pC >= '0' ) && ( *pC <= '9' ) )
		{
			AuxTypeID |= *pC - '0';
		}
		else if ( ( *pC >= 'A' ) && ( *pC <= 'F' ) )
		{
			AuxTypeID |= ( *pC - 'A' ) + 10;
		}
		else if ( ( *pC >= 'a' ) && ( *pC <= 'f' ) )
		{
			AuxTypeID |= ( *pC - 'a' ) + 10;
		}
		else
		{
			return -1;
		}
	}

	CTempFile FileObj;

	spriteFile.ReadFile( pSprite->fileID, FileObj );

	Sprite sprite( FileObj );

	IconDef icon;

	sprite.GetNaturalBitmap( &icon.bmi, &icon.pImage, MaskColour );

	HBITMAP pAuxBitmap = CreateBitmap( icon.bmi.biWidth, icon.bmi.biHeight, icon.bmi.biPlanes, icon.bmi.biBitCount, icon.pImage );

	RISCOSIcons::AddIconMaps( 0x0, AuxTypeID, FT_Arbitrary, std::string( (char *) pSprite->Filename ), pAuxBitmap );

	return 0;
}

FileSystem *ADFSEFileSystem::FileFilesystem( DWORD FileID )
{
	if ( pDirectory->Files[ FileID ].RISCTYPE == 0xFF9 )
	{
		/* Sprite file. These are quite difficult to detect, so we'll offer a ready made FS here. */
		DataSource *pSource = FileDataSource( FileID );

		FileSystem *pSpriteFS = new SpriteFile( pSource );

		return pSpriteFS;
	}

	return nullptr;
}

int ADFSEFileSystem::Refresh( void )
{
	pDirectory->ReadDirectory();

	ResolveAppIcons();

	return 0;
}


WCHAR *ADFSEFileSystem::Identify( DWORD FileID )
{
	static WCHAR *BASICFile  = L"BASIC Program";
	static WCHAR *DrawFile   = L"Draw File";
	static WCHAR *ExecFile   = L"Exec (Spool)";
	static WCHAR *FontFile   = L"Font";
	static WCHAR *JPEGFile   = L"JPEG Image";
	static WCHAR *ModFile    = L"Module";
	static WCHAR *ObeyFile   = L"Obey (Script)";
	static WCHAR *SpriteFile = L"Sprite File";
	static WCHAR *UtilFile   = L"Utility";
	static WCHAR *AppFile    = L"Application";
	static WCHAR *DataFile   = L"Data File";
	static WCHAR *TextFile   = L"Text File";

	switch ( pDirectory->Files[ FileID ].RISCTYPE )
	{
	case 0xFFB:
		return BASICFile;
	case 0xAFF:
		return DrawFile;
	case 0xFFE:
		return ExecFile;
	case 0xFF7:
		return FontFile;
	case 0xC85:
		return JPEGFile;
	case 0xFFA:
		return ModFile;
	case 0xFEB:
		return ObeyFile;
	case 0xFF9:
		return SpriteFile;
	case 0xFFC:
		return UtilFile;
	case 0xA01:
		return AppFile;
	case 0xFFD:
		return DataFile;
	case 0xFFF:
		return TextFile;
	default:
		return FileSystem::Identify( FileID );
	}

	return FileSystem::Identify( FileID );
}


int ADFSEFileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	ResetEvent( hCancelFree );

	static FSSpace Map;

	pFSMap->ReadFSMap();

	Fragment_iter iFragment;
	DWORD         NumBlocks = 0;

	for ( iFragment = pFSMap->Fragments.begin(); iFragment != pFSMap->Fragments.end(); iFragment++ )
	{
		NumBlocks += iFragment->Length;
	}

	NumBlocks &= 0xFFFFFFFF;

	Map.Capacity  = NumBlocks;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = Map.Capacity;

	NumBlocks /= 1024;

	double BlockRatio = ( (double) NumBlocks / (double) TotalBlocks );

	if ( BlockRatio < 1.0 ) { BlockRatio = 1.0 ; }
	
	memset( pBlockMap, BlockAbsent, TotalBlocks );
	memset( pBlockMap, BlockFree, (DWORD) ( NumBlocks / BlockRatio ) );

	DWORD BlkNum;

	if ( pFSMap->Zones > 1 )
	{
		/* This has a Boot Block at sector 3 */
		BlkNum = (DWORD) ( (double) 3 / BlockRatio );
	
		pBlockMap[ BlkNum ] = (BYTE) BlockFixed;
	}

	for ( iFragment = pFSMap->Fragments.begin(); iFragment != pFSMap->Fragments.end(); iFragment++ )
	{
		if ( WaitForSingleObject( hCancelFree, 10 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		if ( iFragment->FragID != 0U )
		{
			Map.UsedBytes -= iFragment->Length;

			/* This is a bit wrong, as it doesn't take into account pieces of fragments (I hate you for this Acorn),
			   but it will give a good enough approximation for the free space/block map dialog. */
			FileFragments FFragments = pFSMap->GetFileFragments( iFragment->FragID << 8 );

			FileFragment_iter iF;

			for ( iF = FFragments.begin(); iF != FFragments.end(); iF++ )
			{
				if ( WaitForSingleObject( hCancelFree, 10 ) == WAIT_OBJECT_0 )
				{
					return 0;
				}

				for ( DWORD Blk = iF->Sector; Blk != iF->Sector + (iF->Length/1024); Blk++ )
				{
					BlkNum = (DWORD) ( (double) Blk / BlockRatio );

					assert( BlkNum < TotalBlocks );
					
					if ( ( pBlockMap[ BlkNum ] == BlockFree ) || ( pBlockMap[ BlkNum ] == BlockUsed ) )
					{
						if ( iFragment->FragID == 2 ) /* Root Directory */
						{
							pBlockMap[ BlkNum ] = BlockFixed;
						}
						else
						{
							pBlockMap[ BlkNum ] = BlockUsed;
						}
					}
				}
			}

			SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );
		}
	}

	return 0;
}

AttrDescriptors ADFSEFileSystem::GetFSAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;
	
	Attrs.clear();

	AttrDesc Attr;

	/* Disc Title */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrString | AttrEnabled;
	Attr.Name  = L"Disc Title";

	Attr.MaxStringLength = 16;
	Attr.pStringVal      = rstrndup( pFSMap->DiscName, 16 );

	Attrs.push_back( Attr );

	/* Boot Option */
	/* File type */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect;
	Attr.Name  = L"Boot Option";

	Attr.StartingValue  = 3; // pFSMap->BootOpt;

	AttrOption opt;

	opt.Name            = L"No Boot";
	opt.EquivalentValue = 0x00000000;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Load";
	opt.EquivalentValue = 0x00000001;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Run";
	opt.EquivalentValue = 0x00000002;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Exec";
	opt.EquivalentValue = 0x00000003;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	Attrs.push_back( Attr );

	return Attrs;
}

AttrDescriptors ADFSEFileSystem::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Start sector. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile | AttrDir;
	Attr.Name  = L"Start sector";
	Attrs.push_back( Attr );

	/* Locked */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"Locked";
	Attrs.push_back( Attr );

	/* Read */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"Read";
	Attrs.push_back( Attr );

	/* Write */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"Read";
	Attrs.push_back( Attr );

	/* Load address. Hex. */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Load address";
	Attrs.push_back( Attr );

	/* Exec address. Hex. */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Execute address";
	Attrs.push_back( Attr );

	/* Sequence number. Hex. */
	Attr.Index = 6;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrFile | AttrDir;
	Attr.Name  = L"Sequence #";
	Attrs.push_back( Attr );

	/* File Type. Hex. */
	Attr.Index = 7;
	Attr.Type  = AttrVisible | AttrEnabled | AttrCombo | AttrHex | AttrFile;
	Attr.Name  = L"File Type";

	static DWORD    FileTypes[ 13 ] = {
		0xFFB, 0xFFD, 0xAFF, 0xFFE, 0xFF7, 0xC85, 0xFFA, 0xFEB, 0xFF9, 0xFFF, 0xFFC
	};

	static std::wstring Names[ 13 ] = {
		L"BASIC Program", L"Data", L"Draw File", L"Exec (Spool)", L"Font", L"JPEG Image", L"Module", L"Obey (Script)", L"Sprite", L"Text File", L"Utility"
	};

	for ( BYTE i=0; i<13; i++ )
	{
		AttrOption opt;

		opt.Name            = Names[ i ];
		opt.EquivalentValue = FileTypes[ i ];
		opt.Dangerous       = false;

		Attr.Options.push_back( opt );
	}

	Attrs.push_back( Attr );

	/* Time Stamp */
	Attr.Index = 8;
	Attr.Type  = AttrVisible | AttrEnabled | AttrTime | AttrFile | AttrDir;
	Attr.Name  = L"Time Stamp";
	Attrs.push_back( Attr );

	return Attrs;
}
