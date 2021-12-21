#include "StdAfx.h"
#include "AcornDLL.h"
#include "AcornDFSFileSystem.h"
#include "../NUTS/TempFile.h"
#include "../NUTS/Preference.h"

#include "resource.h"

#include <sstream>
#include <iterator>
#include <algorithm>

int	AcornDFSFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	NativeFile *file = &pDirectory->Files[FileID];

	int	Sectors	= ( file->Length & 0xffffff00 ) >> 8;

	if ( file->Length & 0xFF )
		Sectors++;

	BYTE Buffer[256];

	DWORD fileSize = (DWORD) file->Length;

	BYTE Track  = file->Attributes[ 0 ] / 10;
	BYTE Sector = file->Attributes[ 0 ] % 10;

	while (Sectors) {
		pSource->ReadSectorCHS( 0, Track, Sector, Buffer );

		if ( fileSize > 256U )
		{
			store.Write( Buffer, 256U );
		}
		else
		{
			store.Write( Buffer, fileSize );
		}

		Sector++;

		if ( Sector == 10 ) { Sector = 0; Track++; }
		
		Sectors--;
		fileSize -= 256;
	}

	return 0;
}

int AcornDFSFileSystem::ReplaceFile(NativeFile *pFile, CTempFile &store)
{
	NativeFileIterator iFile;

	/* Delete the original file */
	for ( iFile = pDFSDirectory->RealFiles.begin(); iFile != pDFSDirectory->RealFiles.end(); iFile++ )
	{
		if ( ( rstrnicmp( pFile->Filename, iFile->Filename, 7 ) ) && ( iFile->AttrPrefix == pFile->AttrPrefix ) ) /* DFS limits files to 7 chars */
		{
			DeleteFile( iFile->fileID );

			break;
		}
	}

	/* Write the new data */
	pFile->Length = store.Ext();

	WriteFile( pFile, store );

	/* Update the reference */
	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( ( rstrnicmp( pFile->Filename, iFile->Filename, 7 ) ) && ( iFile->AttrPrefix == pFile->AttrPrefix ) ) /* DFS limits files to 7 chars */
		{
			*pFile = *iFile;

			break;
		}
	}

	return 0;
}


int	AcornDFSFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	if ( Override )
	{
		pFile = &OverrideFile;

		Override = false;
	}

	if ( ( pFile->EncodingID != ENCODING_ASCII ) && ( pFile->EncodingID != ENCODING_ACORN ) && ( pFile->EncodingID != ENCODING_RISCOS ) )
	{
		return FILEOP_NEEDS_ASCII;
	}

	if ( pDFSDirectory->RealFiles.size() >= 31 )
	{
		return NUTSError( 0x80, L"Directory Full" );
	}

	bool Respect = Preference( L"SidecarPaths", true );

	/* Check we don't already have this */
	NativeFileIterator iFile;

	for ( iFile = pDFSDirectory->RealFiles.begin(); iFile != pDFSDirectory->RealFiles.end(); iFile++ )
	{
		if ( ( rstrnicmp( pFile->Filename, iFile->Filename, 7 ) ) && ( iFile->AttrPrefix == pFile->AttrPrefix ) ) /* DFS limits files to 7 chars */
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

	/* Now find a space in the map big enough for the incoming file */
	DWORD NSectors = pFile->Length / 256;

	if ( pFile->Length % 256 ) { NSectors++; }

	if ( NSectors > ( pDFSDirectory->NumSectors - 2 ) )
	{
		/* 2 sectors for the catalogue */
		return NUTSError( 0x35, L"File too large for this image" );
	}

	DWORD Sector = 0;

	for ( WORD n = 0; n <= (pDFSDirectory->NumSectors - NSectors ); n++ )
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
		file            = *pFile;
		file.AttrPrefix = pFile->AttrPrefix;
	}
	else if ( pFile->FSFileType == FT_ACORNX )
	{
		file            = *pFile;
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

	if ( ( pFile->FSFileType != FT_ACORN ) || ( !Respect ) )
	{
		file.AttrPrefix = pDFSDirectory->CurrentDir;
	}

	file.SSector  = Sector;

	/* Write the sectors out */
	BYTE SectorBuf[ 256 ];

	DWORD CSector = Sector % 10;
	DWORD CTrack  = Sector / 10;
	DWORD Bytes   = pFile->Length;

	store.Seek( 0 );

	while ( Bytes > 0 )
	{
		DWORD BytesToGo = Bytes;

		if ( BytesToGo > 256 )
		{
			BytesToGo = 256;
		}

		store.Read( SectorBuf, BytesToGo );

		pSource->WriteSectorCHS( 0, CTrack, CSector, SectorBuf );

		Bytes -= BytesToGo;

		CSector++;

		if ( CSector == 10 ) { CSector = 0; CTrack++; }
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

int	AcornDFSFileSystem::CreateDirectory( NativeFile *pDir, DWORD CreateFlags ) {

	if ( ( pDir->EncodingID != ENCODING_ASCII ) && ( pDir->EncodingID != ENCODING_ACORN ) && ( pDir->EncodingID != ENCODING_RISCOS )  )
	{
		return FILEOP_NEEDS_ASCII;
	}

	pDFSDirectory->ExtraDirectories[ pDir->Filename[ 0 ] ] = true;

	int r = 0;

	if ( CreateFlags & CDF_ENTER_AFTER )
	{
		pDFSDirectory->CurrentDir = pDir->Filename[ 0 ];
	}
	
	r = pDirectory->ReadDirectory();

	return r;
}

BYTE *AcornDFSFileSystem::DescribeFile(DWORD FileIndex) {
	static BYTE status[8192];

	NativeFile	*pFile	= &pDirectory->Files[FileIndex];

	if ( pFile->Flags & FF_Directory ) 
	{
		rsprintf( status, "Directory Prefix" );
	}
	else
	{
		rsprintf( status, "[%s] %06X bytes, &%08X/&%08X",
			(pFile->AttrLocked)?"L":"-",
			(QWORD) pFile->Length, pFile->LoadAddr, pFile->ExecAddr
		);
	}
		
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

		if ( pFile->Flags & FF_Directory )
		{
			rsprintf( status, "%s | Directory Prefix", (BYTE *) pFile->Filename );
		}
		else
		{
			rsprintf( status,
				"%s | [%s] - %0X bytes - Load: &%08X Exec: &%08X",
				(BYTE *) pFile->Filename, (pFile->AttrLocked)?"L":"-",
				(QWORD) pFile->Length, pFile->LoadAddr, pFile->ExecAddr
			);
		}
	}
		
	return status;
}

FSHint AcornDFSFileSystem::Offer( BYTE *Extension )
{
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

	if ( pSource->ReadSectorCHS( 0, 0, 0, &SectorBuf[ 000 ] ) != DS_SUCCESS )
	{
		return hint;
	}

	if ( pSource->ReadSectorCHS( 0, 0, 1, &SectorBuf[ 256 ] ) != DS_SUCCESS )
	{
		return hint;
	}

	DWORD sectors = ((SectorBuf[256 + 6 + 0] & 3) << 8) | SectorBuf[256 + 6 + 1];

	if ( ( sectors == 400 ) && ( MYFSID == FSID_DFS_40 ) )
	{
		hint.Confidence += 20;
	}

	if ( ( sectors == 800 ) && ( MYFSID == FSID_DFS_80 ) )
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

	pSource->ReadSectorCHS( 0, 0, 0, &Sector[ 000 ] );
	pSource->ReadSectorCHS( 0, 0, 1, &Sector[ 256 ] );

	DWORD NumBlocks = ((Sector[256 + 6 + 0] & 3) << 8) | Sector[256 + 6 + 1];

	Map.Capacity  = NumBlocks * 256;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = 512;

	if ( pDFSDirectory->NumSectors > ( 80 * 10 ) )
	{
		/* Obviously a corrupt file. Don't even try */
		SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

		return NUTSError( 0x38, L"Corrupt image" );
	}

	std::vector<NativeFile>::iterator iFile;

	double BlockRatio = ( (double) NumBlocks / (double) TotalBlocks );

	if ( BlockRatio < 1.0 ) { BlockRatio = 1.0 ; }
	
	memset( pBlockMap, BlockAbsent, TotalBlocks );
	memset( pBlockMap, BlockFree, (DWORD) ( NumBlocks / BlockRatio ) );

	DWORD BlkNum;

	for ( DWORD FixedBlk=0; FixedBlk != 2; FixedBlk++ )
	{
		BlkNum = (DWORD) ( (double) FixedBlk / BlockRatio );
	
		if ( BlkNum < TotalBlocks )
		{
			pBlockMap[ BlkNum ] = (BYTE) BlockFixed;
		}
	}

	if ( pDirectory != nullptr )
	{
		for ( iFile = pDFSDirectory->RealFiles.begin(); iFile != pDFSDirectory->RealFiles.end(); iFile++ )
		{
			if ( iFile->Length > ( pDFSDirectory->NumSectors * 256 ) )
			{
				continue;
			}

			DWORD UBlocks = (DWORD) ( iFile->Length / 256 );
			
			if ( iFile->Length % 256 )
			{
				UBlocks++;
			}

			Map.UsedBytes += UBlocks * 256;

			for ( DWORD Blk = iFile->SSector; Blk != iFile->SSector + UBlocks; Blk++ )
			{
				BlkNum = (DWORD) ( (double) Blk / BlockRatio );

				if ( BlkNum < TotalBlocks )
				{
					if ( pBlockMap[ BlkNum ] != BlockFixed )
					{
						pBlockMap[ BlkNum ] = BlockUsed;
					}
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

AttrDescriptors AcornDFSFileSystem::GetAttributeDescriptions( NativeFile *pFile )
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

int AcornDFSFileSystem::Format_Process( DWORD FT, HWND hWnd ) {

	int Stages = 1;

	DWORD MaxTracks = 40;

	if ( MYFSID == FSID_DFS_80 )
	{
		MaxTracks = 80;
	}

	if ( FT & FTF_Blank )
	{
		Stages++;

		BYTE SectorBuf[ 256 ];

		ZeroMemory( SectorBuf, 256 );

		for ( WORD t=0; t < MaxTracks; t++ )
		{
			for ( WORD s=0; s < 10; s++ )
			{
				if ( WaitForSingleObject( hCancelFormat, 0 ) == WAIT_OBJECT_0 )
				{
					return 0;
				}

				pSource->WriteSectorCHS( 0, t, s, SectorBuf );

				PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 0, Stages, ( t * 10 ) + s, MaxTracks * 10, false ), 0 );
			}
		}
	}

	pDFSDirectory->RealFiles.clear();
	pDFSDirectory->NumSectors = MaxTracks * 10;
	pDFSDirectory->MasterSeq  = 0;
	pDFSDirectory->Option     = 0;

	memset( pDFSDirectory->DiscTitle, 0x20, 12 );

	pDirectory->WriteDirectory();

	PostMessage( hWnd, WM_FORMATPROGRESS, 100, 0);

	return 0;
}


int AcornDFSFileSystem::DeleteFile( DWORD FileID )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	if ( pFile->Flags & FF_Directory ) 
	{
		BYTE Prefix = pFile->Filename[ 0 ];

		std::map<BYTE,bool>::iterator iDir = pDFSDirectory->ExtraDirectories.find( Prefix );

		if ( iDir != pDFSDirectory->ExtraDirectories.end() )
		{
			pDFSDirectory->ExtraDirectories.erase( iDir );
		}

		/* Nothing to write as these virtual directories don't technically exist */
		int r = pDirectory->ReadDirectory();

		return r;
	}

	/* This is easy, we literally just removing the offending file from the directory, and save */
	NativeFileIterator iFile;

	for ( iFile = pDFSDirectory->RealFiles.begin(); iFile != pDFSDirectory->RealFiles.end(); iFile++ )
	{
		if ( ( rstrnicmp( iFile->Filename, pFile->Filename, 7 ) ) && ( iFile->AttrPrefix == pFile->AttrPrefix ) )
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

int AcornDFSFileSystem::ExportSidecar( NativeFile *pFile, SidecarExport &sidecar )
{
	bool Respect = Preference( L"SidecarPaths", true );

	BYTE tf[ 32 ];

	rstrncpy( tf, pFile->Filename, 16 );
	rstrncat( tf, (BYTE *) ".INF", 256 );

	sidecar.Filename = tf;

	BYTE INFData[80];

	char Dir = '$';

	if ( Respect )
	{
		Dir = pDFSDirectory->CurrentDir;
	}

	rsprintf( INFData, "%c.%s %06X %06X %s\n", Dir, (BYTE *) pFile->Filename, pFile->LoadAddr & 0xFFFFFF, pFile->ExecAddr & 0xFFFFFF, ( pFile->AttrLocked ) ? "Locked" : "" );

	CTempFile *pTemp = (CTempFile *) sidecar.FileObj;

	pTemp->Seek( 0 );
	pTemp->Write( INFData, rstrnlen( INFData, 80 ) );

	return 0;
}

int AcornDFSFileSystem::ImportSidecar( NativeFile *pFile, SidecarImport &sidecar, CTempFile *obj )
{
	int r = 0;

	if ( obj == nullptr )
	{
		BYTE tf[32];
		rstrncpy( tf, pFile->Filename, 16 );
		rstrncat( tf, (BYTE *) ".INF", 256 );

		sidecar.Filename = tf;
	}
	else
	{
		bool Respect = Preference( L"SidecarPaths", true );

		BYTE bINFData[ 256 ];

		ZeroMemory( bINFData, 256 );

		obj->Seek( 0 );
		obj->Read( bINFData, min( 256, obj->Ext() ) );

		std::string INFData( (char *) bINFData );

		std::istringstream iINFData( INFData );

		std::vector<std::string> parts((std::istream_iterator<std::string>(iINFData)), std::istream_iterator<std::string>());

		/* Need at least 3 parts */
		if ( parts.size() < 3 )
		{
			return 0;
		}

		OverrideFile = *pFile;

		OverrideFile.AttrPrefix = pDFSDirectory->CurrentDir;

		try
		{
			/* Part deux is the load address. We'll use std::stoull */
			OverrideFile.LoadAddr = std::stoul( parts[ 1 ], nullptr, 16 );
			OverrideFile.LoadAddr &= 0xFFFFFF;

			/* Part the third is the exec address. Same again. */
			OverrideFile.ExecAddr = std::stoul( parts[ 2 ], nullptr, 16 );
			OverrideFile.ExecAddr &= 0xFFFFFF;
		}

		catch ( std::exception &e )
		{
			/* eh-oh */
			return NUTSError( 0x2E, L"Bad sidecar file" );
		}

		/* Fix the standard attrs */
		OverrideFile.AttrLocked = 0x00000000;

		/* Look through the remaining parts for "L" or "Locked" */
		for ( int i=3; i<parts.size(); i++ )
		{
			if ( ( parts[ i ] == "L" ) || ( parts[ i ] == "Locked" ) )
			{
				OverrideFile.AttrLocked = 0xFFFFFFFF;
			}
		}

		OverrideFile.Flags = 0;
		
		/*  Copy the filename - but we must remove the leading directory prefix*/
		if ( parts[ 0 ].substr( 1, 1 ) == "." )
		{
			OverrideFile.Filename = (BYTE *) parts[ 0 ].substr( 2 ).c_str();

			if ( Respect )
			{
				OverrideFile.AttrPrefix = parts[ 0 ].at( 0 );
			}
		}
		else
		{
			OverrideFile.Filename = (BYTE *) parts[ 0 ].c_str();
		}

		OverrideFile.FSFileType = FT_ACORN;

		Override     = true;
	}

	return r;
}

int AcornDFSFileSystem::SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal )
{
	int r = 0;

	if ( PropID == 0 )
	{
		rstrncpy( pDFSDirectory->DiscTitle, pNewVal, 12 );

		/* Fix up the title string - rstrncpy puts a terminator on the end - we don't want that */
		bool Done =false;

		for ( BYTE i=0; i<12; i++ )
		{
			if ( ( pDFSDirectory->DiscTitle[ i ] == 0 ) || ( Done ) )
			{
				pDFSDirectory->DiscTitle[ i ] = 0x20;

				Done = true;
			}
		}

		r = pDirectory->WriteDirectory();

		if ( r == DS_SUCCESS )
		{
			r = pDirectory->ReadDirectory();
		}
	}

	if ( PropID == 1 )
	{
		pDFSDirectory->Option = NewVal & 3;

		r = pDirectory->WriteDirectory();

		if ( r == DS_SUCCESS )
		{
			r = pDirectory->ReadDirectory();
		}
	}

	return r;
}

FSToolList AcornDFSFileSystem::GetToolsList( void )
{
	FSToolList tools;

	FSTool tool;

	tool.ToolIcon = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_COMPACT ) );
	tool.ToolName = L"Compact";
	tool.ToolDesc = L"Move files from used sectors together so that free space is one contiguous block";

	tools.push_back( tool );

	return tools;
}

bool DFSSectorSort( NativeFile &a, NativeFile &b )
{
	if ( a.SSector < b.SSector )
	{
		return true;
	}
	
	return false;
}

int AcornDFSFileSystem::RunTool( BYTE ToolNum, HWND ProgressWnd )
{
	::SendMessage( ProgressWnd, WM_FSTOOL_CURRENTOP, 0, (LPARAM) L"Compacting image" );

	/* First, we'll need a list of RealFiles sorted by Start Sector */
	std::vector<NativeFile> SortedFiles = pDFSDirectory->RealFiles;

	std::sort( SortedFiles.begin(), SortedFiles.end(), DFSSectorSort );

	/* Now build up a map of sectors, as per WriteFile */

	/* No DFS disk has more than 800 sectors */
	BYTE Occupied[ 800 ];

	ZeroMemory( Occupied, 800 );

	Occupied[ 0 ] = 0xFF;
	Occupied[ 1 ] = 0xFF;

	NativeFileIterator iFile;

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

	/* Now here's how we'll do this. Proceed through the block map until an unused block is found.
	   Then find the file in the SortedFiles vector whose start sector is greather than the unused
	   block. Perform the move sector-by-sector, and record the new start sector in RealFiles.

	   Each source block is marked as free. Each destination block marked as occupied.

	   Once all the moves are done, write the directory back. Because we search a sorted list, it
	   is always the file nearest the start of the free space that gets moved, and we'll never
	   overwrite any other data. */

	WORD s = 2; /* Directory is 2 sectors */

	while ( s < pDFSDirectory->NumSectors )
	{
		if ( WaitForSingleObject( hToolEvent, 0 ) == WAIT_OBJECT_0 )
		{
			return -1;
		}

		if ( Occupied[ s ] == 0 )
		{
			/* Empty space - find a file */
			for ( iFile = SortedFiles.begin(); iFile != SortedFiles.end(); )
			{
				if ( iFile->SSector >= s )
				{
					/* This file will do. We now need to locate it in the RealFiles vector, as this
					   is what gets written back */
					NativeFileIterator realFile;

					for ( realFile = pDFSDirectory->RealFiles.begin(); realFile != pDFSDirectory->RealFiles.end(); )
					{
						if ( realFile->SSector == iFile->SSector )
						{
							/* Found it. Start moving */
							WORD NSectors = iFile->Length / 256;

							if ( iFile->Length % 256 ) { NSectors++; }

							BYTE SectorBuf[ 256 ];
							
							BYTE SrcTrack  = iFile->SSector / 10;
							BYTE SrcSector = iFile->SSector % 10;
							BYTE DstTrack  = s / 10;
							BYTE DstSector = s % 10;

							for ( WORD n = 0; n < NSectors; n++ )
							{
								if ( pSource->ReadSectorCHS( 0, SrcTrack, SrcSector, SectorBuf ) != DS_SUCCESS )
								{
									return -1;
								}
								
								if ( pSource->WriteSectorCHS( 0, DstTrack, DstSector, SectorBuf ) != DS_SUCCESS )
								{
									return -1;
								}

								Occupied[ iFile->SSector + n ] = 0x00;
								Occupied[ s + n ]              = 0xFF;

								SrcSector++;

								if ( SrcSector == 10 ) { SrcSector = 0; SrcTrack++; }

								DstSector++;

								if ( DstSector == 10 ) { DstSector = 0; DstTrack++; }
							}

							realFile->SSector = s;

							realFile = pDFSDirectory->RealFiles.end();
							iFile    = SortedFiles.end();
						}
						else
						{
							realFile++;
						}
					}
				}

				if ( iFile != SortedFiles.end() )
				{
					iFile++;
				}
			}
		}

		s++;

		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 0, 1, s, pDFSDirectory->NumSectors, false), 0 );
	}

	/* Write the directory back now */
	int r = pDirectory->WriteDirectory();

	if ( r == DS_SUCCESS )
	{
		r = pDirectory->ReadDirectory();
	}

	MessageBox( ProgressWnd, L"Compaction complete.", L"NUTS Acorn DFS File System", MB_ICONINFORMATION | MB_OK );

	::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, 100, 0 );
	
	return r;
}

int AcornDFSFileSystem::Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  )
{
	int r = 0;

	/* Find the file this really refers to */
	NativeFileIterator iFile;

	for ( iFile = pDFSDirectory->RealFiles.begin(); iFile != pDFSDirectory->RealFiles.end(); iFile++ )
	{
		if ( iFile->SSector == pDirectory->Files[ FileID ].SSector )
		{
			iFile->Filename = BYTEString( NewName, 7 );

			r = pDirectory->WriteDirectory();

			if ( r == DS_SUCCESS )
			{
				r = pDirectory->ReadDirectory();
			}

			return r;
		}
	}

	return r;
}

