#include "StdAfx.h"

#include "CPM.h"

#include "../NUTS/Preference.h"

#include "libfuncs.h"



int CPMFileSystem::ReadRawBySectors( QWORD Offset, DWORD Length, BYTE *Buffer )
{
	DWORD BytesToGo    = Length;
	QWORD SectorOffset = Offset;
	DWORD BufferOffset = 0;

	while ( BytesToGo > 0 )
	{
		int r = pSource->ReadRaw( SectorOffset, dpb.SecSize, &Buffer[ BufferOffset ] );

		BytesToGo    -= min( BytesToGo, dpb.SecSize );
		SectorOffset += dpb.SecSize;
		BufferOffset += dpb.SecSize;

		if ( r != DS_SUCCESS )
		{
			return r;
		}
	}

	return 0;
}

int CPMFileSystem::WriteRawBySectors( QWORD Offset, DWORD Length, BYTE *Buffer )
{
	DWORD BytesToGo    = Length;
	QWORD SectorOffset = Offset;
	DWORD BufferOffset = 0;

	while ( BytesToGo > 0 )
	{
		int r = pSource->WriteRaw( SectorOffset, dpb.SecSize, &Buffer[ BufferOffset ] );

		BytesToGo    -= min( BytesToGo, dpb.SecSize );
		SectorOffset += dpb.SecSize;
		BufferOffset += dpb.SecSize;

		if ( r != DS_SUCCESS )
		{
			return r;
		}
	}

	return 0;
}

BYTE *CPMFileSystem::GetTitleString( NativeFile *pFile )
{
	/* No directories, so this one is easy */
	static BYTE Title[ 256 ];

	if ( pFile == nullptr )
	{
		rsprintf( Title, "%s::/", dpb.ShortFSName );
	}
	else
	{
		rsprintf( Title, "%s::/%s.%s > ", dpb.ShortFSName, pFile->Filename, pFile->Extension );
	}

	return Title;
}

BYTE *CPMFileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE Descript[ 256 ];

	if ( rstrnicmp( pDirectory->Files[ FileIndex ].Extension, (BYTE *) "BAS", 3 ) )
	{
		rsprintf( Descript, "BASIC File, %d bytes", pDirectory->Files[ FileIndex ].Length );
	}
	else if ( rstrnicmp( pDirectory->Files[ FileIndex ].Extension, (BYTE *) "BIN", 3 ) )
	{
		rsprintf( Descript, "Binary File, %d bytes", pDirectory->Files[ FileIndex ].Length );
	}
	else if ( rstrnicmp( pDirectory->Files[ FileIndex ].Extension, (BYTE *) "TXT", 3 ) )
	{
		rsprintf( Descript, "Text File, %d bytes", pDirectory->Files[ FileIndex ].Length );
	}
	else
	{
		rsprintf( Descript, "File, %d bytes", pDirectory->Files[ FileIndex ].Length );
	}

	return Descript;
}

BYTE *CPMFileSystem::GetStatusString( int FileIndex, int SelectedItems )
{
	static BYTE Status[ 256 ];

	if ( SelectedItems == 0 )
	{
		rsprintf( Status, "%d Files", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( Status, "%d Files Selected", SelectedItems );
	}
	else
	{
		NativeFile *pFile = &pDirectory->Files[ FileIndex ];

		if ( rstrnicmp( pFile->Extension, (BYTE *) "BAS", 3 ) )
		{
			rsprintf( Status, "%s.%s, BASIC File, %d bytes", pFile->Filename, pFile->Extension, pFile->Length );
		}
		else if ( rstrnicmp( pFile->Extension, (BYTE *) "BIN", 3 ) )
		{
			rsprintf( Status, "%s.%s, Binary File, %d bytes", pFile->Filename, pFile->Extension, pFile->Length );
		}
		else if ( rstrnicmp(pFile->Extension, (BYTE *) "TXT", 3 ) )
		{
			rsprintf( Status, "%s.%s, Text File, %d bytes", pFile->Filename, pFile->Extension, pFile->Length );
		}
		else
		{
			rsprintf( Status, "%s.%s, File, %d bytes", pFile->Filename, pFile->Extension, pFile->Length );
		}
	}

	return Status;
}

int CPMFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	/* CP/M's format is very screwy here. Basically, the file is split into <=16K chunks, each
	   chunk having it's own directory entry. Each chunk is then sub-split into <=16 blocks of
	   1K each.

	   Since we have just one file, the attribute 0 locates the offset into the directory of
	   the first chunk of the file (extent 0). We need to find each file entry (starting with
	   extent 0) and keep going until we run out of extents.
	*/

	/* If the source is slow (e.g. actual floppy disk) and the user elected to disable slow
	   file enhancement, then we should enhance the file here.
	*/
	bool SlowEnhance = (bool) Preference( L"SlowEnhance", false );

	if ( ( pSource->Flags & DS_SlowAccess ) && ( !SlowEnhance ) && ( dpb.Flags & CF_UseHeaders ) )
	{
		pDir->ExtraReadDirectory( &pDirectory->Files[ FileID ] );
	}

	DWORD SectorOffset = 0;

	if ( SystemDisk )
	{
		SectorOffset += dpb.SysTracks * dpb.SecsPerTrack * dpb.SecSize;
	}

	AutoBuffer Directory( dpb.DirSecs * dpb.SecSize );

	pDir->LoadDirectory( Directory );

	/* First byte is the entry type */
	BYTE *pRootName = &Directory[ pDirectory->Files[ FileID ].Attributes[ 0 ] + 1 ];

	WORD  SearchExtent = 0;
	DWORD Offset       = 0;
	bool  FileDone     = false;

	store.Seek( 0 );

	bool DoneHeader = false;

	DWORD FileSize = pDirectory->Files[ FileID ].Length;

	while ( !FileDone )
	{
		bool DidExtent = false;

		Offset = 0;

		while ( Offset <= ( dpb.DirSecs * dpb.SecSize ) )
		{
			BYTE *pEntry = &Directory[ Offset ];

			if ( pEntry[ 0 ] >= 32 ) { Offset += 32; continue; } // Not a llama.

			WORD ThisExtent = ( pEntry[ 0x0E ] << 8 ) + pEntry[ 0x0C ];

			if ( ( rstrncmp( &pEntry[ 1 ], pRootName, 0x0C ) ) && ( ThisExtent == SearchExtent ) )
			{
				DidExtent = true;

				/* Found the extent we're looking for */
				DWORD KBlocks = ( pEntry[ 0x0F ] * 0x80 ) + pEntry[ 0x0D ];

				if ( pEntry[ 0x0D ] == 0 ) { KBlocks += 0x80; }

				BYTE BlkNum = 0x10;

				AutoBuffer DataBuffer( dpb.ExtentSize );

				/* We must do this in 512 byte chunks because their may be a track information
				   block in the middle of our sectors */
				while ( KBlocks > 0 )
				{
					WORD ExtentID = pEntry[ BlkNum ];

					if ( dpb.Flags & CF_Force16bit )
					{
						ExtentID += ( (WORD) pEntry[ BlkNum + 1 ] * 256 );

						BlkNum++;
					}

					DWORD ThisK = min( dpb.ExtentSize, KBlocks );

					if ( ReadRawBySectors( SectorOffset + ( dpb.ExtentSize * ExtentID ), ThisK, DataBuffer ) != DS_SUCCESS )
					{
						return -1;
					}

					if ( ( !DoneHeader ) && ( dpb.HeaderSize > 0 ) && ( dpb.Flags & CF_UseHeaders ) )
					{
						if ( IncludeHeader( DataBuffer ) )
						{
							store.Write( DataBuffer, dpb.HeaderSize );

							FileSize -= dpb.HeaderSize;
						}
						else
						{
							ParseCPMHeader( &pDirectory->Files[ FileID ], DataBuffer );
						}

						store.Write( &DataBuffer[ dpb.HeaderSize ], min( ThisK - dpb.HeaderSize, FileSize ) );

						FileSize -= min( ThisK - dpb.HeaderSize, FileSize );
					}
					else
					{
						store.Write( DataBuffer, min( ThisK, FileSize ) );

						FileSize -= min( ThisK, FileSize );
					}

					DoneHeader = true;

					KBlocks -= ThisK;

					BlkNum++;

					if ( BlkNum >= 0x20 )
					{
						KBlocks = 0;

						break;
					}
				}
			}

			Offset += 32;
		}

		SearchExtent++;

		if ( !DidExtent )
		{
			FileDone = true; // Missing data - *shrug*
		}
	}

	return 0;
}

int CPMFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	/* Oh god the pain */

	/* Usual checks first */
	if ( ( pFile->EncodingID != ENCODING_ASCII ) && ( pFile->EncodingID != dpb.Encoding ) )
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

	DWORD SectorOffset = 0;

	if ( SystemDisk )
	{
		SectorOffset += dpb.SysTracks * dpb.SecsPerTrack * dpb.SecSize;
	}

	/* Take a copy */
	NativeFile file = *pFile;

	pFile = &file;

	CPMPreWriteCheck( &file );

	/* Because of the TOTALLY screwy way that CP/M directories work, we can't simply write the
	   data blobs, throw a directory entry in pDirectory->Files and then hope for the best with
	   WriteDirectory(). We'll actually have to reconstruct the directory ourselves here.

	   To do this, we first need to determine how many 1K blocks we need for the file (and if
	   the disk can supply enough). Then iterate the 2K directory and find an entry that is not
	   in use (type > 0x33 - on most disks this will be 0xE5), and fill in the entry with the
	   blocks for the file, writing the data as we go.

	   We'll need the filename in disk format (rather than SZ format) to fill in the extents.
	*/

	AutoBuffer Header( dpb.HeaderSize );

	bool WillWriteHeader = GetCPMHeader( &file, Header );

	/* Enough space for this? */
	DWORD FullSize     = ( pFile->Length + ( (WillWriteHeader)?dpb.HeaderSize:0 ) );
	DWORD NeededBlocks = FullSize / dpb.ExtentSize;

	if ( FullSize % dpb.ExtentSize ) { NeededBlocks++; }

	if ( pMap->FreeBlocks() < NeededBlocks )
	{
		return NUTSError( 0xB2, L"Disk full" );
	}

	/* We're on - we'll need the directory first */
	AutoBuffer Directory( dpb.DirSecs * dpb.SecSize );

	pDir->LoadDirectory( Directory );

	store.Seek( 0 );

	DWORD BytesToGo  = pFile->Length;
	BYTE  ExtentID   = 0;
	BYTE  ExtentSlot = 16;

	BYTE *pEntry;

	if ( pFile->FSFileType != dpb.FSFileType )
	{
		if ( !( pFile->Flags & FF_Extension ) )
		{
			pFile->Flags |= FF_Extension;

			rsprintf( pFile->Extension, (BYTE *) "BIN" );
		}

		pFile->Attributes[ 1 ] = 0;
		pFile->Attributes[ 2 ] = 0;
		pFile->Attributes[ 3 ] = 0;
	}

	AutoBuffer SectorBuf( dpb.ExtentSize );

	bool DoneHeader = false;

	while ( ( NeededBlocks > 0 ) || ( pFile->Length == 0 ) )
	{
		if ( ExtentSlot == 16 )
		{
			ExtentSlot = 0;

			/* Find a directory slot */
			DWORD DOffset = 0;

			/* Because each directory entry directly describes the blocks it uses, and the directory
			   is 2K in size, and no block may be used more than once, it is impossible for the
			   directory to contain no free slots. */

			while ( DOffset < ( dpb.DirSecs * dpb.SecSize ) )
			{
				pEntry = &Directory[ DOffset ];

				if ( pEntry[ 0 ] < 0x33 )
				{
					DOffset += 32;

					continue;
				}

				DOffset = dpb.DirSecs * dpb.SecSize;

				DWORD SlotBytes = min( 16384, BytesToGo );

				BYTE  Records   = (BYTE) ( SlotBytes / 128 );

				/* I've left the next line in for curiosity reasons. Ordinarily, I follow a pattern
				   of calculating the blocks needed by dividing the length by the block size, then
				   adding one if there are remaining bytes.

				   CP/M (or at least the incarnation used by AMSDOS) does not recognise "leftovers",
				   and rather naïvely assumes that there are at least 1 bytes in the last record. As
				   such the number of records read in is one higher than the value at pEntry[0x0F]
				   at all times, even if the size is an even number of records (as it always is with
				   AMSDOS). The reading code allows the value at pEntry[ 0x0D ] to be a bytes-in-last-record
				   value, but AMSDOS doesn't use it, so it is always zero, and thus 128.

				   And in fact, in that case, we actually need to *subtract* one.
				*/

				/* if ( SlotBytes % 0x80 ) { Records++; } */

				if ( ( SlotBytes % 0x80 ) != 0 ) { Records--; }

				/* Undo that if the file is 0-byte */
				if ( pFile->Length == 0 ) { Records = 0; }

				memset( pEntry, 0, 32 );

				ExportCPMDirectoryEntry( pFile, pEntry );

				pEntry[ 0x00 ] = 0;
				pEntry[ 0x0C ] = ExtentID;
				pEntry[ 0x0F ] = Records;

				ExtentID++;

				BytesToGo -= SlotBytes;
			}
		}

		/* This allows writing 0-byte files */
		if ( pFile->Length > 0 )
		{
			/* Write out the blocks in this slot */
			DWORD Block = pMap->GetFreeBlock();

			pMap->SetBlock( Block );

			pEntry[ 0x10 + ExtentSlot ] = (BYTE) ( Block & 0xFF );

			if ( dpb.Flags & CF_Force16bit )
			{
				pEntry[ ExtentSlot + 1 ] = (BYTE) ( ( Block & 0xFF00 ) >> 8 );

				ExtentSlot++;
			}

			pEntry[ 0x10 + ExtentSlot ] = (BYTE) Block;

			if ( ( !DoneHeader ) && ( dpb.HeaderSize > 0 ) && ( WillWriteHeader ) )
			{
				memcpy( SectorBuf, Header, dpb.HeaderSize );

				store.Read( &SectorBuf[ dpb.HeaderSize ], dpb.ExtentSize - dpb.HeaderSize );
			}
			else
			{
				store.Read( SectorBuf, dpb.ExtentSize );
			}

			DoneHeader = true;

			WriteRawBySectors( SectorOffset + ( Block * dpb.ExtentSize ), dpb.ExtentSize, SectorBuf );
		}
		else
		{
			pEntry[0x10] = (BYTE) pMap->GetFreeBlock(); // Better than nothing I guess?
		}

		ExtentSlot++;

		if ( pFile->Length == 0 )
		{
			break;
		}

		NeededBlocks--;
	}
	
	/* Write the updated directory back */
	pDir->SaveDirectory( Directory );

	return pDirectory->ReadDirectory();
}

int CPMFileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	ResetEvent( hCancelFree );

	static FSSpace Map;

	/* Need to account for system tracks */
	DWORD st = (DWORD) dpb.SysTracks * (DWORD) dpb.SecsPerTrack;

	if ( !SystemDisk )
	{
		st = 0;
	}

	DWORD NumBlocks = pSource->PhysicalDiskSize / dpb.SecSize;

	Map.Capacity  = NumBlocks * dpb.SecSize;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = 0;

	double BlockRatio = ( (double) NumBlocks / (double) TotalBlocks );

	if ( BlockRatio < 1.0 ) { BlockRatio = 1.0 ; }
	
	memset( pBlockMap, BlockAbsent, TotalBlocks );
	memset( pBlockMap, BlockFree, (DWORD) ( NumBlocks / BlockRatio ) );

	DWORD BlkNum;

	SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

	AutoBuffer Directory( dpb.DirSecs * dpb.SecSize );

	pDir->LoadDirectory( Directory );

	DWORD Offset = 0;

	while ( Offset < ( dpb.SecSize * dpb.DirSecs ) )
	{
		if ( WaitForSingleObject( hCancelFree, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		BYTE *pEntry = &Directory[ Offset ];

		if ( pEntry[ 0 ] >= 32 )
		{
			Offset += 32;

			continue;
		}

		/* There are 16 bytes of used 1K blocks. But not all of them are used */
		WORD ExtentSize = pEntry[ 0x0F ] * 0x80;

		WORD MaxExtentBlock = ExtentSize / dpb.ExtentSize;

		if ( ExtentSize % dpb.ExtentSize ) { MaxExtentBlock++; }

		for ( BYTE i=0; i<(BYTE) MaxExtentBlock; i++ )
		{
			DWORD KBlock = pEntry[ 0x10 + i ];

			DWORD KO = 0;

			while ( KO < dpb.ExtentSize )
			{
				DWORD SecNum = st + ( ( ( KBlock * dpb.ExtentSize ) / dpb.SecSize ) + ( KO / dpb.SecSize ) );

				DWORD BlkNum = (DWORD) ( (double) SecNum / BlockRatio );
		
				Map.pBlockMap[ BlkNum ] = BlockUsed;

				KO += dpb.SecSize;
			}
			
			Map.UsedBytes += 0x0400;
		}

		SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

		Offset += 32;
	}

	/* Directory is immovable */
	for ( BYTE i=0; i<dpb.DirSecs; i++ )
	{
		DWORD BlkNum = (DWORD) ( (double) ( st + i ) / BlockRatio );

		Map.pBlockMap[ BlkNum ] = BlockFixed;

		Map.UsedBytes += dpb.SecSize;
	}

	/* System Tracks (if any) are immovable */
	if ( SystemDisk )
	{
		for ( BYTE t=0; t<dpb.SysTracks; t++ )
		{
			for ( BYTE i=0; i<dpb.SecsPerTrack; i++ )
			{
				DWORD si = (DWORD) t * (DWORD) dpb.SecsPerTrack;

				si      += (DWORD) i;

				DWORD BlkNum = (DWORD) ( (double) si / BlockRatio );

				Map.pBlockMap[ BlkNum ] = BlockFixed;

				Map.UsedBytes += dpb.SecSize;
			}
		}
	}

	SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

	return 0;
}

std::vector<AttrDesc> CPMFileSystem::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Extent 0 offset, the offset into the directory containing the entry for logical extent 0. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"Extent 0 Offset";
	Attrs.push_back( Attr );

	/* File start block */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile;
	Attr.Name  = L"Start block";
	Attrs.push_back( Attr );

	/* Read Only */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile;
	Attr.Name  = L"Read Only";
	Attrs.push_back( Attr );

	/* System File */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile;
	Attr.Name  = L"System File";
	Attrs.push_back( Attr );

	/* Archived File */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile;
	Attr.Name  = L"Archived File";
	Attrs.push_back( Attr );

	return Attrs;
}

int CPMFileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	BYTE Entry[ 32 ];

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	for ( BYTE i=2; i<=4; i++ ) { pFile->Attributes[ i ] = Changes->Attributes[ i ]; }

	ExportCPMDirectoryEntry( pFile, Entry );

	AutoBuffer Directory( dpb.DirSecs * dpb.SecSize );
	
	pDir->LoadDirectory( Directory );

	DWORD Offset = 0;

	while ( Offset < ( dpb.DirSecs * dpb.SecSize ) )
	{
		if ( CPMDirectoryEntryCMP( &Directory[ Offset ], Entry ) )
		{
			memcpy( &Directory[ Offset + 1 ], &Entry[ 1 ], 11 );
		}

		Offset += 32;
	}

	pDir->SaveDirectory( Directory );

	return pDirectory->ReadDirectory();
}

int CPMFileSystem::Rename( DWORD FileID, BYTE *NewName )
{
	BYTE Entry[ 32 ];
	BYTE RenamedEntry[ 32 ];

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	ExportCPMDirectoryEntry( pFile, Entry );

	rstrncpy( pFile->Filename, NewName, 8 );

	ExportCPMDirectoryEntry( pFile, RenamedEntry );

	AutoBuffer Directory( dpb.DirSecs * dpb.SecSize );
	
	pDir->LoadDirectory( Directory );

	DWORD Offset = 0;

	while ( Offset < ( dpb.DirSecs * dpb.SecSize ) )
	{
		if ( CPMDirectoryEntryCMP( &Directory[ Offset ], Entry ) )
		{
			memcpy( &Directory[ Offset + 1 ], &RenamedEntry[ 1 ], 11 );
		}

		Offset += 32;
	}

	pDir->SaveDirectory( Directory );

	return pDirectory->ReadDirectory();
}

int CPMFileSystem::DeleteFile( NativeFile *pFile, int FileOp )
{
	BYTE Entry[ 32 ];

	ExportCPMDirectoryEntry( pFile, Entry );

	AutoBuffer Directory( dpb.DirSecs * dpb.SecSize );
	
	pDir->LoadDirectory( Directory );

	DWORD Offset = 0;

	while ( Offset < ( dpb.DirSecs * dpb.SecSize ) )
	{
		if ( CPMDirectoryEntryCMP( &Directory[ Offset ], Entry ) )
		{
			memset( &Directory[ Offset ], 0xE5, 32 );
		}

		Offset += 32;
	}

	pDir->SaveDirectory( Directory );

	return pDirectory->ReadDirectory();
}

int	CPMFileSystem::ReplaceFile(NativeFile *pFile, CTempFile &store)
{
	NativeFile file = *pFile;

	DeleteFile( &file, FILEOP_DELETE_FILE );

	file.Length = store.Ext();

	pFile->Length = store.Ext();

	WriteFile( &file, store );

	return pDirectory->ReadDirectory();
}

int CPMFileSystem::Format_PreCheck( int FormatType, HWND hWnd )
{
	SystemDisk = false;

	if ( dpb.Flags & CF_SystemOptional )
	{
		if ( MessageBox( hWnd,
			L"This file system can create a System Disk wich contains tracks that may be bootable, instead of the usual unbootable Data Disk. "
			L"Do you want to create a System Disk instead of a Data Disk?",
			L"NUTS Formatter", MB_ICONQUESTION | MB_YESNO ) == IDYES )
		{

			SystemDisk = true;
		}
	}

	return 0;
}

std::vector<AttrDesc> CPMFileSystem::GetFSAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;
	
	Attrs.clear();

	AttrDesc Attr;

	/* Disc Title */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrBool;
	Attr.Name  = L"System Disk";

	Attr.StartingValue = (SystemDisk)?0xFFFFFFFF:0x00000000;

	Attrs.push_back( Attr );

	if ( dpb.Flags & CF_VolumeLabels )
	{
		Attr.Index = 1;
		Attr.Type  = AttrVisible | AttrEnabled | AttrString;
		Attr.Name  = L"Volume label";

		Attr.MaxStringLength = 11;
		Attr.pStringVal      = rstrndup( (BYTE *) "", 11 );

		Attrs.push_back( Attr );
	}

	return Attrs;
}

int CPMFileSystem::SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal )
{
	AutoBuffer Dir( dpb.DirSecs * dpb.SecSize );

	pDir->LoadDirectory( Dir );

	DWORD Offset = 0;

	bool DoneLabel = false;

	while ( Offset < ( dpb.DirSecs * dpb.SecSize ) )
	{
		BYTE *pEntry = &Dir[ Offset ];

		if ( pEntry[ 0 ] < 32 )
		{
			Offset += 32;

			continue;
		}

		if ( ( pEntry[ 0 ] == 32 ) || ( pEntry[ 0 ] == 0xE5 ) )
		{
			if ( !DoneLabel )
			{
				rstrncpy( &pEntry[ 1 ], pDir->DiskLabel, 11 );

				pEntry[ 0 ] = 32;

				DoneLabel = true;
			}
			else
			{
				pEntry[ 0 ] = 0xE5;
			}
		}
	}

	return 0;
}