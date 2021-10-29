#include "StdAfx.h"
#include "MacintoshMFSFileSystem.h"

#include "AppleDLL.h"
#include "resource.h"

#include "..\NUTS\EncodingEdit.h"
#include <CommCtrl.h>

MacintoshMFSFileSystem::MacintoshMFSFileSystem( DataSource *pSource ) : FileSystem( pSource )
{
	pMacDirectory = new MacintoshMFSDirectory( pSource );

	pDirectory = pMacDirectory;

	pMacDirectory->pRecord = &VolRecord;

	VolRecord.Valid = false;

	pFAT = new MFSFAT( pSource );

	pFAT->pRecord = &VolRecord;

	Flags = FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Capacity | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Supports_Dirs | FSF_FixedSize | FSF_UseSectors | FSF_Exports_Sidecars;

	pMacDirectory->pFAT = pFAT;

	OverrideFork = nullptr;
	OverrideFinder = nullptr;
}


MacintoshMFSFileSystem::~MacintoshMFSFileSystem(void)
{
	delete pDirectory;
	delete pFAT;
}

void MacintoshMFSFileSystem::SetShape()
{
	DiskShape shape;

	shape.Heads            = 1;
	shape.InterleavedHeads = false;
	shape.LowestSector     = 0;
	shape.Sectors          = 10;
	shape.SectorSize       = 512;
	shape.TrackInterleave  = 0;
	shape.Tracks           = 80;

	pSource->SetDiskShape( shape );
}

void MacintoshMFSFileSystem::ReadVolumeRecord()
{
	// Volume record is always at sector 2.
	BYTE Buffer[ 512 ];

	VolRecord.Valid = false;

	if ( pSource->ReadSectorCHS( 0, 0, 2, Buffer ) != DS_SUCCESS )
	{
		return;
	}

	if ( ( Buffer[ 0 ] != 0xD2 ) || ( Buffer[ 1 ] != 0xD7 ) )
	{
		return ;
	}

	VolRecord.InitStamp      = BEDWORD( &Buffer[  2 ] );
	VolRecord.ModStamp       = BEDWORD( &Buffer[  6 ] );
	VolRecord.VolAttrs       = BEWORD(  &Buffer[ 10 ] );
	VolRecord.NumFiles       = BEWORD(  &Buffer[ 12 ] );
	VolRecord.FirstBlock     = BEWORD(  &Buffer[ 14 ] );
	VolRecord.DirBlockLen    = BEWORD(  &Buffer[ 16 ] );
	VolRecord.AllocBlocks    = BEWORD(  &Buffer[ 18 ] );
	VolRecord.AllocSize      = BEDWORD( &Buffer[ 20 ] );
	VolRecord.AllocBytes     = BEDWORD( &Buffer[ 24 ] );
	VolRecord.FirstAlloc     = BEWORD(  &Buffer[ 28 ] );
	VolRecord.NextUnusedFile = BEDWORD( &Buffer[ 30 ] );
	VolRecord.UnusuedAllocs  = BEWORD(  &Buffer[ 34 ] );

	VolRecord.VolumeName = BYTEString( &Buffer[ 37 ], max( Buffer[ 36 ], 28 ) );

	VolRecord.Valid = true;

	pFAT->ReadFAT();
}

int MacintoshMFSFileSystem::Init(void)
{
	if ( MYFSID == FSID_MFS_HD )
	{
		TopicIcon = FT_HardImage;
	}
	else
	{
		TopicIcon = FT_DiskImage;
	}

	SetShape();
	ReadVolumeRecord();

	if ( !VolRecord.Valid )
	{
		return NUTSError( 0x601, L"Failed to read volume record" );
	}

	if ( pDirectory->ReadDirectory() != NUTS_SUCCESS )
	{
		return -1;
	}

	return 0;
}

FSHint MacintoshMFSFileSystem::Offer( BYTE *Extension )
{
	SetShape();

	FSHint hint;

	hint.FSID       = FSID;
	hint.Confidence = 0;

	// Look for the two byte signature in the volume descriptor
	BYTE Buffer[ 0x200 ];

	pSource->ReadSectorCHS( 0, 0, 2, Buffer );

	if ( ( Buffer[ 0 ] == 0xD2 ) && ( Buffer[ 1 ] == 0xD7 ) )
	{
		// We got one - read a couple of values
		DWORD NumBlocks = BEWORD(  &Buffer[ 18 ] );
		DWORD BlockSize = BEDWORD( &Buffer[ 20 ] );

		DWORD Size = BlockSize * NumBlocks;

		if ( ( Size < 500000U ) && ( MYFSID == FSID_MFS ) )
		{
			hint.Confidence = 30;
		}
		else if ( ( Size > 300000U ) && ( MYFSID == FSID_MFS_HD ) )
		{
			hint.Confidence = 30;
		}
	}

	return hint;
}

BYTE *MacintoshMFSFileSystem::GetTitleString( NativeFile *pFile, DWORD Flags )
{
	static BYTE Title[ 300 ];

	rsprintf( Title, "MFS:" );

	rstrncpy( &Title[ 4 ], (BYTE *) VolRecord.VolumeName, VolRecord.VolumeName.length() );

	if ( pFile != nullptr )
	{
		WORD p = rstrnlen( Title, 280 );

		Title[ p ] = ':';

		rstrncpy( &Title[ p + 1 ], (BYTE *) pFile->Filename, pFile->Filename.length() );

		p++;
	}

	if ( Flags & TF_Titlebar )
	{
		if ( !( Flags & TF_Final ) )
		{
			WORD p = rstrnlen( Title, 280 );

			Title[ p + 0 ] = ' ';
			Title[ p + 1 ] = 0xC8;
			Title[ p + 2 ] = ' ';
			Title[ p + 3 ] = 0;
		}
	}

	return Title;
}

BYTE *MacintoshMFSFileSystem::GetStatusString( int FileIndex, int SelectedItems )
{
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
		NativeFile *pFile = &pDirectory->Files[FileIndex];

		rsprintf( status, "%s | %d bytes data, %d bytes resource",
			(BYTE *) pFile->Filename, pFile->Length, pFile->Attributes[ 15 ]
		);
	}
		
	return status;
}

std::vector<AttrDesc> MacintoshMFSFileSystem::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Start Alloc Unit, Data Fork */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile | AttrDir;
	Attr.Name  = L"Data Fork First Unit";
	Attrs.push_back( Attr );

	/* Start Alloc Unit, Resource Fork */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile | AttrDir;
	Attr.Name  = L"Res. Fork First Unit";
	Attrs.push_back( Attr );

	/* Resource Fork Length */
	Attr.Index = 15;
	Attr.Type  = AttrVisible | AttrNumeric | AttrDec | AttrFile | AttrDir;
	Attr.Name  = L"Res. Fork Length";
	Attrs.push_back( Attr );

	/* File Number */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrNumeric | AttrDec | AttrFile | AttrDir;
	Attr.Name  = L"File Number";
	Attrs.push_back( Attr );

	/* Locked */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"Locked";
	Attrs.push_back( Attr );

	/* Creation time */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrEnabled | AttrTime | AttrFile | AttrDir;
	Attr.Name  = L"Creation Time";
	Attrs.push_back( Attr );

	/* Creation time */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrEnabled | AttrTime | AttrFile | AttrDir;
	Attr.Name  = L"Modification Time";
	Attrs.push_back( Attr );

	return Attrs;
}


int MacintoshMFSFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	DWORD BytesToGo = pFile->Length;

	DWORD AllocBlockBytesToGo = VolRecord.AllocSize;

	store.Seek( 0 );

	BYTE Buffer[ 0x200 ];

	WORD  Alloc   = pFile->Attributes[ 2 ] - 2;
	DWORD DiscAdx = ( Alloc * VolRecord.AllocSize ) + ( VolRecord.FirstAlloc * 0x200 );

	if ( Alloc == 0x000 )
	{
		// Empty file
		store.SetExt( 0 );

		return 0;
	}

	DWORD SecNum  = DiscAdx / 0x200;

	while ( BytesToGo > 0 )
	{
		if ( pSource->ReadSectorLBA( SecNum, Buffer, 0x200 ) != DS_SUCCESS )
		{
			return -1;
		}

		AllocBlockBytesToGo -= 0x200;
		DiscAdx             += 0x200;

		store.Write( Buffer, min( BytesToGo, 0x200 ) );

		SecNum++;

		if ( AllocBlockBytesToGo == 0 )
		{
			AllocBlockBytesToGo = VolRecord.AllocSize;

			Alloc = pFAT->NextAllocBlock( Alloc + 2 ) - 2;

			DiscAdx = DiscAdx = ( Alloc * VolRecord.AllocSize ) + ( VolRecord.FirstAlloc * 0x200 );

			SecNum = DiscAdx / 0x200;
		}

		BytesToGo -= min( BytesToGo, 0x200 );
	}

	store.SetExt( pFile->Length );

	return 0;
}

int MacintoshMFSFileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	ResetEvent( hCancelFree );

	BYTE Sector[ 1024 ];

	static FSSpace Map;

	DWORD NumBlocks = VolRecord.AllocBlocks;

	Map.Capacity  = NumBlocks * VolRecord.AllocSize;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = Map.Capacity;

	double BlockRatio = ( (double) NumBlocks / (double) TotalBlocks );

	if ( BlockRatio < 1.0 ) { BlockRatio = 1.0 ; }
	
	memset( pBlockMap, BlockAbsent, TotalBlocks );
	memset( pBlockMap, BlockUsed, (DWORD) ( NumBlocks / BlockRatio ) );

	DWORD BlkNum;

	for ( DWORD FixedBlk=0; FixedBlk != 2; FixedBlk++ )
	{
		BlkNum = (DWORD) ( (double) FixedBlk / BlockRatio );
	
		pBlockMap[ BlkNum ] = (BYTE) BlockFixed;
	}

	if ( pFAT != nullptr )
	{
		for ( WORD AllocBlock = 2; AllocBlock < VolRecord.AllocBlocks; AllocBlock++ )
		{
			if ( WaitForSingleObject( hCancelFree, 0 ) == WAIT_OBJECT_0 )
			{
				return 0;
			}

			if ( pFAT->NextAllocBlock( AllocBlock ) == 0x000 )
			{
				Map.UsedBytes -= VolRecord.AllocSize;

				BlkNum = (DWORD) ( (double) AllocBlock / BlockRatio );

				if ( ( BlkNum < TotalBlocks ) && ( pBlockMap[ BlkNum ] != BlockFixed ) )
				{
					pBlockMap[ BlkNum ] = BlockFree;
				}

				if ( ( AllocBlock % 100 ) == 0 )
				{
					SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );
				}
			}
		}

		SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );
	}

	return 0;
}

AttrDescriptors MacintoshMFSFileSystem::GetFSAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;
	static BYTE VolName[ 256 ];

	Attrs.clear();

	AttrDesc Attr;

	/* Creation Date */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrEnabled | AttrTime;
	Attr.Name  = L"Creation Time";
	
	Attr.StartingValue = VolRecord.InitStamp - APPLE_TIME_OFFSET;

	Attrs.push_back( Attr );

	/* Creation Date */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrTime;
	Attr.Name  = L"Modification Time";

	Attr.StartingValue = VolRecord.ModStamp - APPLE_TIME_OFFSET;

	Attrs.push_back( Attr );

	/* Allocation Block Size */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrNumeric | AttrDec;
	Attr.Name  = L"Alloc. Block Size";

	Attr.StartingValue = VolRecord.AllocSize;

	Attrs.push_back( Attr );

	/* Clump Size */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrNumeric | AttrDec;
	Attr.Name  = L"Clump Size";

	Attr.StartingValue = VolRecord.AllocBytes;

	Attrs.push_back( Attr );

	/* Volume Name */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrEnabled | AttrString;
	Attr.Name  = L"Volume Name";

	Attr.MaxStringLength = 28;
	Attr.pStringVal      = rstrndup( VolRecord.VolumeName, 28 );

	Attrs.push_back( Attr );

	return Attrs;
}

int MacintoshMFSFileSystem::SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal )
{
	if ( PropID == 0 )
	{
		VolRecord.InitStamp = NewVal + APPLE_TIME_OFFSET;
	}

	if ( PropID == 1 )
	{
		VolRecord.ModStamp = NewVal + APPLE_TIME_OFFSET;
	}

	if ( PropID == 4 )
	{
		VolRecord.VolumeName = BYTEString( pNewVal, rstrnlen( pNewVal, 28 ) );
	}

	VolRecord.ModStamp = time(NULL) + APPLE_TIME_OFFSET;

	return WriteVolumeRecord();
}

int MacintoshMFSFileSystem::WriteVolumeRecord()
{
	// Volume record is always at sector 2. BEWARE! FAT is on the end!
	BYTE Buffer[ 512 ];

	if ( pSource->ReadSectorCHS( 0, 0, 2, Buffer ) != DS_SUCCESS )
	{
		return -1;
	}

	// Set the signature field
	Buffer[ 0 ] = 0xD2;
	Buffer[ 1 ] = 0xD7;

	WBEDWORD( &Buffer[  2 ], VolRecord.InitStamp );
	WBEDWORD( &Buffer[  6 ], VolRecord.ModStamp );
	WBEWORD(  &Buffer[ 10 ], VolRecord.VolAttrs );
	WBEWORD(  &Buffer[ 12 ], VolRecord.NumFiles );
	WBEWORD(  &Buffer[ 14 ], VolRecord.FirstBlock );
	WBEWORD(  &Buffer[ 16 ], VolRecord.DirBlockLen );
	WBEWORD(  &Buffer[ 18 ], VolRecord.AllocBlocks );
	WBEDWORD( &Buffer[ 20 ], VolRecord.AllocSize );
	WBEDWORD( &Buffer[ 24 ], VolRecord.AllocBytes );
	WBEWORD(  &Buffer[ 28 ], VolRecord.FirstAlloc );
	WBEDWORD( &Buffer[ 30 ], VolRecord.NextUnusedFile );
	WBEWORD(  &Buffer[ 34 ], VolRecord.UnusuedAllocs );

	Buffer[ 36 ] = rstrnlen( VolRecord.VolumeName, 28 );

	rstrncpy( &Buffer[ 37 ], VolRecord.VolumeName, 28 );

	if ( pSource->WriteSectorCHS( 0, 0, 2, Buffer ) != DS_SUCCESS )
	{
		return -1;
	}
}

FSToolList MacintoshMFSFileSystem::GetToolsList( void )
{
	FSToolList list;

	/* Repair Icon, Large Android Icons, Aha-Soft, CC Attribution 3.0 US */
	HBITMAP hRepair = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_REPAIR ) );

	FSTool ValidateTool = { hRepair, L"Validate", L"Fix discrepancies in file and allocation block counts." };

	list.push_back( ValidateTool );

	return list;
}


int MacintoshMFSFileSystem::RunTool( BYTE ToolNum, HWND ProgressWnd )
{
	/* Validate FS */

	std::string Log ="";

	::SendMessage( ProgressWnd, WM_FSTOOL_SETDESC, 0, (LPARAM) L"This tool will examine the File Allocation Table and Directory File Count and update the Volume Record to the same values." );
	::SendMessage( ProgressWnd, WM_FSTOOL_PROGLIMIT, 0, 100 );

	bool AllocsWrong = false;
	bool FilesWrong  = false;

	WORD FreeAllocs = pFAT->GetFreeAllocs();

	if ( FreeAllocs != VolRecord.UnusuedAllocs )
	{
		AllocsWrong = true;
	}

	if ( pDirectory->Files.size() != VolRecord.NumFiles )
	{
		FilesWrong = true;
	}

	BYTE Report[ 256 ];

	if ( ( !AllocsWrong ) && ( !FilesWrong ) )
	{
		rsprintf( Report, "File count and unused Free Allocation Block count are correct" );
	}
	else if ( ( !AllocsWrong ) && ( FilesWrong ) )
	{
		rsprintf( Report, "File count is %d and should be %d. Free Allocation Block count is correct. Fix?", VolRecord.NumFiles, pDirectory->Files.size() );
	}
	else if ( ( AllocsWrong ) && ( !FilesWrong ) )
	{
		rsprintf( Report, "File count is correct. Free Allocation Block count is %d and should be %d. Fix?", VolRecord.UnusuedAllocs, FreeAllocs );
	}
	else if ( ( AllocsWrong ) && ( FilesWrong ) )
	{
		rsprintf( Report, "File count is %d and should %d, Free Allocation Block is %d and should be %d. Filx?", VolRecord.NumFiles, pDirectory->Files.size(), VolRecord.UnusuedAllocs, FreeAllocs );
	}

	if ( ( AllocsWrong ) || ( FilesWrong ) )
	{
		if ( MessageBoxA( ProgressWnd, (char *) Report, "NUTS Macintosh MFS Validator", MB_ICONQUESTION | MB_YESNO ) == IDYES )
		{
			VolRecord.UnusuedAllocs = FreeAllocs;
			VolRecord.NumFiles      = pDirectory->Files.size();

			WriteVolumeRecord();
		}
	}
	else
	{
		MessageBoxA( ProgressWnd, (char *) Report, "NUTS Macintosh MFS Validator", MB_ICONINFORMATION | MB_OK );
	}

	::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, 100, 0 );

	return 0;
}


BYTE *MacintoshMFSFileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE desc[ 128 ];

	BYTE ft[ 5 ];
	BYTE fc[ 5 ];

	fc[ 4 ] = 0;
	ft[ 4 ] = 0;

	pMacDirectory->GetFinderCodes( FileIndex, ft, fc );

	rsprintf( desc, "Type: %s, Creator: %s", ft, fc );

	return desc;
}

int MacintoshMFSFileSystem::ReadFork( DWORD FileID, WORD ForkID, CTempFile &forkObj )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	DWORD BytesToGo = pFile->Attributes[ 15 ];

	DWORD AllocBlockBytesToGo = VolRecord.AllocSize;

	forkObj.Seek( 0 );

	BYTE Buffer[ 0x200 ];

	WORD  Alloc   = pFile->Attributes[ 3 ] - 2;
	DWORD DiscAdx = ( Alloc * VolRecord.AllocSize ) + ( VolRecord.FirstAlloc * 0x200 );

	DWORD SecNum  = DiscAdx / 0x200;

	while ( BytesToGo > 0 )
	{
		if ( pSource->ReadSectorLBA( SecNum, Buffer, 0x200 ) != DS_SUCCESS )
		{
			return -1;
		}

		AllocBlockBytesToGo -= 0x200;
		DiscAdx             += 0x200;

		forkObj.Write( Buffer, min( BytesToGo, 0x200 ) );

		SecNum++;

		if ( AllocBlockBytesToGo == 0 )
		{
			AllocBlockBytesToGo = VolRecord.AllocSize;

			Alloc = pFAT->NextAllocBlock( Alloc + 2 ) - 2;

			DiscAdx = DiscAdx = ( Alloc * VolRecord.AllocSize ) + ( VolRecord.FirstAlloc * 0x200 );

			SecNum = DiscAdx / 0x200;
		}

		BytesToGo -= min( BytesToGo, 0x200 );
	}

	forkObj.SetExt( pFile->Attributes[ 15 ] );
	
	return 0;
}

// The underlying function would probably be enough, but it'll write the whole directory.
// This is the "one ping only" version.
int MacintoshMFSFileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	DWORD EntryOffset = pFile->Attributes[ 14 ];

	EntryOffset += VolRecord.FirstBlock * 0x200;

	DWORD EntrySector = EntryOffset / 0x200;

	BYTE Buffer[ 512 ];

	if ( pSource->ReadSectorLBA( EntrySector, Buffer, 0x200 ) != DS_SUCCESS )
	{
		return -1;
	}

	BYTE *Entry = (BYTE *) &Buffer[ EntryOffset % 0x200 ];

	Entry[ 0 ] = ( Entry[ 0 ] & 0x7F ) | ( (Changes->Attributes[ 0 ] != 0 )?1:0 );

	WBEDWORD( &Entry[ 42 ], Changes->Attributes[ 4 ] + APPLE_TIME_OFFSET );
	WBEDWORD( &Entry[ 46 ], Changes->Attributes[ 5 ] + APPLE_TIME_OFFSET );

	if ( pSource->WriteSectorLBA( EntrySector, Buffer, 0x0200 ) != DS_SUCCESS )
	{
		return -1;
	}

	VolRecord.ModStamp = time(NULL) + APPLE_TIME_OFFSET;

	if ( WriteVolumeRecord() != NUTS_SUCCESS )
	{
		return -1;
	}

	return pDirectory->ReadDirectory();
}

int MacintoshMFSFileSystem::DeleteFile( DWORD FileID )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	// First, erase the FAT chain
	WORD StartAllocData = pFile->Attributes[ 2 ];
	WORD StartAllocRes  = pFile->Attributes[ 3 ];

	if ( StartAllocData != 0 )
	{
		pFAT->ClearAllocs( StartAllocData );
	}

	if ( StartAllocRes != 0 )
	{
		pFAT->ClearAllocs( StartAllocRes );
	}

	pFAT->WriteFAT();

	// Then remove the directory entry
	pDirectory->Files.erase( pDirectory->Files.begin() + FileID );

	int r = pDirectory->WriteDirectory();

	if ( r == NUTS_SUCCESS )
	{
		VolRecord.UnusuedAllocs = pFAT->GetFreeAllocs();
		VolRecord.NumFiles--;
		VolRecord.ModStamp      = time(NULL) + APPLE_TIME_OFFSET;

		r = WriteVolumeRecord();
	}

	if ( r == NUTS_SUCCESS )
	{
		r = pDirectory->ReadDirectory();
	}

	return r;
}

int MacintoshMFSFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	// Basic sanity checks first
	if ( ( pFile->EncodingID != ENCODING_ASCII) && ( pFile->EncodingID != ENCODING_MACINTOSH ) )
	{
		return FILEOP_NEEDS_ASCII;
	}

	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( pFile, &*iFile ) )
		{
			return FILEOP_EXISTS;
		}
	}

	WORD BlocksPerClump = VolRecord.AllocBytes / VolRecord.AllocSize;

	// Will this fit? We use Clump Size, as per spec
	DWORD DataAllocBlocks = pFile->Length / VolRecord.AllocSize;

	if ( pFile->Length % VolRecord.AllocSize ) { DataAllocBlocks++; }

	DWORD ResAllocBlocks = 0;

	// If it's not a MAC file, we won't create a resource fork.
	if ( ( pFile->FSFileType == FILE_MACINTOSH ) && ( Forks.size() > 0 ) && ( Forks[ 0 ]->Ext() > 0 ) )
	{
		ResAllocBlocks = pFile->Attributes[ 15 ] / VolRecord.AllocSize;

		if ( pFile->Attributes[ 15 ] % VolRecord.AllocSize ) { ResAllocBlocks++; }
	}

	if ( OverrideFork != nullptr )
	{
		DWORD ForkSize = (DWORD) OverrideFork->Ext();

		ResAllocBlocks = ForkSize / VolRecord.AllocSize;

		if ( ForkSize % VolRecord.AllocSize ) { ResAllocBlocks++; }

		pFile->Attributes[ 15 ] = ForkSize;
	}

	DWORD TotalAllocBlocks = DataAllocBlocks + ResAllocBlocks;

	if ( TotalAllocBlocks > 0 )
	{
		TotalAllocBlocks = max( BlocksPerClump, TotalAllocBlocks );
	}

	if ( pFAT->GetFreeAllocs() < TotalAllocBlocks )
	{
		// Not enough room - sad mac :(
		CleanupImportForks();

		return NUTSError( 0x805, L"Disk full" );
	}

	DWORD DataForkStart = 0;
	DWORD ResForkStart  = 0;

	// We're on.
	if ( DataAllocBlocks > 0 )
	{
		std::vector<WORD> AllocBlocks = pFAT->AllocateBlocks( DataAllocBlocks );

		if ( WriteForkData( store, &AllocBlocks ) != NUTS_SUCCESS )
		{
			CleanupImportForks();

			return -1;
		}
		
		DataForkStart = AllocBlocks[ 0 ];
	}

	if ( ResAllocBlocks > 0 )
	{
		std::vector<WORD> AllocBlocks = pFAT->AllocateBlocks( ResAllocBlocks );

		if ( OverrideFork != nullptr )
		{
			if ( WriteForkData( *OverrideFork, &AllocBlocks ) != NUTS_SUCCESS )
			{
				CleanupImportForks();

				return -1;
			}
		}
		else
		{
			if ( WriteForkData( *(Forks[0]), &AllocBlocks ) != NUTS_SUCCESS )
			{ 
				CleanupImportForks();

				return -1;
			}
		}

		ResForkStart = AllocBlocks[ 0 ];
	}

	// Now that we've written the data, we need to make a directory entry.
	// We can skip some fields: The file number needs to be next file number from the volume record.
	// The fork starts come from above, and the timestamps come from time().
	// Only the resource fork length needs to be added.

	NativeFile newFile = *pFile;
	
	newFile.Attributes[ 1 ] = VolRecord.NextUnusedFile;
	newFile.Attributes[ 2 ] = DataForkStart;
	newFile.Attributes[ 3 ] = ResForkStart;
	newFile.Attributes[ 4 ] = time( NULL );
	newFile.Attributes[ 5 ] = time( NULL );

	newFile.fileID = pDirectory->Files.size();

	AutoBuffer finder( 16 );

	BYTE *pFinder = finder;

	if ( pFile->FSFileType != FILE_MACINTOSH )
	{
		newFile.Attributes[ 0 ] = 0; // Not Locked

		if ( OverrideFinder != nullptr )
		{
			memcpy( (BYTE *) pFinder, (BYTE *) *OverrideFinder, 16 );
		}
		else
		{
			// We need to set up some finder info here.
			pFinder[ 0 ] = 'B'; pFinder[ 1 ] = 'I'; pFinder[ 2 ] = 'N'; pFinder[ 3 ] = 'A';
			pFinder[ 4 ] = 'N'; pFinder[ 5 ] = 'U'; pFinder[ 6 ] = 'T'; pFinder[ 7 ] = 'S';
		}
	}
	else
	{
		// Copy across the creator and type code
		* (DWORD *) &pFinder[ 0 ] = pFile->Attributes[ 11 ];
		* (DWORD *) &pFinder[ 4 ] = pFile->Attributes[ 12 ];
	}

	// Finder flags (?)
	WBEWORD( &pFinder[ 8 ], 0 );

	// Position
	WBEDWORD( &pFinder[ 10 ], 0 );

	// Folder number
	WBEWORD( &pFinder[ 14 ], 0 );

	pMacDirectory->FinderInfo.push_back( finder );

	if ( ( Forks.size() > 0 ) && ( Forks[ 0 ]->Ext() > 0 ) )
	{
		newFile.Attributes[ 15 ] = Forks[ 0 ]->Ext();
	}
	else
	{
		if ( OverrideFork != nullptr )
		{
			newFile.Attributes[ 15 ] = OverrideFork->Ext();
		}
		else
		{
			newFile.Attributes[ 15 ] = 0;
		}
	}

	pDirectory->Files.push_back( newFile );

	// Clear the forks or we'll try to write them next time
	Forks.clear();
	Forks.shrink_to_fit();

	CleanupImportForks();

	// Write the directory first
	int r = pDirectory->WriteDirectory();

	// Then write the volume record (as we updated the next file pointer, and decreased the free block count) 
	if ( r == NUTS_SUCCESS )
	{
		VolRecord.NextUnusedFile++;
		VolRecord.UnusuedAllocs -= DataAllocBlocks;
		VolRecord.UnusuedAllocs -= ResAllocBlocks;
		VolRecord.NumFiles++;
		VolRecord.ModStamp = time(NULL) + APPLE_TIME_OFFSET;

		r = WriteVolumeRecord();
	}

	// Write out the updated FAT
	if ( r == NUTS_SUCCESS )
	{
		r = pFAT->WriteFAT();
	}

	// ... aaaaaaaaaaaaand refresh.
	if ( r == NUTS_SUCCESS )
	{
		r = pDirectory->ReadDirectory();
	}

	return r;
}

int MacintoshMFSFileSystem::WriteForkData( CTempFile &store, std::vector<WORD> *pBlocks )
{
	DWORD BytesToGo = store.Ext();

	DWORD AllocBlockBytesToGo = VolRecord.AllocSize;

	store.Seek( 0 );

	BYTE Buffer[ 0x200 ];

	std::vector<WORD>::iterator iBlock = pBlocks->begin();

	WORD  Alloc   = (*iBlock) - 2;
	DWORD DiscAdx = ( Alloc * VolRecord.AllocSize ) + ( VolRecord.FirstAlloc * 0x200 );

	DWORD SecNum  = DiscAdx / 0x200;

	while ( BytesToGo > 0 )
	{
		store.Read( Buffer, min( BytesToGo, 0x200 ) );

		if ( pSource->WriteSectorLBA( SecNum, Buffer, 0x200 ) != DS_SUCCESS )
		{
			return -1;
		}

		AllocBlockBytesToGo -= 0x200;
		DiscAdx             += 0x200;

		SecNum++;

		BytesToGo -= min( BytesToGo, 0x200 );

		if ( ( AllocBlockBytesToGo == 0 ) && ( BytesToGo > 0 ) )
		{
			AllocBlockBytesToGo = VolRecord.AllocSize;

			iBlock++;

			if ( iBlock == pBlocks->end() )
			{
				// EEP!
				return NUTSError( 0x808, L"Mismatch between allocated blocks and fork size (software bug)" );
			}

			Alloc = (*iBlock) - 2;

			DiscAdx = DiscAdx = ( Alloc * VolRecord.AllocSize ) + ( VolRecord.FirstAlloc * 0x200 );

			SecNum = DiscAdx / 0x200;
		}
	}

	return 0;
}

LocalCommands MacintoshMFSFileSystem::GetLocalCommands( void )
{
	LocalCommands cmds;

	cmds.HasCommandSet = true;

	LocalCommand cmd;

	cmd.Flags = LC_Always;
	cmd.Name = L"Add Boot Block";

	cmds.CommandSet.push_back( cmd );

	cmd.Flags = LC_IsSeparator;
	cmd.Name = L"";

	cmds.CommandSet.push_back( cmd );

	cmd.Flags = LC_ApplyOne;
	cmd.Name  = L"Edit Finder Info";

	cmds.CommandSet.push_back( cmd );

	cmds.Root = L"Macintosh MFS";

	return cmds;
}

BYTE *FinderEditBuffer;

int MacintoshMFSFileSystem::ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd )
{
	if ( CmdIndex == 0 )
	{
		// Apply Boot Block. Let's check something first.
		BYTE Buffer[ 1024 ];

		if ( pSource->ReadSectorLBA( 0, &Buffer[ 0x000 ], 0x200 ) != DS_SUCCESS )
		{
			return -1;
		}

		if ( ( Buffer[ 0 ] == 0x4C ) && ( Buffer[ 1 ] == 0x4B ) )
		{
			if ( MessageBox( hParentWnd,
				L"This disk appears to have a boot block installed already. "
				L"If you continue, it will be replaced, which may cause it to stop working. "
				L"This operation cannot be undone.\r\n\r\n"
				L"Continue?", L"NUTS Macintosh MFS Boot Block", MB_ICONWARNING | MB_YESNO ) == IDNO )
			{
				return 0;
			}
		}
		else
		{
			if ( MessageBox( hParentWnd,
				L"Installing a boot block will erase the first two sectors of the disk, "
				L"which may cause it to stop working. This operation cannot be undone.\r\n\r\n"
				L"Continue?", L"NUTS Macintosh MFS Boot Block", MB_ICONWARNING | MB_YESNO ) == IDNO )
			{
				return 0;
			}
		}

		// We're on. Need to load the resource
		HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDR_MFSBOOT ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hInstance, hResource);
		DWORD dwSize     = SizeofResource(hInstance, hResource);
		LPVOID lpAddress = LockResource(hMemory);

		memcpy( Buffer, lpAddress, dwSize );

		UnlockResource( hMemory );

		if ( pSource->WriteSectorLBA( 0, &Buffer[ 0x000 ], 0x200 ) != DS_SUCCESS )
		{
			return -1;
		}

		if ( pSource->WriteSectorLBA( 1, &Buffer[ 0x000 ], 0x200 ) != DS_SUCCESS )
		{
			return -1;
		}

		MessageBox( hParentWnd, L"Boot Block installed successfully.", L"NUTS Macintosh MFS Boot Block", MB_ICONINFORMATION | MB_OK );

		return 0;
	}

	// Remember! Separators count!
	if ( CmdIndex == 2 )
	{
		FinderEditBuffer = pMacDirectory->FinderInfo[ Selection[0].fileID ];

		if ( DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_FINDEREDIT ), hParentWnd, FinderWindowProc, 0 ) == IDOK )
		{
			int r = pDirectory->WriteDirectory();

			if ( r == NUTS_SUCCESS )
			{
				r = CMDOP_REFRESH;
			}

			return r;
		}

		return 0;
	}

	return NUTSError( 0x001, L"Unknown command index" );
}

HFONT hFont = NULL;
HFONT hBold = NULL;

EncodingEdit *pFileType, *pCreatorCode, *pFinderFlags, *pPosition, *pFolderNum;

extern BYTE *pChicago;

EncodingEdit *SetupEdit( HWND parent, int wnd, WORD MaxLen )
{
	RECT r;

	GetWindowRect( GetDlgItem( parent, wnd ), &r );
	MapWindowPoints( GetDesktopWindow(), parent, (LPPOINT) &r, 2 );

	EncodingEdit *pNew = new EncodingEdit( parent, r.left + 85, r.top - 4, 80, pChicago );

	pNew->MaxLen = MaxLen;

	return pNew;
}

INT_PTR CALLBACK MacintoshMFSFileSystem::FinderWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg )
	{
		case WM_CTLCOLORSTATIC:
			{
				if ((HWND) lParam == GetDlgItem(hwndDlg, IDC_MFSTITLE)) {
					HDC	hDC	= (HDC) wParam;

					SetBkColor(hDC, 0xFFFFFF);

					return (INT_PTR) GetStockObject(WHITE_BRUSH);
				}
			}

			break;

		case WM_INITDIALOG:
			if (!hFont) {
				hFont	= CreateFont(36,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
			}

			SendMessage(GetDlgItem(hwndDlg, IDC_MFSTITLE), WM_SETFONT, (WPARAM) hFont, 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_MFSTITLE), WM_SETTEXT, 0, (LPARAM) L"       Configure Finder Properties" );

			pFileType    = SetupEdit( hwndDlg, IDC_MFSFILETYPE, 4 );
			pCreatorCode = SetupEdit( hwndDlg, IDC_MFSCREATOR,  4 );
			pFinderFlags = SetupEdit( hwndDlg, IDC_MFSFLAGS,    4 );
			pPosition    = SetupEdit( hwndDlg, IDC_MFSPOSITION, 8 );
			pFolderNum   = SetupEdit( hwndDlg, IDC_MFSFOLDER,   4 );

			pFinderFlags->AllowedChars = EncodingEdit::InputType::textInputHex;
			pPosition->AllowedChars = EncodingEdit::InputType::textInputHex;
			pFolderNum->AllowedChars = EncodingEdit::InputType::textInputHex;

			// Do some format conversions
			BYTE hex[ 16 ];

			rsprintf( hex, "%08X", BEWORD(  &FinderEditBuffer[ 8  ] ) ); pFinderFlags->SetText( hex );
			rsprintf( hex, "%08X", BEDWORD( &FinderEditBuffer[ 10 ] ) ); pPosition->SetText( hex );
			rsprintf( hex, "%08X", BEWORD(  &FinderEditBuffer[ 14 ] ) ); pFolderNum->SetText( hex );

			// These are ... fruity

			rstrncpy( hex, &FinderEditBuffer[ 0 ], 4 ); pFileType->SetText( hex );
			rstrncpy( hex, &FinderEditBuffer[ 4 ], 4 ); pCreatorCode->SetText( hex );

			break;

		case WM_PAINT:
			{
				RECT r;

				GetClientRect( GetDlgItem( hwndDlg, IDC_MFSTITLE ), &r );

				r.left   -= 10;
				r.right  += 10;
				r.top    -= 10;
				r.bottom += 2;

				HDC hDC = GetDC( hwndDlg );

				DrawEdge( hDC, &r, EDGE_SUNKEN, BF_RECT );

				ReleaseDC( hwndDlg, hDC );
			}
			return FALSE;

		case WM_CLOSE:
			EndDialog( hwndDlg, IDCANCEL );
			break;

		case WM_COMMAND:
			if ( wParam == IDOK )
			{
				BYTE *pT = pFileType->GetText();

				memcpy( &FinderEditBuffer[  0 ], pT, 4 ); // Trust me.

				pT = pCreatorCode->GetText();

				memcpy( &FinderEditBuffer[  4 ], pT, 4 ); // No really.

				WBEWORD(  &FinderEditBuffer[  8 ], (WORD) pFinderFlags->GetHexText() );

				long v = pPosition->GetHexText();

				WBEDWORD( &FinderEditBuffer[ 10 ], * (DWORD *) &v );

				WBEWORD(  &FinderEditBuffer[ 14 ], (WORD) pFolderNum->GetHexText() );
				
				EndDialog( hwndDlg, IDOK );
			}

			if ( wParam == IDCANCEL )
			{
				EndDialog( hwndDlg, IDCANCEL );
			}
			break;
	}

	return FALSE;
}

int MacintoshMFSFileSystem::ExportSidecar( NativeFile *pFile, SidecarExport &sidecar )
{
	BYTEString sidecarF = BYTEString( pFile->Filename.size() + 2 );

	sidecarF[ 0 ] = (BYTE) '.';
	sidecarF[ 1 ] = (BYTE) '_';

	memcpy( &sidecarF[ 2 ], (BYTE *) pFile->Filename, pFile->Filename.length() );

	sidecar.Filename = BYTEString( (BYTE *) sidecarF, sidecarF.length() );

	BYTE SidecarHeader[ 32 ];

	WBEDWORD( &SidecarHeader[ 0 ], 0x00051607 ); // Magic number
	WBEDWORD( &SidecarHeader[ 4 ], 0x00020000 ); // Version 2

	memcpy( &SidecarHeader[ 8 ], (BYTE *) "Macintosh       ", 16 );

	DWORD Offset1 = 26;
	DWORD Offset2 = 0;

	if ( pFile->Attributes[ 15 ] > 0 )
	{
		Offset1 += 2 * 12;
		Offset2 = Offset1 + 16;
		WBEWORD( &SidecarHeader[ 24 ], 2 );
	}
	else
	{
		Offset1 += 12 + 16;
		WBEWORD( &SidecarHeader[ 24 ], 1 );
	}

	// Write out the header
	CTempFile *pObj = (CTempFile *) sidecar.FileObj;

	pObj->Seek( 0 );
	pObj->Write( SidecarHeader, 26 );

	// Export finder info
	BYTE bedword[ 4 ];

	WBEDWORD( bedword, 9 ); pObj->Write( bedword, 4 ); // Finder Info
	WBEDWORD( bedword, Offset1 ); pObj->Write( bedword, 4 ); // offset
	WBEDWORD( bedword, 16 ); pObj->Write( bedword, 4 ); // Length

	if ( pFile->Attributes[ 15 ] > 0 )
	{
		WBEDWORD( bedword, 2 ); pObj->Write( bedword, 4 ); // Resource fork
		WBEDWORD( bedword, Offset2 ); pObj->Write( bedword, 4 ); // offset
		WBEDWORD( bedword, pFile->Attributes[ 15 ] ); pObj->Write( bedword, 4 ); // Length
	}

	// Write the finder info block
	pObj->Write( (BYTE *) pMacDirectory->FinderInfo[ pFile->fileID ], 16 );

	// Write the resource fork if we have one.

	if ( pFile->Attributes[ 15 ] > 0 )
	{
		CTempFile fork;

		if ( ReadFork( pFile->fileID, 0, fork ) != NUTS_SUCCESS )
		{
			return -1;
		}

		BYTE Buffer[ 0x200 ];

		DWORD BytesToGo = fork.Ext();

		fork.Seek( 0 );

		while ( BytesToGo > 0 )
		{
			fork.Read( Buffer, min( BytesToGo, 0x200 ) );
			pObj->Write( Buffer, min( BytesToGo, 0x200 ) );

			BytesToGo -= min( BytesToGo, 0x200 );
		}
	}

	return 0;
}

int  MacintoshMFSFileSystem::ImportSidecar( NativeFile *pFile, SidecarImport &sidecar, CTempFile *obj )
{
	CleanupImportForks();

	if ( obj == nullptr )
	{
		// Wants the filename to look for
		BYTEString sidecarF = BYTEString( pFile->Filename.size() + 2 );

		sidecarF[ 0 ] = (BYTE) '.';
		sidecarF[ 1 ] = (BYTE) '_';

		memcpy( &sidecarF[ 2 ], (BYTE *) pFile->Filename, pFile->Filename.length() );

		sidecar.Filename = BYTEString( (BYTE *) sidecarF, sidecarF.length() );

		if ( pFile->Flags & FF_Extension )
		{
			WORD l = sidecarF.length() + 1 + pFile->Extension.length() + 1;

			BYTEString sidecarE = BYTEString( l );

			rstrncpy( (BYTE *) sidecarE, (BYTE *) sidecarF, l );
			rstrncat( (BYTE *) sidecarE, (BYTE *) ".", l );
			rstrncat( (BYTE *) sidecarE, (BYTE *) pFile->Extension, l );

			sidecar.Filename = BYTEString( (BYTE *) sidecarE, sidecarE.length() );
		}
	}
	else
	{
		// has the data
		BYTE Data[ 16 ];

		obj->Seek( 0 );
		obj->Read( Data, 4 );

		if ( BEDWORD( Data ) != 0x00051607 )
		{
			return NUTSError( 0x809, L"Bad AppleDouble file (No magic number)" );
		}

		obj->Read( Data, 4 );

		if ( BEDWORD( Data ) != 0x00020000 )
		{
			return NUTSError( 0x809, L"Bad AppleDouble file (bad version)" );
		}

		obj->Read( Data, 16 );

		// Skip the Home FS. It seems it's not reliably set anyway.

		WORD forks;
		DWORD Offset1 = 0;
		DWORD Offset2 = 0;
		DWORD Length1 = 0;
		DWORD Length2 = 0;

		obj->Read( Data, 2 );

		forks = BEWORD( Data );

		for ( WORD i=0; i<forks; i++ )
		{
			DWORD Type;
			DWORD Offset;
			DWORD Length;

			obj->Read( Data, 4 ); Type   = BEDWORD( Data );
			obj->Read( Data, 4 ); Offset = BEDWORD( Data );
			obj->Read( Data, 4 ); Length = BEDWORD( Data );

			if ( Type == 0x00000009 )
			{
				Offset1 = Offset;
				Length1 = Length;
			}

			if ( Type == 0x00000002 )
			{
				Offset2 = Offset;
				Length2 = Length;
			}
		}
		
		if ( Offset1 != 0 )
		{
			OverrideFinder = new AutoBuffer( 16 );

			obj->Seek( Offset1 );
			
			memset( (BYTE *) *OverrideFinder, 0, 16 );

			obj->Read( (BYTE *) *OverrideFinder, min( 16, Length1 ) );
		}

		if ( Offset2 != 0 )
		{
			OverrideFork = new CTempFile;

			BYTE Buffer[ 0x200 ];

			DWORD BytesToGo = Length2;

			Length2 = min( Length2, ( obj->Ext() - Offset2 ) );
		
			obj->Seek( Offset2 );
			OverrideFork->Seek( 0 );

			while ( BytesToGo > 0 )
			{
				obj->Read( Buffer, min( BytesToGo, 0x200 ) );
				OverrideFork->Write( Buffer, min( BytesToGo, 0x200 ) );

				BytesToGo -= min( BytesToGo, 0x200 );
			}
		}
	}

	return 0;
}

int MacintoshMFSFileSystem::WriteCleanup( void )
{
	CleanupImportForks();

	return FileSystem::WriteCleanup();
}

void MacintoshMFSFileSystem::CleanupImportForks()
{
	if ( OverrideFork != nullptr )
	{
		delete OverrideFork;
	}

	OverrideFork = nullptr;

	if ( OverrideFinder != nullptr )
	{
		delete OverrideFinder;
	}

	OverrideFinder = nullptr;
}

DWORD sAllocBlockSize = 1024;
DWORD sClumpSize      = 8192;
WORD  sDirBlocks      = 12;

INT_PTR CALLBACK MacintoshMFSFileSystem::FormatWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg )
	{
		case WM_CTLCOLORSTATIC:
			{
				if ((HWND) lParam == GetDlgItem(hwndDlg, IDC_MFSTITLE)) {
					HDC	hDC	= (HDC) wParam;

					SetBkColor(hDC, 0xFFFFFF);

					return (INT_PTR) GetStockObject(WHITE_BRUSH);
				}
			}

			break;

		case WM_INITDIALOG:
			if (!hFont) {
				hFont = CreateFont(36,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
			}

			if (!hBold) {
				hBold = CreateFont(12,0,0,0,FW_BOLD,FALSE,TRUE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
			}

			SendMessage(GetDlgItem(hwndDlg, IDC_MFSTITLE), WM_SETFONT, (WPARAM) hFont, 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_MFSTITLE), WM_SETTEXT, 0, (LPARAM) L"       Format Macintosh MFS Disk" );

			SendMessage(GetDlgItem(hwndDlg, IDC_DIRBLOCKLEN), WM_SETFONT, (WPARAM) hBold, 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_ALLOCSIZE), WM_SETFONT, (WPARAM) hBold, 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_ALLOCBYTES), WM_SETFONT, (WPARAM) hBold, 0);

			SendMessage( GetDlgItem( hwndDlg, IDC_DIRLENSLIDE ),     TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) 0x03000001 );
			SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCSIZESLIDE ),  TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) 0x003E0001 );
			SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCBYTESSLIDE ), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) 0x003E0001 );

			SendMessage( GetDlgItem( hwndDlg, IDC_DIRLENSLIDE ),     TBM_SETLINESIZE, (WPARAM) 0, (LPARAM) 0x00000001 );
			SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCSIZESLIDE ),  TBM_SETLINESIZE, (WPARAM) 0, (LPARAM) 0x00000001 );
			SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCBYTESSLIDE ), TBM_SETLINESIZE, (WPARAM) 0, (LPARAM) 0x00000001 );

			SendMessage( GetDlgItem( hwndDlg, IDC_DIRLENSLIDE ),     TBM_SETPAGESIZE, (WPARAM) 0, (LPARAM) 0x00000001 );
			SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCSIZESLIDE ),  TBM_SETPAGESIZE, (WPARAM) 0, (LPARAM) 0x00000200 );
			SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCBYTESSLIDE ), TBM_SETPAGESIZE, (WPARAM) 0, (LPARAM) 0x00000200 );

			SendMessage( GetDlgItem( hwndDlg, IDC_DIRLENSLIDE ),     TBM_SETTICFREQ, (WPARAM) 0x00000001, (LPARAM) 0 );
			SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCSIZESLIDE ),  TBM_SETTICFREQ, (WPARAM) 0x00000200, (LPARAM) 0 );
			SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCBYTESSLIDE ), TBM_SETTICFREQ, (WPARAM) 0x00000200, (LPARAM) 0 );

			SendMessage( GetDlgItem( hwndDlg, IDC_DIRLENSLIDE ),     TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 12 );
			SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCSIZESLIDE ),  TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 2 );
			SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCBYTESSLIDE ), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 16 );

			break;

		case WM_CLOSE:
			EndDialog( hwndDlg, 0 );
			break;

		case WM_HSCROLL:
			{
				BYTE v[16];

				sDirBlocks = SendMessage( GetDlgItem( hwndDlg, IDC_DIRLENSLIDE ), TBM_GETPOS, 0, 0 );
				rsprintf( v, "%d", sDirBlocks );
				SendMessageA( GetDlgItem( hwndDlg, IDC_DIRLENVAL ), WM_SETTEXT, 0, (LPARAM) v );

				sAllocBlockSize = SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCSIZESLIDE ), TBM_GETPOS, 0, 0 ) * 0x200;
				rsprintf( v, "%d", sAllocBlockSize );
				SendMessageA( GetDlgItem( hwndDlg, IDC_ALLOCSIZEVAL ), WM_SETTEXT, 0, (LPARAM) v );

				sClumpSize = SendMessage( GetDlgItem( hwndDlg, IDC_ALLOCBYTESSLIDE ), TBM_GETPOS, 0, 0 ) * 0x200;
				rsprintf( v, "%d", sClumpSize );
				SendMessageA( GetDlgItem( hwndDlg, IDC_ALLOCBYTESVAL ), WM_SETTEXT, 0, (LPARAM) v );
			}
			break;

		case WM_COMMAND:
			if ( wParam == IDOK )
			{
				EndDialog( hwndDlg, IDOK );
			}
			else if ( wParam == IDCANCEL )
			{
				EndDialog( hwndDlg, IDCANCEL );
			}
			break;

		case WM_PAINT:
			{
				RECT r;

				GetClientRect( GetDlgItem( hwndDlg, IDC_MFSTITLE ), &r );

				r.left   -= 10;
				r.right  += 10;
				r.top    -= 10;
				r.bottom += 2;

				HDC hDC = GetDC( hwndDlg );

				DrawEdge( hDC, &r, EDGE_SUNKEN, BF_RECT );

				ReleaseDC( hwndDlg, hDC );
			}
			return FALSE;
	}
	return FALSE;
}

int	MacintoshMFSFileSystem::Format_PreCheck( int FormatType, HWND hWnd )
{
	if ( DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_MFSFORMAT ), hWnd, FormatWindowProc, 0 ) == IDOK )
	{
		return 0;
	}

	return -1;
}

int MacintoshMFSFileSystem::Format_Process( DWORD FT, HWND hWnd )
{
	static WCHAR * const InitMsg  = L"Initialising";
	static WCHAR * const EraseMsg = L"Erasing";
	static WCHAR * const FATMsg   = L"Creating FAT";
	static WCHAR * const RootMsg  = L"Creating Root Directory";
	static WCHAR * const DoneMsg  = L"Complete";

	if ( MYFSID == FSID_MFS )
	{
		SetShape();
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, 0, (LPARAM) EraseMsg );

	BYTE SectorBuf[ 512 ];

	if ( FT & FTF_Blank )
	{
		ZeroMemory( SectorBuf, 512 );

		if ( MYFSID == FSID_MFS )
		{
			WORD Secs = 0;

			for ( WORD track=0; track<80; track++ )
			{
				for ( WORD sector=0; sector<10; sector++ )
				{
					if ( WaitForSingleObject( hCancelFormat, 0 ) == WAIT_OBJECT_0 )
					{
						return 0;
					}

					pSource->WriteSectorCHS( 0, track, sector, SectorBuf );

					Secs++;

					PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 0, 3, Secs, 80 * 10, false ), (LPARAM) EraseMsg );
				}
			}
		}

		if ( MYFSID == FSID_MFS_HD )
		{
			DWORD Secs = pSource->PhysicalDiskSize / (QWORD) 0x200;

			for ( DWORD i=0; i<Secs; i++ )
			{
				if ( WaitForSingleObject( hCancelFormat, 0 ) == WAIT_OBJECT_0 )
				{
					return 0;
				}

				pSource->WriteSectorLBA( i, SectorBuf, 0x200 );

				if ( ( i % 100 ) == 0 )
				{
					PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 0, 3, i, Secs, false ), (LPARAM) EraseMsg );
				}
			}
		}
	}

	if ( FT & FTF_Initialise )
	{
		PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 1, 3, 0, 1, false ), (LPARAM) InitMsg );

		// We need to calculate some stuff now.
		DWORD Secs = 80 * 10;

		if ( MYFSID == FSID_MFS_HD )
		{
			Secs = pSource->PhysicalDiskSize / (QWORD) 0x200;
		}

		// First we need to know how big the FAT will be. Since this is dependent on a value
		// that is in turn dependent on this, estimate it for now, assuming just the one FAT
		// sector.
		WORD DataSize = ( Secs - ( 4 + sDirBlocks ) ) / ( sAllocBlockSize / 512 );

		// Subtract an allocation block for the backup MDB (this is optional, and we don't
		// write it, but something might). We need at least 2 sectors, which is two blocks
		// if block size is 512, or 1 block in all other cases.
		if ( sAllocBlockSize == 512 )
		{
			DataSize -= 2;
		}
		else
		{
			DataSize -= 1;
		}

		WORD FATSize  = ( DataSize & 0xFFFE ) + 2;

		// In case we have a REAL BIG DISK
		if ( FATSize == 0 ) { FATSize = 0xFFFE; }

		// Convert to number of sectors
		FATSize = ( ( FATSize / 2 ) * 3 ) + 64;

		WORD FATSecs = FATSize / 512;

		if ( FATSize % 512 ) { FATSecs++; }

		// Next, we'll need the start of the data area in sectors.
		WORD DataStart = 2 + sDirBlocks + FATSecs;

		// Now we need to know how many allocation blocks that is.
		QWORD RealDataSize = ( Secs - DataStart ) / ( sAllocBlockSize / 512 );

		if ( RealDataSize > 0xFF00 )
		{
			return NUTSError( 0x80E, L"This disk cannot be represented with these size settings. Please try again with different settings, most likely the Allocation Block Size needs increasing." );
		}

		DataSize = (WORD) RealDataSize;

		// Now let's construct the volume record
		VolRecord.AllocBlocks    = DataSize;
		VolRecord.AllocBytes     = sClumpSize;
		VolRecord.AllocSize      = sAllocBlockSize;
		VolRecord.DirBlockLen    = sDirBlocks;
		VolRecord.FirstAlloc     = DataStart;
		VolRecord.FirstBlock     = 2 + FATSecs;
		VolRecord.InitStamp      = time(NULL) + APPLE_TIME_OFFSET;
		VolRecord.ModStamp       = time(NULL) + APPLE_TIME_OFFSET;
		VolRecord.NumFiles       = 0;
		VolRecord.UnusuedAllocs  = DataSize;
		VolRecord.Valid          = true;
		VolRecord.NextUnusedFile = 1;
		VolRecord.VolAttrs       = 0;

		VolRecord.VolumeName = BYTEString( (BYTE *) "Macintosh", 9 );

		if ( WaitForSingleObject( hCancelFormat, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		if ( WriteVolumeRecord() != NUTS_SUCCESS )
		{
			return -1;
		}

		PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 2, 3, 0, 2, false ), (LPARAM) FATMsg );

		// Now we must fill in a fat
		pFAT->CreateFAT( DataSize );

		if ( WaitForSingleObject( hCancelFormat, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		if ( pFAT->WriteFAT() != NUTS_SUCCESS )
		{
			return -1;
		}

		PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 2, 3, 1, 2, false ), (LPARAM) RootMsg );

		if ( pDirectory->WriteDirectory() != NUTS_SUCCESS )
		{
			return -1;
		}

		if ( FT & FTF_Truncate )
		{
			// Truncate the disk to the last sector of the directory
			pSource->Truncate( ( VolRecord.FirstBlock + VolRecord.DirBlockLen ) * 0x200 );
		}
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 3, 3, 1, 1, true ), (LPARAM) DoneMsg );

	return 0;
}