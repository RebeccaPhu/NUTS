#include "StdAfx.h"
#include "D64FileSystem.h"
#include "CBMFunctions.h"

#include "../NUTS/NUTSError.h"

int	D64FileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	//	See notes in D64Directory regarding file trapesing.

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	int	foffset	= 0;
	int	logsect	= pFile->Attributes[ 0 ];

	unsigned char sector[256];

	D64Directory *pd = (D64Directory *) pDirectory;

	while (logsect != -1) {
		pSource->ReadSector(logsect, sector, 256);

		if (sector[0] != 0) {
			store.Write( &sector[2], 254 );

			logsect	= SectorForLink(sector[0], sector[1]);

			foffset	+= 254;
		} else {
			store.Write( &sector[2], (DWORD) sector[1] );

			logsect	= -1;
		}
	}

	return 0;
}

int	D64FileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	if ( ( pFile->EncodingID != ENCODING_PETSCII ) && ( pFile->EncodingID != ENCODING_ASCII ) )
	{
		return FILEOP_NEEDS_ASCII;
	}

	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( &*iFile, pFile ) )
		{
			return FILEOP_EXISTS;
		}
	}

	NativeFile file = *pFile;

	if ( file.FSFileType != FT_C64 )
	{
		file.Attributes[ 2 ] = 0x00000000;
		file.Attributes[ 3 ] = 0xFFFFFFFF;

		if ( file.Flags & FF_Extension )
		{
			if ( rstrnicmp( file.Extension, (BYTE *) "DEL", 3 ) )
			{
				file.Attributes[ 1 ] = 0;
			}
			else if ( rstrnicmp( file.Extension, (BYTE *) "SEQ", 3 ) )
			{
				file.Attributes[ 1 ] = 1;
			}
			else if ( rstrnicmp( file.Extension, (BYTE *) "PRG", 3 ) )
			{
				file.Attributes[ 1 ] = 2;
			}
			else if ( rstrnicmp( file.Extension, (BYTE *) "USR", 3 ) )
			{
				file.Attributes[ 1 ] = 3;
			}
			else if ( rstrnicmp( file.Extension, (BYTE *) "REL", 3 ) )
			{
				file.Attributes[ 1 ] = 4;
			}
			else
			{
				file.Attributes[ 1 ] = 2; // Make it a PRG
			}
		}
		else
		{
			file.Attributes[ 1 ] = 2;
		}
	}

	if ( file.EncodingID != ENCODING_PETSCII )
	{
		IncomingASCII( &file );
	}

	/* Enough disk space? */
	DWORD RequiredSectors = file.Length / 254; // 2 bytes of link.

	if ( file.Length % 254 ) { RequiredSectors++; }

	/* If the file count is an exact multiple of 8, then we'll need another sector */
	if ( ( pDirectory->Files.size() % 8 ) == 0 ) { RequiredSectors++; }

	if ( pBAM->CountFreeSectors() < RequiredSectors )
	{
		/* Disk full! */
		return NUTSError( 0x42, L"Disk full" );
	}

	/* Write the file. */
	DWORD BytesToGo = file.Length;
	BYTE  Buffer[ 256 ];
	TSLink Loc = pBAM->GetFreeTS();

	file.Attributes[ 0 ] = SectorForLink( Loc.Track, Loc.Sector );

	store.Seek( 0 );

	while ( BytesToGo > 0 )
	{
		DWORD BytesWrite = BytesToGo;

		if ( BytesWrite > 254 )
		{
			BytesWrite = 254;
		}

		DWORD AbsSec = SectorForLink( Loc.Track, Loc.Sector );

		if ( BytesToGo > 254 )
		{
			Loc = pBAM->GetFreeTS();

			Buffer[ 0 ] = Loc.Track;
			Buffer[ 1 ] = Loc.Sector;
		}
		else
		{
			Buffer[ 0 ] = 0;
			Buffer[ 1 ] = (BYTE) BytesToGo & 0xFF;
		}

		store.Read( &Buffer[ 2 ], BytesWrite );

		if ( pSource->WriteSector( AbsSec, Buffer, 256 ) != DS_SUCCESS )
		{
			return -1;
		}
	
		BytesToGo -= BytesWrite;
	}

	pDirectory->Files.push_back( file );

	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	return pDirectory->ReadDirectory();
}

BYTE *D64FileSystem::DescribeFile(DWORD FileIndex) {
	static BYTE status[40];

	if ((FileIndex < 0) || ((unsigned) FileIndex > pDirectory->Files.size()))
	{
		status[0] = 0;

		return status;
	}

	NativeFile	*pFile	= &pDirectory->Files[FileIndex];

	rsprintf( status, "%d BYTES, %s, %s",
		(DWORD) pDirectory->Files[FileIndex].Length,
		( pDirectory->Files[FileIndex].Attributes[ 2 ] )?"LOCKED":"NOT LOCKED",
		( pDirectory->Files[FileIndex].Attributes[ 3 ] )?"CLOSED":"NOT CLOSED (SPLAT)"
	);

	return status;
}

BYTE *D64FileSystem::GetStatusString( int FileIndex, int SelectedItems ) {
	static BYTE status[ 128 ];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "%d FILES", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d FILES SELECTED", SelectedItems );
	}
	else 
	{
		rsprintf(
			status, "%s.%s - %d BYTES, %s, %s",
			pDirectory->Files[FileIndex].Filename,
			pDirectory->Files[FileIndex].Extension,
			(DWORD) pDirectory->Files[FileIndex].Length,
			( pDirectory->Files[FileIndex].Attributes[ 2 ] )?"LOCKED":"NOT LOCKED",
			( pDirectory->Files[FileIndex].Attributes[ 3 ] )?"CLOSED":"NOT CLOSED (SPLAT)"
		);
	}

	return status;
}

FSHint D64FileSystem::Offer( BYTE *Extension )
{
	//	D64s have a somewhat "optimized" layout that doesn't give away any reliable markers that the file
	//	is indeed a D64. So we'll just have to check the extension and see what happens.

	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FS_Null;

	if ( Extension != nullptr )
	{
		if ( _strnicmp( (char *) Extension, "D64", 3 ) == 0 )
		{
			hint.Confidence = 20;
			hint.FSID       = FSID_D64;
		}
	}

	return hint;
}


BYTE *D64FileSystem::GetTitleString( NativeFile *pFile )
{
	static BYTE title[512];
	
	if ( pFile == nullptr )
	{
		rsprintf( title, "D64::" );
		rstrncat( &title[ 5 ], pBAM->DiskName, 16 );
	}
	else
	{
		rsprintf( title, "%s.%s", pFile->Filename, pFile->Extension );
	}

	return title;
}


AttrDescriptors D64FileSystem::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Start sector. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"Start sector";
	Attrs.push_back( Attr );

	/* Locked */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile;
	Attr.Name  = L"Locked";
	Attrs.push_back( Attr );

	/* Closed */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile;
	Attr.Name  = L"Closed";
	Attrs.push_back( Attr );

	/* File type */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect | AttrWarning | AttrFile;
	Attr.Name  = L"File type";

	AttrOption opt;

	opt.Name            = L"Deleted (DEL)";
	opt.EquivalentValue = 0x00000000;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Sequential (SEQ)";
	opt.EquivalentValue = 0x00000001;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Program (PRG)";
	opt.EquivalentValue = 0x00000002;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"User (USR)";
	opt.EquivalentValue = 0x00000003;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Relative (REL)";
	opt.EquivalentValue = 0x00000004;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	Attrs.push_back( Attr );
		
	/* REL Side-Sector */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"REL Side Sector";
	Attrs.push_back( Attr );

	/* REL Length */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"REL Length";
	Attrs.push_back( Attr );

	return Attrs;
}

int D64FileSystem::MakeASCIIFilename( NativeFile *pFile )
{
	MakeASCII( pFile );

	return 0;
}

int D64FileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	ResetEvent( hCancelFree );

	DWORD NumBlocks = 0;

	static FSSpace Map;

	for ( BYTE T=1; T<=35; T++ )
	{
		NumBlocks += spt[ T ];
	}

	Map.Capacity  = NumBlocks * 256;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = Map.Capacity;

	double BlockRatio = ( (double) NumBlocks / (double) TotalBlocks );

	if ( BlockRatio < 1.0 ) { BlockRatio = 1.0 ; }
	
	memset( pBlockMap, BlockAbsent, TotalBlocks );
	memset( pBlockMap, BlockUsed, (DWORD) ( NumBlocks / BlockRatio ) );

	DWORD BlkNum;

	for ( BYTE T=1; T<=35; T++ )
	{
		for ( BYTE S=0; S<spt[ T ]; S++ )
		{
			if ( WaitForSingleObject( hCancelFree, 0 ) == WAIT_OBJECT_0 )
			{
				return 0;
			}

			if ( pBAM->IsFreeBlock( T, S ) )
			{
				Map.UsedBytes -= 256;

				DWORD Blk = SectorForLink( T, S );

				BlkNum = (DWORD) ( (double) Blk / BlockRatio );

				if ( ( BlkNum < TotalBlocks ) && ( pBlockMap[ BlkNum ] != BlockFixed ) )
				{
					if ( T == 18 )
					{
						if ( S < 2 )
						{
							pBlockMap[ BlkNum ] = BlockFixed;
						}
						else
						{
							pBlockMap[ BlkNum ] = BlockFree;
						}
					}
					else
					{
						pBlockMap[ BlkNum ] = BlockFree;
					}
				}
			}
		}
	}

	SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

	return 0;
}

int D64FileSystem::ReplaceFile(NativeFile *pFile, CTempFile &store)
{
	/* Rather than the traditional method of Delete then Write Fresh, and because D64 uses
	   allocated sectors, we'll actually just overwrite the file's existing sectors with
	   the new data. If the file ends up shorter, we'll free the excess. If we need
	   extra sectors, we'll grab them from the BAM.
	*/
	NativeFile file = *pFile;

	if ( file.EncodingID != ENCODING_PETSCII )
	{
		IncomingASCII( &file );
	}

	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( &*iFile, &file ) )
		{
			/* Got it */
			DWORD BytesToGo = store.Ext();

			DWORD CurrentSectors = iFile->Length / 254;

			if ( iFile->Length % 254 ) { CurrentSectors++; }

			DWORD NewSectors = BytesToGo / 254;

			if ( BytesToGo % 254 ) { NewSectors++; }

			if ( NewSectors > CurrentSectors )
			{
				if ( pBAM->CountFreeSectors() < ( NewSectors - CurrentSectors ) )
				{
					return NUTSError( 0x42, L"Disk full" );
				}
			}

			/* There's room. Let's do this. */
			store.Seek( 0 );

			TSLink Loc = LinkForSector( iFile->Attributes[ 0 ] );

			BYTE Buffer[ 256 ];

			while ( Loc.Track != 0 )
			{
				DWORD BytesWrite = BytesToGo;

				if ( BytesWrite > 254 ) { BytesWrite = 254; }

				pSource->ReadSector( SectorForLink( Loc.Track, Loc.Sector ), Buffer, 256 );

				BYTE Track  = Buffer[ 0 ];
				BYTE Sector = Buffer[ 1 ];

				if ( BytesToGo > 0 )
				{
					store.Read( &Buffer[ 2 ], BytesWrite );

					BytesToGo -= BytesWrite;
				}

				if ( ( Track == 0 ) && ( BytesToGo <= 254 ) ) 
				{
					/* Same number of sectors */
					if ( BytesWrite == 0 )
					{
						/* Free this */
						pBAM->ReleaseSector( Track, Sector );
					}

					Buffer[ 0 ] = 0;
					Buffer[ 1 ] = ( BYTE ) ( BytesWrite & 0xFF );
				}
				else if ( ( Track == 0 ) && ( BytesToGo > 0 ) )
				{
					/* More sectors needed! */
					TSLink TS = pBAM->GetFreeTS();

					Buffer[ 0 ] = TS.Track;
					Buffer[ 1 ] = TS.Sector;

					Track  = TS.Track;
					Sector = TS.Sector;
				}
				else if ( ( Track != 0 ) && ( BytesToGo == 0 ) )
				{
					/* Need to realse old sectors */
					pBAM->ReleaseSector( Track, Sector );

					Buffer[ 0 ] = 0;
					Buffer[ 1 ] = (BYTE) ( BytesWrite & 0xFF );
				}

				pSource->WriteSector( SectorForLink( Loc.Track, Loc.Sector ), Buffer, 256 );

				Loc.Track  = Track;
				Loc.Sector = Sector;
			}

			iFile->Length = (WORD) ( store.Ext() & 0xFFFF );

			if ( pDirectory->WriteDirectory() != DS_SUCCESS )
			{
				return -1;
			}

			if ( pDirectory->ReadDirectory() != DS_SUCCESS )
			{
				return -1;
			}

			return 0;
		}
	}

	return 0;}

int D64FileSystem::DeleteFile( NativeFile *pFile, int FileOp )
{
	/* This is an easy one. Find the object and get it's sector link, then remove sectors in a chain. */

	NativeFile file = *pFile;

	if ( file.EncodingID != ENCODING_PETSCII )
	{
		IncomingASCII( &file );
	}

	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( &*iFile, &file ) )
		{
			/* Got it */
			TSLink Loc = LinkForSector( iFile->Attributes[ 0 ] );

			BYTE Buffer[ 256 ];

			while ( Loc.Track != 0 )
			{
				pSource->ReadSector( SectorForLink( Loc.Track, Loc.Sector ), Buffer, 256 );

				BYTE Track  = Buffer[ 0 ];
				BYTE Sector = Buffer[ 1 ];

				pBAM->ReleaseSector( Track, Sector );

				Loc.Track  = Track;
				Loc.Sector = Sector;
			}

			pDirectory->Files.erase( iFile );

			if ( pDirectory->WriteDirectory() != DS_SUCCESS )
			{
				return -1;
			}

			if ( pDirectory->ReadDirectory() != DS_SUCCESS )
			{
				return -1;
			}

			return 0;
		}
	}

	return 0;
}
