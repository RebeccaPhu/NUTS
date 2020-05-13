#include "StdAfx.h"
#include "AcornDFSFileSystem.h"
#include "../NUTS/TempFile.h"

int	AcornDFSFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	NativeFile *file = &pDirectory->Files[FileID];

	int	Sectors	= ( file->Length & 0xffffff00 ) >> 8;

	if ( file->Length & 0xFF )
		Sectors++;

	unsigned char Sector[256];

	DWORD offset   = 0;
	DWORD fileSize = (DWORD) file->Length;

	while (Sectors) {
		pSource->ReadSector( file->Attributes[ 0 ] + offset, Sector, 256);

		if ( fileSize > 256U )
		{
			store.Write( Sector, 256U );
		}
		else
		{
			store.Write( Sector, fileSize );
		}

		offset++;
		Sectors--;
		fileSize -= 256;
	}

	return 0;
}

int	AcornDFSFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	if ( ( pFile->EncodingID != ENCODING_ASCII ) && ( pFile->EncodingID != ENCODING_ACORN ) )
	{
		return FILEOP_NEEDS_ASCII;
	}

	if ( pDirectory->Files.size() >= 31 )
	{
		return NUTSError( 0x80, L"Directory Full" );
	}

	/* Check we don't already have this */
	NativeFileIterator iFile;

	for ( iFile = pDFSDirectory->RealFiles.begin(); iFile != pDFSDirectory->RealFiles.end(); iFile++ )
	{
		if ( rstrnicmp( pFile->Filename, iFile->Filename, 7 ) ) /* DFS limits files to 7 chars */
		{
			return FILEOP_EXISTS;
		}
	}

	/* No DFS disk has more than 800 sectors */
	BYTE Occupied[ 800 ];

	ZeroMemory( Occupied, 800 );

	Occupied[ 0 ] = 0xFF;
	Occupied[ 1 ] = 0xFF;

	/* Build up a map of occupied sectors */
	for ( iFile = pDFSDirectory->RealFiles.begin(); iFile != pDFSDirectory->RealFiles.end(); iFile++ )
	{
		DWORD NSectors = iFile->Length / 256;

		if ( iFile->Length % 256 ) { NSectors++; }

		DWORD Sector   = iFile->SSector;

		for ( BYTE n = 0; n < NSectors; n++ )
		{
			if ( ( Sector + n ) < 800 )
			{
				Occupied[ Sector + n ] = 0xFF;
			}
		}
	}

	/* Get the size of the disk. We can't store a file bigger than the capacity */
	BYTE SectorBuf[ 512 ];

	/* Now find a space in the map big enough for the incoming file */
	DWORD NSectors = pFile->Length / 256;

	if ( pFile->Length % 256 ) { NSectors++; }

	if ( NSectors > ( pDFSDirectory->NumSectors - 2 ) )
	{
		/* 2 sectors for the catalogue */
		return NUTSError( 0x35, L"File too large for this image" );
	}

	DWORD Sector = 0;

	for ( WORD n = 0; n < (pDFSDirectory->NumSectors - NSectors ); n++ )
	{
		Sector = n;

		for ( WORD s = 0; s < NSectors; s++ )
		{
			if ( Occupied[ n + s ] != 0 )
			{
				Sector = 0;

				break;
			}
		}

		if ( Sector != 0 )
		{
			/* Found one */
			break;
		}
	}

	if ( Sector == 0 )
	{
		return NUTSError( 0x35, L"Disk full" );
	}

	/* Sector now holds the starting point to write. Now fill in a NativeFile. */
	NativeFile file;

	if ( pFile->FSFileType == FT_ACORN )
	{
		file         = *pFile;
		file.SSector = Sector;
	}
	else
	{
		/* File being imported. Set a few sensibilities. */
		file            = *pFile;
		file.AttrLocked = 0xFFFFFFFF;
		file.LoadAddr   = 0xFFFFFFFF;
		file.ExecAddr   = 0xFFFFFFFF;
		file.Flags      = 0; /* No extensions here, ta */
	}

	file.AttrPrefix = pDFSDirectory->CurrentDir;

	/* Write the sectors out */
	DWORD CSector = Sector;
	DWORD Bytes   = pFile->Length;

	while ( Bytes > 0 )
	{
		DWORD BytesToGo = Bytes;

		if ( BytesToGo > 256 )
		{
			BytesToGo = 256;
		}

		store.Read( SectorBuf, BytesToGo );

		pSource->WriteSector( CSector, SectorBuf, BytesToGo );

		Bytes -= BytesToGo;
		CSector++;
	}

	/* Write out the new directory */
	pDFSDirectory->RealFiles.push_back( file );

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

int	AcornDFSFileSystem::CreateDirectory( BYTE *Filename, bool EnterAfter ) {

	return -1;
}

BYTE *AcornDFSFileSystem::DescribeFile(DWORD FileIndex) {
	static BYTE status[8192];

	NativeFile	*pFile	= &pDirectory->Files[FileIndex];

	sprintf_s( (char *) status, 8192, "[%s] %06X bytes, &%08X/&%08X",
		(pFile->AttrLocked)?"L":"-",
		pFile->Length, pFile->LoadAddr, pFile->ExecAddr);
		
	return status;
}

BYTE *AcornDFSFileSystem::GetStatusString( int FileIndex, int SelectedItems ) {
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

		rsprintf( status,
			"%s | [%s] - %0X bytes - Load: &%08X Exec: &%08X",
			pFile->Filename, (pFile->AttrLocked)?"L":"-",
			(DWORD) pFile->Length, pFile->LoadAddr, pFile->ExecAddr
		);
	}
		
	return status;
}

FSHint AcornDFSFileSystem::Offer( BYTE *Extension )
{
	//	D64s have a somewhat "optimized" layout that doesn't give away any reliable markers that the file
	//	is indeed a D64. So we'll just have to check the extension and see what happens.

	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FSID;

	if ( Extension != nullptr )
	{
		if ( _strnicmp( (char *) Extension, "SSD", 3 ) == 0 )
		{
			hint.Confidence = 20;
		}

		if ( _strnicmp( (char *) Extension, "IMG", 3 ) == 0 )
		{
			hint.Confidence = 10;
		}
	}

	BYTE SectorBuf[ 512 ];

	if ( pSource->ReadSector(0, SectorBuf, 512) != DS_SUCCESS )
	{
		return hint;
	}

	DWORD sectors = ((SectorBuf[256 + 6 + 0] & 3) << 8) | SectorBuf[256 + 6 + 1];

	if ( ( sectors == 400 ) && ( FSID == FSID_DFS_40 ) )
	{
		hint.Confidence += 20;
	}

	if ( ( sectors == 800 ) && ( FSID == FSID_DFS_80 ) )
	{
		hint.Confidence += 20;
	}

	return hint;
}

int AcornDFSFileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	/* DFS doesn't have a free space map, but rather the directory is considered
	   to be an inverse of one. Since there are no subdirectories (and thus no
	   tree to traverse) and only 31 files max per disk, the table can be parsed
	   easily to subtract occupied sectors from a transient free space map,
	   resulting in a temporary free space map. DFS then uses this to locate
	   a space to save a file. To construct the block map, we can do the same
	   (using the sector count) and subtract each file's allocation from the map. */

	BYTE Sector[ 512 ];

	static FSSpace Map;

	pSource->ReadSector( 0, Sector, 512 );

	DWORD NumBlocks = ((Sector[256 + 6 + 0] & 3) << 8) | Sector[256 + 6 + 1];

	Map.Capacity  = NumBlocks * 256;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = 512;

	std::vector<NativeFile>::iterator iFile;

	double BlockRatio = ( (double) NumBlocks / (double) TotalBlocks );

	if ( BlockRatio < 1.0 ) { BlockRatio = 1.0 ; }
	
	memset( pBlockMap, BlockAbsent, TotalBlocks );
	memset( pBlockMap, BlockFree, (DWORD) ( NumBlocks / BlockRatio ) );

	DWORD BlkNum;

	for ( DWORD FixedBlk=0; FixedBlk != 2; FixedBlk++ )
	{
		BlkNum = (DWORD) ( (double) FixedBlk / BlockRatio );
	
		pBlockMap[ BlkNum ] = (BYTE) BlockFixed;
	}

	if ( pDirectory != nullptr )
	{
		for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
		{
			DWORD UBlocks = (DWORD) ( iFile->Length / 256 );
			
			if ( iFile->Length % 256 )
			{
				UBlocks++;
			}

			Map.UsedBytes += UBlocks * 256;

			for ( DWORD Blk = iFile->SSector; Blk != iFile->SSector + UBlocks; Blk++ )
			{
				BlkNum = (DWORD) ( (double) Blk / BlockRatio );

				if ( pBlockMap[ BlkNum ] != BlockFixed )
				{
					pBlockMap[ BlkNum ] = BlockUsed;
				}
			}

			SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );
		}
	}

	SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

	return 0;
}


int AcornDFSFileSystem::ChangeDirectory( DWORD FileID )
{
	if ( FileID > pDirectory->Files.size() )
	{
		return -1;
	}

	NativeFile *file = &pDirectory->Files[ FileID ];

	pDFSDirectory->CurrentDir = file->Filename[ 0 ];

	pDirectory->ReadDirectory();

	return 0;
}

bool AcornDFSFileSystem::IsRoot() {
	if ( pDFSDirectory->CurrentDir == '$' )
		return true;

	return false;
}

int	AcornDFSFileSystem::Parent() {
	
	if ( IsRoot() )
	{
		return -1;
	}

	pDFSDirectory->CurrentDir = '$';

	pDirectory->ReadDirectory();

	return 0;
}


AttrDescriptors AcornDFSFileSystem::GetFSAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;
	
	Attrs.clear();

	AttrDesc Attr;

	/* Disc Title */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrString | AttrEnabled;
	Attr.Name  = L"Disc Title";

	Attr.MaxStringLength = 12;
	Attr.pStringVal      = rstrndup( pDFSDirectory->DiscTitle, 12 );

	Attrs.push_back( Attr );

	/* Boot Option */
	/* File type */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect;
	Attr.Name  = L"Boot Option";

	Attr.StartingValue  = pDFSDirectory->Option;

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

AttrDescriptors AcornDFSFileSystem::GetAttributeDescriptions( void )
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

	/* Locked */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"Directory";
	Attrs.push_back( Attr );

	/* Load address. Hex. */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;;
	Attr.Name  = L"Load address";
	Attrs.push_back( Attr );

	/* Exec address. Hex. */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;;
	Attr.Name  = L"Execute address";
	Attrs.push_back( Attr );

	return Attrs;
}

int AcornDFSFileSystem::Format_Process( FormatType FT, HWND hWnd ) {

	int Stages = 1;

	DWORD NumSectors = 0;

	if ( FSID == FSID_DFS_40 )
	{
		NumSectors = 40 * 10;
	}

	if ( FSID == FSID_DFS_80 )
	{
		NumSectors = 80 * 10;
	}

	if ( NumSectors == 0 )
	{
		return NUTSError( 0x36, L"Invalid format specified" );
	}

	if ( FT = FormatType_Full )
	{
		Stages++;

		BYTE SectorBuf[ 256 ];

		ZeroMemory( SectorBuf, 256 );

		for ( WORD n=0; n < NumSectors; n++ )
		{
			if ( WaitForSingleObject( hCancelFormat, 0 ) == WAIT_OBJECT_0 )
			{
				return 0;
			}

			pSource->WriteSector( n, SectorBuf, 256 );

			PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 0, Stages, n, NumSectors, false ), 0 );
		}
	}

	pDFSDirectory->RealFiles.clear();
	pDFSDirectory->NumSectors = NumSectors;
	pDFSDirectory->MasterSeq  = 0;
	pDFSDirectory->Option     = 0;

	rstrncpy( pDFSDirectory->DiscTitle, (BYTE *) "            ", 12 );

	pDirectory->WriteDirectory();

	PostMessage( hWnd, WM_FORMATPROGRESS, 100, 0);

	return 0;
}


int AcornDFSFileSystem::DeleteFile( NativeFile *pFile, int FileOp )
{
	/* This is easy, we literally just removing the offending file from the directory, and save */
	NativeFileIterator iFile;

	for ( iFile = pDFSDirectory->RealFiles.begin(); iFile != pDFSDirectory->RealFiles.end(); iFile++ )
	{
		if ( iFile->SSector == pFile->SSector )
		{
			pDFSDirectory->RealFiles.erase( iFile );

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

	return NUTSError( 0x37, L"File not found" );
}