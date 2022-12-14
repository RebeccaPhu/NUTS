#include "StdAfx.h"
#include "D64FileSystem.h"
#include "CBMFunctions.h"
#include "OpenCBMPlugin.h"

#include "../NUTS/NUTSError.h"
#include "resource.h"

#include "CBMDLL.h"

int	D64FileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	//	See notes in D64Directory regarding file trapesing.

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	int	foffset	= 0;
	int	logsect	= pFile->Attributes[ 0 ];

	unsigned char sector[256];

	D64Directory *pd = (D64Directory *) pDirectory;

	while (logsect != -1) {
		TSLink ts = LinkForSector( logsect );

		pSource->ReadSectorCHS( 0, ts.Track, ts.Sector, sector );

		if (sector[0] != 0) {
			store.Write( &sector[2], 254 );

			logsect	= SectorForLink(sector[0], sector[1]);

			foffset	+= 254;
		} else {
			store.Write( &sector[2], (DWORD) sector[1] );

			logsect	= -1;
		}
	}

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

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

	if ( file.FSFileType == FT_CBM_TAPE )
	{
		file.Attributes[ 2 ] = 0x00000000;
		file.Attributes[ 3 ] = 0xFFFFFFFF;

		file.Attributes[ 1 ] = pFile->Attributes[ 3 ];
	}
	else if ( file.FSFileType != FT_C64 )
	{
		file.Attributes[ 2 ] = 0xFFFFFFFF;
		file.Attributes[ 3 ] = 0x00000000;

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
	DWORD RequiredSectors = DWORD( file.Length ) / 254; // 2 bytes of link.

	if ( file.Length % 254 ) { RequiredSectors++; }

	/* If the file count is an exact multiple of 8, then we'll need another sector */
	if ( ( pDirectory->Files.size() % 8 ) == 0 ) { RequiredSectors++; }

	if ( pBAM->CountFreeSectors() < RequiredSectors )
	{
		/* Disk full! */
		return NUTSError( 0x42, L"Disk full" );
	}

	/* Write the file. */
	DWORD BytesToGo = (DWORD) file.Length;
	BYTE  Buffer[ 256 ];
	TSLink Loc = pBAM->GetFreeTS();

	file.Attributes[ 0 ] = SectorForLink( Loc.Track, Loc.Sector );

	store.Seek( 0 );

	bool FirstBlock = true;

	while ( BytesToGo > 0 )
	{
		DWORD BytesWrite = min( 254, BytesToGo );

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

		FirstBlock = false;

		if ( pSource->WriteSectorCHS( 0, Loc.Track, Loc.Sector, Buffer ) != DS_SUCCESS )
		{
			if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

			return -1;
		}
	
		BytesToGo -= BytesWrite;
	}

	pDirectory->Files.push_back( file );

	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	int r = pDirectory->ReadDirectory();

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	return r;
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
			status, "%s.%s%s%s - %d BYTES, %s, %s",
			(BYTE *) pDirectory->Files[FileIndex].Filename,
			(pDirectory->Files[FileIndex].Attributes[ 2 ])?"":">",
			(pDirectory->Files[FileIndex].Attributes[ 3 ])?"":"*",
			(BYTE *) pDirectory->Files[FileIndex].Extension,
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


BYTE *D64FileSystem::GetTitleString( NativeFile *pFile, DWORD Flags )
{
	static BYTE title[512];
	
	if ( pFile == nullptr )
	{
		rsprintf( title, "D64::" );

		if ( Drive != 0 )
		{
			rsprintf( title, "OPENCBM/%d::", Drive );
		}

		rstrncat( &title[ 5 ], pBAM->DiskName, 32 );
	}
	else
	{
		rsprintf( title, "%s.%s", pFile->Filename, pFile->Extension );

		if ( Flags & TF_Titlebar )
		{
			if ( !(Flags & TF_Final ) )
			{
				// ">" happens to be in the same place in both PETSCIIs and ASCII.
				rsprintf( title, "%s.%s > ", (BYTE *) pFile->Filename, (BYTE *) pFile->Extension );
			}
		}
	}

	return title;
}


AttrDescriptors D64FileSystem::GetAttributeDescriptions( NativeFile *pFile )
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
					pBlockMap[ BlkNum ] = BlockFree;
				}
			}
			else
			{
				if ( T == 18 )
				{
					if ( S < 2 )
					{
						DWORD Blk = SectorForLink( T, S );

						BlkNum = (DWORD) ( (double) Blk / BlockRatio );

						pBlockMap[ BlkNum ] = BlockFixed;
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
			DWORD BytesToGo = (DWORD) store.Ext();

			DWORD CurrentSectors = DWORD( iFile->Length ) / 254;

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

				pSource->ReadSectorCHS( 0, Loc.Track, Loc.Sector, Buffer );

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

				pSource->WriteSectorCHS( 0, Loc.Track, Loc.Sector, Buffer );

				Loc.Track  = Track;
				Loc.Sector = Sector;
			}

			iFile->Length = (WORD) ( store.Ext() & 0xFFFF );

			if ( pDirectory->WriteDirectory() != DS_SUCCESS )
			{
				if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

				return -1;
			}

			if ( pDirectory->ReadDirectory() != DS_SUCCESS )
			{
				if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

				return -1;
			}

			return 0;
		}
	}

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	return 0;
}

int D64FileSystem::DeleteFile( DWORD FileID )
{
	/* This is an easy one. Find the object and get it's sector link, then remove sectors in a chain. */

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	TSLink Loc = LinkForSector( pFile->Attributes[ 0 ] );

	BYTE Buffer[ 256 ];

	while ( Loc.Track != 0 )
	{
		pSource->ReadSectorCHS( 0, Loc.Track, Loc.Sector, Buffer );

		BYTE Track  = Buffer[ 0 ];
		BYTE Sector = Buffer[ 1 ];

		pBAM->ReleaseSector( Track, Sector );

		Loc.Track  = Track;
		Loc.Sector = Sector;
	}

	pDirectory->Files.erase( pDirectory->Files.begin() + FileID );

	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

		return -1;
	}

	if ( pDirectory->ReadDirectory() != DS_SUCCESS )
	{
		if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

		return -1;
	}

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	return 0;
}

AttrDescriptors D64FileSystem::GetFSAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Disc Title */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrString | AttrEnabled;
	Attr.Name  = L"Disk Name";

	Attr.MaxStringLength = 16;
	Attr.pStringVal      = rstrndup( pBAM->DiskName, 16 );

	Attrs.push_back( Attr );

	/* Disc ID */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrString | AttrEnabled;
	Attr.Name  = L"Disk ID";

	Attr.MaxStringLength = 2;
	Attr.pStringVal      = rstrndup( pBAM->DiskName, 2 );

	Attrs.push_back( Attr );

	/* Boot Option */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex;
	Attr.Name  = L"DOS Version";

	Attr.StartingValue = pBAM->DosVer;

	Attrs.push_back( Attr );

	/* Boot Option */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrEnabled | AttrString;
	Attr.Name  = L"DOS Type";

	Attr.MaxStringLength = 2;
	Attr.pStringVal = rstrndup( pBAM->DOSID, 2 );

	Attrs.push_back( Attr );

	return Attrs;
}

int D64FileSystem::SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal )
{
	switch ( PropID )
	{
	case 0:
		rstrncpy( pBAM->DiskName, pNewVal, 16 );
		break;

	case 1:
		rstrncpy( pBAM->DiskID, pNewVal, 2 );
		break;

	case 2:
		pBAM->DosVer = (BYTE) ( NewVal & 0xFF );
		break;

	case 3:
		rstrncpy( pBAM->DOSID, pNewVal, 2 );
		break;
	}

	int r = pBAM->WriteBAM();

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	return r;
}

int D64FileSystem::Format_Process( DWORD FT, HWND hWnd )
{
	static WCHAR * const InitMsg  = L"Initialising";
	static WCHAR * const EraseMsg = L"Erasing";
	static WCHAR * const MapMsg   = L"Creating BAM";
	static WCHAR * const RootMsg  = L"Creating Root Directory";
	static WCHAR * const DoneMsg  = L"Complete";

	PostMessage( hWnd, WM_FORMATPROGRESS, 0, (LPARAM) InitMsg );

	DWORD Sectors = DWORD( pSource->PhysicalDiskSize / (QWORD) 256 );

	BYTE Buffer[ 256 ];

	if ( FT & FTF_LLF )
	{
		// Well... if a DataSource ever exists that accepts CBM_GCR....

		pSource->StartFormat();

		BYTE CSPT = 0;
		for ( BYTE t=1; t<=40; t++ )
		{
			if ( CSPT != spt[ t ] )
			{
				CSPT = spt[ t ];
			}

			TrackDefinition track;

			// Most of TrackDefinition is for FM/MFM.
			track.Density = CBM_GCR;

			for ( BYTE s=0; s<spt[ t ]; s++ )
			{
				SectorDefinition sector;

				// Again, most of SectorDefinition is FM/MFM.
				sector.SectorID     = s;
				sector.SectorLength = 0x01; // 256 Bytes
				sector.Side         = 0;
				sector.Track        = t;

				ZeroMemory( (void *) &sector.Data, 256 );

				track.Sectors.push_back( sector );
			}

			pSource->WriteTrack( track );
		}

		pSource->EndFormat();
	}

	if ( FT & FTF_Blank )
	{
		ZeroMemory( Buffer, 256 );

		WORD Secs = 0;

		for ( BYTE T=1; T<=35; T++ )
		{
			Secs += spt[ T ];
		}

		WORD DoneSecs = 0;

		for ( BYTE T=1; T<=35; T++ )
		{
			for ( DWORD S=0; S < spt[ T ]; S++ )
			{
				if ( WaitForSingleObject( hCancelFormat, 0 ) == WAIT_OBJECT_0 )
				{
					return 0;
				}

				if ( pSource->WriteSectorCHS( 0, T, S, Buffer ) != DS_SUCCESS )
				{
					return -1;
				}

				PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 0, 3, DoneSecs, Secs, false ), (LPARAM) EraseMsg );
			}
		}
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 1, 3, 0, 1, false ), (LPARAM) MapMsg );

	/* This clears out the BAM */
	for ( BYTE T=1; T<=35; T++ )
	{
		for ( DWORD S=0; S < spt[ T ]; S++ )
		{
			pBAM->ReleaseSector( (BYTE) T, (BYTE) S );
		}
	}

	/* Set some defaults for the other parts in the BAM */
	rstrncpy( pBAM->DiskName, (BYTE *) "Blank", 16 );
	rstrncpy( pBAM->DiskID, (BYTE *) "01", 2 );
	rstrncpy( pBAM->DOSID, (BYTE *) "2A", 2 );
	
	pBAM->DosVer = 0x41;

	/* Occupy the sector for the blank directory and the BAM itself */
	pBAM->OccupySector( 18, 0 );
	pBAM->OccupySector( 18, 1 );

	if ( pBAM->WriteBAM() != DS_SUCCESS )
	{
		if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

		return -1;
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 3, 4, 0, 1, false ), (LPARAM) RootMsg );

	ZeroMemory( Buffer, 256 );

	pSource->WriteSectorCHS( 0, 18, 1, Buffer );

	pDirectory->Files.clear();
	
	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

		return -1;
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 3, 4, 1, 1, true ), (LPARAM) DoneMsg );

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	return 0;
}

FSToolList D64FileSystem::GetToolsList( void )
{
	FSToolList Tools;

	FSTool tool;

	tool.ToolName = L"Validate Block Allocation Map";
	tool.ToolDesc = L"Wipes the Block Allocation Map and refills it from actual occupied sectors on the disk.";
	tool.ToolIcon = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_REPAIR ) );

	Tools.push_back( tool );

	return Tools;
}

int D64FileSystem::MarkChain( TSLink Loc )
{
	BYTE Buffer[ 256 ];

	TSLink ThisSec = Loc;

	while ( 1 )
	{
		if ( pSource->ReadSectorCHS( 0, ThisSec.Track, ThisSec.Sector, Buffer ) != DS_SUCCESS )
		{
			return -1;
		}

		if ( Buffer[ 0 ] == 0 )
		{
			if ( pBAM->WriteBAM() != DS_SUCCESS )
			{
				if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

				return -1;
			}

			if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

			return 0;
		}

		pBAM->OccupySector( ThisSec.Track, ThisSec.Sector );

		ThisSec.Track  = Buffer[ 0 ];
		ThisSec.Sector = Buffer[ 1 ];
	}

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	return -1;
}

int D64FileSystem::RunTool( BYTE ToolNum, HWND ProgressWnd )
{
	/* This is actually quite easy. We'll wipe the BAM, then go through each file and
	   traverse its sector chain, ticking off the occupied sectors as we go.

	   We'll start with 18/0 (BAM) and 18/1 (Directory). We'll also have to traverse
	   the directory sector chain too.
	*/
	
	::SendMessage( ProgressWnd, WM_FSTOOL_SETDESC, 0, (LPARAM) L"This tool will wipe the existing Block Allocation Map, and examine the sectors occupied by files, filling in the Map to resconstruct it." );
	::SendMessage( ProgressWnd, WM_FSTOOL_PROGLIMIT, 0, 100 );
	::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 0, 0, 0, 1, false), 0 );

	for ( BYTE T=1; T<=35; T++ )
	{
		for ( BYTE S=0; S<spt[ T ]; S++ )
		{
			pBAM->ReleaseSector( T, S );
		}
	}

	DWORD MaxSteps = (DWORD) pDirectory->Files.size() + 1;

	pBAM->OccupySector( 18, 0 );

	TSLink Loc = { 18, 1 };
	
	MarkChain( Loc );

	NativeFileIterator iFile;

	DWORD Step = 1;

	::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 0, 1, Step, MaxSteps, false), 0 );

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( WaitForSingleObject( hToolEvent, 0 ) == WAIT_OBJECT_0 )
		{
			if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

			return 0;
		}

		TSLink FileLoc = LinkForSector( iFile->Attributes[ 0 ] );

		MarkChain( FileLoc );

		Step++;

		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 0, 1, Step, MaxSteps, false), 0 );
	}

	::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, 100, 0 );

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	return 0;
}

WCHAR *D64FileSystem::Identify( DWORD FileID )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	const WCHAR *ftypes[6] = { L"Deleted File Entry", L"Sequential Access File", L"Program", L"User File", L"Relative Access File", L"State Snapshot" };

	return (WCHAR *) ftypes[ pFile->Attributes[ 1 ] ];
}

int D64FileSystem::Imaging( DataSource *pImagingSource, DataSource *pImagingTarget, HWND ProgressWnd )
{
	ResetEvent( hImageEvent );

	SetShape();

	AutoBuffer ByteBuf( 256 );

	for ( DWORD track=1; track<=40; track++ )
	{
		for ( DWORD sector=0; sector < spt[ track ]; sector++ )
		{
			if ( pImagingSource->ReadSectorCHS( 0, track, sector, (BYTE *) ByteBuf ) != DS_SUCCESS )
			{
				return -1;
			}

			if ( pImagingTarget->WriteSectorCHS( 0, track, sector, (BYTE *) ByteBuf ) != DS_SUCCESS )
			{
				return -1;
			}

			if ( WaitForSingleObject( hImageEvent, 0 ) == WAIT_OBJECT_0 )
			{
				return 0;
			}
		}

		std::wstring status = L"Copying track " + std::to_wstring( (QWORD) track ) + L" ...";

		::SendMessage( ProgressWnd, WM_FORMATPROGRESS, (WPARAM) Percent( 0, 1, track, 80, true ), (LPARAM)  status.c_str() );
	}

	return NUTS_SUCCESS;
}
