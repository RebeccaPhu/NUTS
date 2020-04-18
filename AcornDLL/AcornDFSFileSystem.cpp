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
	return 0;
}

int	AcornDFSFileSystem::CreateDirectory( BYTE *Filename ) {

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

BYTE *AcornDFSFileSystem::GetStatusString(int FileIndex) {
	static BYTE status[128];

	NativeFile	*pFile	= &pDirectory->Files[FileIndex];

	rsprintf( status, "%s | [%s] - %0X bytes - Load: &%08X Exec: &%08X\n",
		pFile->Filename, (pFile->AttrLocked)?"L":"-",
		pFile->Length, pFile->LoadAddr, pFile->ExecAddr);
		
	return status;
}

FSHint AcornDFSFileSystem::Offer( BYTE *Extension )
{
	//	D64s have a somewhat "optimized" layout that doesn't give away any reliable markers that the file
	//	is indeed a D64. So we'll just have to check the extension and see what happens.

	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FS_Null;

	if ( Extension != nullptr )
	{
		if ( _strnicmp( (char *) Extension, "SSD", 3 ) == 0 )
		{
			hint.Confidence = 20;

			if ( pSource->PhysicalDiskSize <= ( 10 * 40 * 256 ) )
				hint.FSID = FSID_DFS_40;
			else if ( pSource->PhysicalDiskSize <= ( 10 * 80 * 256 ) )
				hint.FSID = FSID_DFS_80;
		}
	}

	if ( ( hint.FSID != FSID_DFS_40 ) && ( hint.FSID != FSID_DFS_80 ) )
	{
		BYTE SectorBuf[ 512 ];

		pSource->ReadSector(0, SectorBuf, 512);

		DWORD sectors = ((SectorBuf[256 + 6 + 0] & 3) << 8) | SectorBuf[256 + 6 + 1];

		if ((sectors == 400) || (sectors == 800))
		{
			hint.Confidence = 5;

			if ( sectors == 400 )
				hint.FSID = FSID_DFS_40;
			else
				hint.FSID = FSID_DFS_80;
		}
	}

	return hint;
}

int AcornDFSFileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	/* DFS doesn't have a free space map, but rather the directory is considered
	   to be an inverse of one. Since there are no subdirectories (and thus no
	   tree to traverse) and only 31 files max per disk, the table can be parsed
	   easily to subtract occupied sectors from a transiet free space map,
	   resulting in a temporary free space map. DFS then uses this to locate
	   a space to save a file. To construct the block map, we can do the same
	   (using the sector count) and subtract each file's allocation from the map. */

	BYTE Sector[ 512 ];

	static FSSpace Map;

	pSource->ReadSector( 0, Sector, 512 );

	DWORD NumBlocks = ((Sector[256 + 6 + 0] & 3) << 8) | Sector[256 + 6 + 1];

	Map.Capacity  = NumBlocks * 256;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = Map.Capacity;

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

			Map.UsedBytes -= UBlocks * 256;

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

