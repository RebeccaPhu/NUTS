#include "StdAfx.h"
#include "IECATAFileSystem.h"
#include "../nuts/FormatWizard.h"

#include "CBMFunctions.h"

//	Files on IEC-ATA are stored as a table of sector links. The sector entry in the descriptor is a sector
//	containing a list of data sectors (4 bytes/32-bit LE uint) with the actual file. The last of these entries
//	is not actually a data sector, but a pointer to the next table sector.

int	IECATAFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	BYTE LinkTable[512];
	BYTE SectorBuf[512];

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	if ( pSource->ReadSectorLBA( pFile->Attributes[ 0 ], LinkTable, 512 ) != DS_SUCCESS )
	{
		return -1;
	}

	DWORD RemainingLength = (DWORD) pFile->Length;
	DWORD SectorPtr       = 0;

	DWORD *pBlocks = (DWORD *) LinkTable;

	DWORD DataToRead = 0;

	while ( RemainingLength )
	{
		if ( SectorPtr == 127 )
		{
			//	Not a data sector, another link table.
			DWORD NextPtr = pBlocks[127];

			if ( pSource->ReadSectorLBA( NextPtr, LinkTable, 512 ) != DS_SUCCESS )
			{
				return -1;
			}

			SectorPtr = 0;
		} else {
			DWORD Sector = pBlocks[ SectorPtr++ ];

			DataToRead = min( 512, RemainingLength );

			if ( pSource->ReadSectorLBA( Sector, SectorBuf, 512 ) != DS_SUCCESS )
			{
				return -1;
			}

			store.Write( SectorBuf, DataToRead );

			RemainingLength	-= DataToRead;
		}
	}

	return 0;
}

int	IECATAFileSystem::WriteFile( NativeFile *pFile, CTempFile &store )
{
	/* Is the source encoding compatible ? */
	if ( ( pFile->EncodingID != ENCODING_PETSCII ) && ( pFile->EncodingID != ENCODING_ASCII ) )
	{
		return FILEOP_NEEDS_ASCII;
	}

	if ( pFile->EncodingID == ENCODING_ASCII )
	{
		static NativeFile ASCIIFile;
		
		ASCIIFile = *pFile;

		IncomingASCII( &ASCIIFile );

		pFile = &ASCIIFile;
	}

	/* Existence check */
	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( pFile, &*iFile ) )
		{
			/* Check if this is a directory */
			if ( iFile->Flags & FF_Directory )
			{
				return FILEOP_ISDIR;
			}
			else
			{
				return FILEOP_EXISTS;
			}
		}
	}

	//	This is complicated. First we must write the file, then the directory entry.

	//	Step 1. Allocate blocks and write the file. First get a sector link block
	DWORD SectorLink    = pIECDirectory->GetFreeBlock();
	DWORD DataRemaining = (DWORD) pFile->Length;
	DWORD DataToWrite;
	DWORD DataPtr       = 0;
	DWORD LinkPtr       = 0;
	DWORD LinkTable[128];
	BYTE  Data[512];

	if ( SectorLink == 0 )
	{
		// Didn't get a link table sector
		return -1;
	}

	NativeFile outfile = *pFile;

	store.Seek( 0 );

	outfile.Attributes[ 0 ] = SectorLink;

	while (DataRemaining) {
		DataToWrite = min( 512, DataRemaining) ; 

		DWORD Sector = pIECDirectory->GetFreeBlock();

		if ( Sector == 0 )
		{
			// None free! Eep!
			return -1;
		}

		if ( LinkPtr == 127 )
		{
			// Filled up this link table sector. Need a new one.
			DWORD NewLink = pIECDirectory->GetFreeBlock();

			if ( NewLink == 0 )
			{
				return -1;
			}

			LinkTable[127] = NewLink;

			if ( pSource->WriteSectorLBA( SectorLink, (BYTE *) LinkTable, 512 ) != DS_SUCCESS )
			{
				return -1;
			}

			SectorLink = NewLink;
			LinkPtr	   = 0;

			memset( LinkTable, 0, 512 );
		}

		store.Read( &Data, DataToWrite );

		LinkTable[ LinkPtr ] = Sector;

		if ( pSource->WriteSectorLBA( Sector, Data, 512 ) != DS_SUCCESS )
		{
			return -1;
		}

		LinkPtr++;

		DataRemaining -= DataToWrite;
		DataPtr       += DataToWrite;
	}

	if ( pSource->WriteSectorLBA(SectorLink, (BYTE *) LinkTable, 512 ) != DS_SUCCESS )
	{
		return -1;
	}

	//	Step 2.	Create an entry in the Files list, and write the directory back.
	if ( pFile->FSFileType == FT_CBM_TAPE )
	{
		outfile.AttrLocked = 0xFFFFFFFF;
		outfile.AttrClosed = 0xFFFFFFFF;
		outfile.AttrType   = pFile->Attributes[ 3 ];
	}
	else if ( pFile->FSFileType != FT_C64 )
	{
		/* Foreign file. Make this a PRG. */
		outfile.AttrLocked = 0xFFFFFFFF;
		outfile.AttrClosed = 0xFFFFFFFF;
		outfile.AttrType   = 2; // PRG

		/* Check the extension if it has one, it might be a recognised type */
		if ( pFile->Flags & FF_Extension )
		{
			if ( rstrnicmp( pFile->Extension, (BYTE *) "SEQ", 3 ) )
			{
				outfile.AttrType = 1; // SEQ
			}
		}
	}
	else
	{
		// Must do some pruning on the type here. IECATA doesn't recognise DEL, or beyond PRG.
		if ( ( outfile.AttrType == 0 ) || ( outfile.AttrType > 2 ) )
		{
			outfile.AttrType = 2;
		}
	}

	pDirectory->Files.push_back(outfile);

	if ( pDirectory->WriteDirectory() != NUTS_SUCCESS )
	{
		return -1;
	}

	return pDirectory->ReadDirectory();
}

int IECATAFileSystem::DeleteFile( DWORD FileID )
{
	std::vector<NativeFile>::iterator iFile = pDirectory->Files.begin() + FileID;

	if ( ReleaseBlocks( &*iFile ) != NUTS_SUCCESS )
	{
		return -1;
	}

	iFile = pDirectory->Files.erase( iFile );

	if ( pDirectory->WriteDirectory() != NUTS_SUCCESS )
	{
		return -1;
	}

	return pDirectory->ReadDirectory();
}

int	IECATAFileSystem::ReleaseBlocks( NativeFile *pFile )
{
	unsigned char LinkTable[512];

	if ( pSource->ReadSectorLBA( pFile->Attributes[ 0 ], LinkTable, 512 ) != DS_SUCCESS )
	{
		return -1;
	}

	DWORD RemainingLength = (DWORD) pFile->Length;
	DWORD SectorPtr       = 0;
	DWORD DataToRead      = 0;

	DWORD *pBlocks= (DWORD *) LinkTable;

	std::vector<DWORD> ReturnBlocks;

	ReturnBlocks.push_back( pFile->Attributes[ 0 ] );

	while ( RemainingLength > 0U )
	{
		if ( SectorPtr == 127 )
		{
			//	Not a data sector, another link table.
			DWORD NextPtr = pBlocks[ 127 ];

			if ( pSource->ReadSectorLBA( NextPtr, LinkTable, 512 ) != DS_SUCCESS )
			{
				return -1;
			}

			ReturnBlocks.push_back( NextPtr );

			SectorPtr = 0;
		} else {
			DWORD Sector = pBlocks[ SectorPtr++ ];

			DataToRead = min( 512, RemainingLength );

			ReturnBlocks.push_back( Sector );

			RemainingLength -= DataToRead;
		}
	}

	if ( pIECDirectory->ReleaseBlock( &ReturnBlocks ) != NUTS_SUCCESS )
	{
		return -1;
	}

	return 0;
}

BYTE *IECATAFileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE status[128];

	if ( ( FileIndex < 0 ) || ( FileIndex > pDirectory->Files.size() ) )
	{
		status[0] = 0;

		return status;
	}

	NativeFile *pFile = &pDirectory->Files[FileIndex];

	if ( pDirectory->Files[ FileIndex ].Flags & FF_Directory )
	{
		rsprintf( status, "DIRECTORY" );
	}
	else
	{
		rsprintf( status, "%s%s%s %d BYTES, %s, %s",
			(pFile->AttrClosed)?"":"*",
			(pFile->Flags & FF_Extension)?(char *) pFile->Extension:"",
			(pFile->AttrLocked)?"<":"",
			 (DWORD) pFile->Length,
			(pFile->AttrLocked)?"LOCKED":"NOT LOCKED",
			(pFile->AttrClosed)?"CLOSED":"NOT CLOSED (SPLAT)"
		);
	}

	return status;
}

BYTE *IECATAFileSystem::GetStatusString( int FileIndex, int SelectedItems )
{
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
		NativeFile *pFile = &pDirectory->Files[FileIndex];

		if ( pDirectory->Files[ FileIndex ].Flags & FF_Directory ) {
			rsprintf( status, "%s DIRECTORY", (BYTE *) pFile->Filename );
		} else {
			rsprintf( status, "%s.%s%s%s - %d BYTES, %s, %s",
				 (BYTE *) pFile->Filename,
				(pFile->AttrLocked)?"":">",
				(pFile->AttrClosed)?"":"*",
				(pFile->Flags & FF_Extension)?(char *)(BYTE *) pFile->Extension:"",
				 (DWORD) pFile->Length,
				(pFile->AttrLocked)?"LOCKED":"NOT LOCKED",
				(pFile->AttrClosed)?"CLOSED":"NOT CLOSED (SPLAT)"
			);
		}
	}

	return status;
}

BYTE *IECATAFileSystem::GetTitleString( NativeFile *pFile, DWORD Flags )
{
	static BYTE title[512];
	
	if ( pFile == nullptr )
	{
		rsprintf( title, "IECATA::$" );
	}
	else
	{
		rsprintf( title, "%s.%s", (BYTE *) pFile->Filename, (BYTE *) pFile->Extension );

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

int IECATAFileSystem::ChangeDirectory(NativeFile *pFile)
{
	if ( !pFile->Flags && FF_Directory )
	{
		return -1;
	}

	DWORD NewDirSector = pFile->Attributes[ 0 ];

	ParentLinks[ NewDirSector ] = DirSector;

	DirSector = NewDirSector;

	pIECDirectory->SetDirSector( NewDirSector );

	pDirectory->ReadDirectory();

	return 0;
}

int	IECATAFileSystem::Parent()
{
	if ( ParentLinks[ DirSector ] == 0)
	{
		return -1;
	}

	DirSector = ParentLinks[ DirSector ];

	pIECDirectory->SetDirSector( DirSector );

	pDirectory->ReadDirectory( );

	return 0;
}

int	IECATAFileSystem::CreateDirectory( NativeFile *pDir, DWORD CreateFlags )
{
	return -1;
}

bool IECATAFileSystem::IsRoot()
{
	if ( ParentLinks[ DirSector ] == 0 )
	{
		return true;
	}

	return false;
}


int IECATAFileSystem::Format_PreCheck(int FormatType, HWND hWnd)
{
	//	No further options to check.

	return 0;
}

int IECATAFileSystem::Format_Process( DWORD FT, HWND hWnd )
{
	ResetEvent( hCancelFormat );

	static WCHAR ProgressText[512];

	DWORD NumSectors = ( DWORD( pSource->PhysicalDiskSize ) + 511 ) / 512;

	DWORD steps	= (NumSectors / 512) + 2;

	BYTE blank[512];

	int Stages = 2;

	if ( FT & FTF_Blank ) {
		Stages = 3;

		//	Blank sectors
		memset(blank, 0, 512);

		int	s = 0;

		for ( DWORD i=0; i<NumSectors; i++) {
			if ( pSource->WriteSectorLBA( s, blank, 512 ) != DS_SUCCESS )
			{
				return -1;
			}

			if ((i / 512) != s) {
				s = i / 512;

				wsprintf( ProgressText, L"Formatting sector %d of %d", s, steps );

				::PostMessage( hWnd, WM_FORMATPROGRESS, Percent(0, Stages, s, steps, false), (LPARAM) ProgressText );
			}

			Sleep(0);
		}
	}

	//	Now write the structure.
	//	The free block list will start from sector 2 in any case. Sector 0 is the superblock, and sector 1 holds the
	//	directory, which will be empty.

	//	First the superblock:
	wsprintf( ProgressText, L"Writing SuperBlock");
	::PostMessage( hWnd, WM_FORMATPROGRESS, Percent(Stages - 2, Stages, 1, 1, false), (LPARAM) ProgressText );

	memset(blank, 0, 512);

	* (DWORD *) &blank[0]	= 2;
	sprintf_s((char *) &blank[4], 16, "adfs 1.0");

	if ( pSource->WriteSectorLBA(0, blank, 512) != DS_SUCCESS )
	{
		return -1;
	}

	//	Now the blank directory. Since it is the root, we can just fill it with zeroes to indicate no entries.
	wsprintf( ProgressText, L"Writing Root Directory");
	::PostMessage( hWnd, WM_FORMATPROGRESS, Percent(Stages - 2, Stages, 1, 1, false), (LPARAM) ProgressText );

	memset(blank, 0, 512);
	pSource->WriteSectorLBA(1, blank, 512);

	//	Update progress bar
	wsprintf( ProgressText, L"Writing Free Block List");
	::PostMessage( hWnd, WM_FORMATPROGRESS, Percent(Stages - 2, Stages, 1, 1, false), (LPARAM) ProgressText );

	//	Now the free block list. This is however many 127-entry sectors are required to store the number of sectors.
	//	The first free block is the block after the end of the list.

	DWORD FBL_Sectors = NumSectors / 127;

	if (NumSectors % 127)
		FBL_Sectors++;

	DWORD Sect = 2 + FBL_Sectors;
	DWORD FBS  = 2;
	DWORD FBC  = 1;

	DWORD *FBL = (DWORD *) blank;
	int	   FBP = 0;

	while (1) {
		if (FBP == 127) {
			if ((Sect < NumSectors) && (FBC < FBL_Sectors)) {
				FBL[127]	= FBS + 1;

				if ( pSource->WriteSectorLBA( FBS, blank, 512 ) != DS_SUCCESS )
				{
					return -1;
				}

				FBP	= 0;

				memset(blank, 0, 512);

				FBS++;
				FBC++;
			} else
				break;
		}

		FBL[FBP]	= Sect;

		FBP++;
		Sect++;

		if ( ( FBC %  256 ) == 0 )
		{
			wsprintf( ProgressText, L"Writing Free Block List: %d / %d", FBC, FBL_Sectors );
			::PostMessage( hWnd, WM_FORMATPROGRESS, Percent(Stages - 1, Stages, FBC, FBL_Sectors, false), (LPARAM) ProgressText );
		}

		if ((Sect >= NumSectors) || (FBC >= FBL_Sectors)) {
			if ( pSource->WriteSectorLBA( FBS, blank, 512 ) != DS_SUCCESS )
			{
				return -1;
			}

			break;
		}

		if ( WaitForSingleObject( hCancelFormat, 0 ) == WAIT_OBJECT_0 )
		{
			::PostMessage( hWnd, WM_FORMATPROGRESS, Percent(Stages - 1, Stages, FBC, FBL_Sectors, false), (LPARAM) L"Format canceled - filesystem probably unreadable!" );

			break;
		}

		Sleep(0);
	}

	//	Update progress bar
	wsprintf( ProgressText, L"Format Complete");
	::PostMessage( hWnd, WM_FORMATPROGRESS, 100, (LPARAM) ProgressText );

	return 0;
}

FSHint IECATAFileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FS_Null;

	unsigned char SectorBuf[512];

	if ( pSource->ReadSectorLBA(0, SectorBuf, 512) != DS_SUCCESS )
	{
		return hint;
	}

	if (strncmp((char *) &SectorBuf[4], (char *) "adfs 1.0", 8) == 0)	//	For some reason, IECATA identifies itself as "adfs 1.0"
	{
		hint.Confidence = 20;
		hint.FSID       = FSID_IECATA;
	}

	return hint;
}

int IECATAFileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	static FSSpace Map;

	BYTE Sector[ 512 ];

	Map.Capacity  = (DWORD) pSource->PhysicalDiskSize;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = 0;

	DWORD NumBlocks = (DWORD) ( pSource->PhysicalDiskSize / 512 );

	double BlockRatio = ( (double) NumBlocks / (double) TotalBlocks );

	if ( BlockRatio < 1.0 ) { BlockRatio = 1.0 ; }
	
	/* Get FBL pointer from SuperBlock */
	if ( pSource->ReadSectorLBA( 0, Sector, 512 ) != DS_SUCCESS )
	{
		return -1;
	}

	DWORD FBLPtr = * (DWORD *) &Sector[ 0 ];
	DWORD Blocks = 0;

	Map.UsedBytes = Map.Capacity;

	memset( pBlockMap, BlockAbsent, TotalBlocks );
	memset( pBlockMap, BlockUsed, (DWORD) ( NumBlocks / BlockRatio ) );

	DWORD BlkNum;

	pBlockMap[ 0 ] = (BYTE) BlockFixed;

	BlkNum = (DWORD) ( 1.0 / BlockRatio );

	pBlockMap[ BlkNum ] = (BYTE) BlockFixed;

	while ( 1 )
	{
		BlkNum = (DWORD) ( (double) FBLPtr / BlockRatio );

		if ( pBlockMap[ BlkNum ] != BlockFixed )
		{
			pBlockMap[ BlkNum ] = BlockUsed;
		}

		if ( pSource->ReadSectorLBA( FBLPtr, Sector, 512 ) != DS_SUCCESS )
		{
			return -1;
		}

		for ( int b=0; b<127; b++ )
		{
			DWORD FB = * (DWORD *) &Sector[ b * 4 ];

			if ( FB != 0 )
			{
				Blocks++;

				Map.UsedBytes -= 512;

				BlkNum = (DWORD) ( (double) FB / BlockRatio );

				if ( pBlockMap[ BlkNum ] != BlockFixed )
				{
					pBlockMap[ BlkNum ] = BlockFree;
				}

				if ( ( Blocks % 1000 ) == 0 )
				{
					SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );
					SendMessage( hBlockWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );
				}
			}
		}

		FBLPtr = * (DWORD *) &Sector[ 127 * 4 ];

		if ( FBLPtr == 0 )
		{
			SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

			return 0;
		}
	}

	return 0;
}

std::vector<AttrDesc> IECATAFileSystem::GetAttributeDescriptions( NativeFile *pFile )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Start sector. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex;
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

	opt.Name            = L"Program (PRG)";
	opt.EquivalentValue = 0x00000002;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Sequential (SEQ)";
	opt.EquivalentValue = 0x00000001;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	Attrs.push_back( Attr );
		

	return Attrs;
}

int IECATAFileSystem::MakeASCIIFilename( NativeFile *pFile )
{
	MakeASCII( pFile );

	return 0;
}

WCHAR *IECATAFileSystem::Identify( DWORD FileID )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	const WCHAR *ftypes[6] = { L"Deleted File Entry", L"Sequential Access File", L"Program", L"User File", L"Relative Access File", L"State Snapshot" };

	return (WCHAR *) ftypes[ pFile->Attributes[ 1 ] ];
}