#include "StdAfx.h"
#include "MacintoshMFSDirectory.h"

#include "..\NUTS\libfuncs.h"
#include "..\NUTS\Preference.h"

#include "..\NUTS\NUTSError.h"

#include "AppleDLL.h"

#include <MMSystem.h>

MacintoshMFSDirectory::MacintoshMFSDirectory( DataSource *pSource ) : Directory( pSource )
{
}


MacintoshMFSDirectory::~MacintoshMFSDirectory(void)
{
}

void MacintoshMFSDirectory::ClearIcons()
{
	for ( ResolvedIconList::iterator iIcon=ResolvedIcons.begin(); iIcon != ResolvedIcons.end(); iIcon++ )
	{
		if ( iIcon->second.pImage != nullptr )
		{
			delete iIcon->second.pImage;
		}
	}

	ResolvedIcons.clear();
}

int MacintoshMFSDirectory::ReadDirectory(void)
{
	Files.clear();
	ClearIcons();
	FinderInfo.clear();

	if ( !pRecord->Valid )
	{
		return NUTSError( 0x601, L"Failed to read volume record" );
	}

	// No directory entry (supposedly) spans across a block, so we only need
	// a sector's worth of data. If an entry is proposing to cross a block
	// boundary, we'll ignore it.
	BYTE Buffer[ 0x200 ];

	DWORD CurrentBlock = pRecord->FirstBlock;
	DWORD ReadBlocks   = 1;

	if ( pSource->ReadSectorLBA( CurrentBlock, &Buffer[ 0x000 ], 0x200 ) != DS_SUCCESS )
	{
		return -1;
	}

	CurrentBlock++;

	// Now start the while loop
	DWORD EntryPtr = 0;
	DWORD FullPtr  = 0;
	DWORD FileID   = 0;

	while ( FullPtr < ( pRecord->DirBlockLen * 0x200 ) )
	{
		if ( EntryPtr & 1 )
		{
			// 2-byte alignment please
			EntryPtr++;
			FullPtr++;
		}

		if ( EntryPtr >= 0x200 )
		{
			EntryPtr -= 0x200;

			if ( pSource->ReadSectorLBA( CurrentBlock, Buffer, 0x200 ) != DS_SUCCESS )
			{
				return -1;
			}

			CurrentBlock++;

			ReadBlocks++;
		}

		if ( ( Buffer[ EntryPtr ] == 0x00 ) || ( ( EntryPtr + 50) > 0x200 ) || ( ( EntryPtr + Buffer[ EntryPtr + 50 ] + 1 ) > 0x200 ) )
		{
			// No more in this block
			FullPtr  += 0x200 - ( EntryPtr % 0x200 );
			EntryPtr += 0x200 - ( EntryPtr % 0x200 );
		}
		else
		{
			NativeFile file;

			file.fileID          = FileID;
			file.EncodingID      = ENCODING_MACINTOSH;
			file.Flags           = 0;
			file.FSFileType      = FILE_MACINTOSH;
			file.HasResolvedIcon = false;
			file.Icon            = BLANK_ICON;
			file.Type            = FT_Arbitrary;
			file.XlatorID        = NULL;
			file.Length          = BEDWORD( &Buffer[ EntryPtr + 24 ] );
		
			file.Filename = BYTEString( &Buffer[ EntryPtr + 51 ], Buffer[ EntryPtr + 50 ] );

			BYTE Flags = Buffer[ EntryPtr ];

			// Sort out some attributes
			file.Attributes[ 0 ] = (Flags&0x01)?0xFFFFFFFF:0x00000000; // Locked
			file.Attributes[ 1 ] = BEDWORD( &Buffer[ EntryPtr + 18 ] ); // File Number
			file.Attributes[ 2 ] = BEWORD(  &Buffer[ EntryPtr + 22 ] ); // Data Fork Start
			file.Attributes[ 3 ] = BEWORD(  &Buffer[ EntryPtr + 32 ] ); // Resource Fork Start
			file.Attributes[ 4 ] = BEDWORD( &Buffer[ EntryPtr + 42 ] ) - APPLE_TIME_OFFSET; // Creation Time
			file.Attributes[ 5 ] = BEDWORD( &Buffer[ EntryPtr + 46 ] ) - APPLE_TIME_OFFSET; // Modification Time

			file.Attributes[ 13 ] = Flags; // MacOS might have interpretation here.
			file.Attributes[ 14 ] = FullPtr; // Offset into directory of entry
			file.Attributes[ 15 ] = BEDWORD( &Buffer[ EntryPtr + 34 ] ); // Resource Fork Length

			// Copy these two so they can be copied to target FS
			file.Attributes[ 11 ] = * (DWORD *) &Buffer[ EntryPtr + 2 ];
			file.Attributes[ 12 ] = * (DWORD *) &Buffer[ EntryPtr + 6 ];

			file.ExtraForks = 1;

			// Copy the finder info
			AutoBuffer finder(16);

			memcpy( (BYTE *) finder, &Buffer[ EntryPtr + 2 ], 16 );

			SetFileType( &file, finder );

			FinderInfo.push_back( finder );

			FullPtr  += 51UL + (DWORD) Buffer[ EntryPtr + 50 ];
			EntryPtr += 51UL + (DWORD) Buffer[ EntryPtr + 50 ];

			FileID++;

			Files.push_back( file );
		}
	}

	if ( (bool) Preference( L"UseResolvedIcons" ) )
	{
		NativeFileIterator iFile;

		for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
		{
			if ( iFile->Attributes[ 15 ] == 0 )
			{
				// No resource fork here
				continue;
			}

			CTempFile obj;

			ReadResourceFork( iFile->fileID, obj );

			ExtractIcon( &*iFile, obj );
		}
	}

	return 0;
}

int MacintoshMFSDirectory::WriteDirectory(void)
{
	// So this gets messy. Directory entries are variable length, WORD-aligned,
	// and don't cross sector boundaries. So for each entry, we must bump the
	// entry pointer if it's not on a word boundary, and push it to the next
	// sector if the calculated entry size will not fit. As such, we will do
	// this in two passes. The first pass is the "check" pass. If it would
	// exceed the number of sectors allocated to the directory, then we abort
	// with Directory Full. We only write the sectors on the second pass.

	DWORD NumSectors = 1;
	DWORD DirSecNum  = pRecord->FirstBlock;
	DWORD EntryPtr   = 0;
	DWORD ClumpSize  = pRecord->AllocBytes;

	BYTE Buffer[ 512 ];

	NativeFileIterator iFile;

	for ( int pass=0; pass<2; pass++ )
	{
		NumSectors = 1;
		DirSecNum  = pRecord->FirstBlock;
		EntryPtr   = 0;

		iFile = Files.begin();

		while ( 1 )
		{
			if ( EntryPtr & 1 ) { EntryPtr++; }

			bool WriteSec = false;

			if ( EntryPtr == 0x200 ) { WriteSec = true; }
			if ( iFile != Files.end() )
			{
				if ( EntryPtr + 51 + iFile->Filename.length() > 0x200 ) { WriteSec = true; }
			}

			if ( WriteSec )
			{
				if ( pass == 1 )
				{
					if ( EntryPtr != 0x200 )
					{
						Buffer[ EntryPtr ] = 0x00;
					}

					if ( pSource->WriteSectorLBA( DirSecNum, Buffer, 0x200 ) != DS_SUCCESS )
					{
						return -1;
					}

				}

				EntryPtr = 0;
				DirSecNum++;
				NumSectors++;
			}

			if ( iFile == Files.end() )
			{
				Buffer[EntryPtr] = 0;

				break;
			}

			Buffer[ EntryPtr +  0 ] = (BYTE ) ( iFile->Attributes[ 13 ] & 0x7F );
			Buffer[ EntryPtr +  0 ] |= 0x80;

			if ( iFile->Attributes[ 0 ] != 0 ) { Buffer[ EntryPtr + 0 ] |= 1; }

			Buffer[ EntryPtr +  1 ] = 0;

			// Finder Info
			memcpy( &Buffer[ EntryPtr + 2 ], FinderInfo[ iFile->fileID ], 16 );

			WBEDWORD( &Buffer[ EntryPtr + 18 ], iFile->Attributes[ 1 ] );
			WBEWORD(  &Buffer[ EntryPtr + 22 ], iFile->Attributes[ 2 ] );
			WBEWORD(  &Buffer[ EntryPtr + 32 ], iFile->Attributes[ 3 ] );
			WBEDWORD( &Buffer[ EntryPtr + 42 ], iFile->Attributes[ 4 ] + APPLE_TIME_OFFSET );
			WBEDWORD( &Buffer[ EntryPtr + 46 ], iFile->Attributes[ 5 ] + APPLE_TIME_OFFSET );
			WBEDWORD( &Buffer[ EntryPtr + 34 ], iFile->Attributes[ 15 ] );
			WBEDWORD( &Buffer[ EntryPtr + 24 ], iFile->Length );

			Buffer[ EntryPtr + 50 ] = (BYTE) min( 255, iFile->Filename.length() );

			rstrncpy( &Buffer[ EntryPtr + 51 ], iFile->Filename, min( 255, iFile->Filename.length() ) );

			// We need to fix up the allocated sizes, which is measured in clumps
			DWORD DataClumpSize = 0;
			DWORD ResClumpSize  = 0;

			if ( iFile->Length > 0 )
			{
				DataClumpSize = pRecord->AllocSize * pFAT->GetFileAllocCount( iFile->Attributes[ 2 ] );
			}

			if ( iFile->Attributes[ 15 ] > 0 )
			{
				ResClumpSize = pRecord->AllocSize * pFAT->GetFileAllocCount( iFile->Attributes[ 3 ] );
			}

			WBEDWORD( &Buffer[ EntryPtr + 28 ], DataClumpSize );
			WBEDWORD( &Buffer[ EntryPtr + 38 ], ResClumpSize );

			EntryPtr += 51 + iFile->Filename.length();

			iFile++;
		}

		if ( pass == 1 )
		{
			if ( pSource->WriteSectorLBA( DirSecNum, Buffer, 0x200 ) != DS_SUCCESS )
			{
				return -1;
			}
		}

		if ( ( pass == 0 ) && ( NumSectors > pRecord->DirBlockLen ) )
		{
			return NUTSError( 0x803, L"Directory Full" );
		}

		if ( pass == 1 ) 
		{
			while ( NumSectors < pRecord->DirBlockLen )
			{
				NumSectors++;
				DirSecNum++;

				// need to fill in the blank sectors with 0s.
				Buffer[ 0 ] = 0;

				if ( pSource->WriteSectorLBA( DirSecNum, Buffer, 0x200 ) != DS_SUCCESS )
				{
					return -1;
				}
			}
		}
	}

	return 0;
}

void MacintoshMFSDirectory::ReadResourceFork( DWORD FileID, CTempFile &fileobj )
{
	NativeFile *pFile = &Files[ FileID ];

	DWORD BytesToGo = pFile->Attributes[ 15 ];

	DWORD AllocBlockBytesToGo = pRecord->AllocSize;

	fileobj.Seek( 0 );

	BYTE Buffer[ 0x200 ];

	WORD  Alloc   = pFile->Attributes[ 3 ] - 2;
	DWORD DiscAdx = ( Alloc * pRecord->AllocSize ) + ( pRecord->FirstAlloc * 0x200 );

	DWORD SecNum  = DiscAdx / 0x200;

	while ( BytesToGo > 0 )
	{
		if ( pSource->ReadSectorLBA( SecNum, Buffer, 0x200 ) != DS_SUCCESS )
		{
			return;
		}

		AllocBlockBytesToGo -= 0x200;
		DiscAdx             += 0x200;

		fileobj.Write( Buffer, min( BytesToGo, 0x200 ) );

		SecNum++;

		if ( AllocBlockBytesToGo == 0 )
		{
			AllocBlockBytesToGo = pRecord->AllocSize;

			Alloc = pFAT->NextAllocBlock( Alloc + 2 ) - 2;

			DiscAdx = DiscAdx = ( Alloc * pRecord->AllocSize ) + ( pRecord->FirstAlloc * 0x200 );

			SecNum = DiscAdx / 0x200;
		}

		BytesToGo -= min( BytesToGo, 0x200 );
	}

	fileobj.SetExt( pFile->Attributes[ 15 ] );
}

void MacintoshMFSDirectory::ExtractIcon( NativeFile *pFile, CTempFile &fileobj )
{
	BYTE Buffer[ 256 ];

	// First read the resource header
	fileobj.Seek( 0 );
	fileobj.Read( Buffer, 16 );

	DWORD resDataOffset = BEDWORD( &Buffer[ 0x0 ] );
	DWORD resMapOffset  = BEDWORD( &Buffer[ 0x4 ] );
	DWORD resDataSize   = BEDWORD( &Buffer[ 0x8 ] );
	DWORD resMapSize    = BEDWORD( &Buffer[ 0xC ] );

	// We now need the type list offset
	fileobj.Seek( resMapOffset );
	fileobj.Read( Buffer, 32 );

	DWORD typeListOffset = resMapOffset + BEWORD( &Buffer[ 24 ] );

	// Now process the type list
	fileobj.Seek( typeListOffset );
	fileobj.Read( Buffer, 2 );

	WORD typeCount = BEWORD( Buffer );

	if ( typeCount == 0xFFFF )
	{
		// no resource types. exit.
		return;
	}

	for ( WORD type = 0; type <= typeCount; type++ )
	{
		// Get the type array element
		fileobj.Seek( typeListOffset + 2 + ( 8 * type ) );

		fileobj.Read( Buffer, 8 );

		// Check the FOURCC
		DWORD fcc  = BEDWORD( Buffer );
		DWORD tfcc = MAKEFOURCC( 'N', 'O', 'C', 'I' );

		if ( fcc == tfcc )
		{
			// Get the resource count and offset
			WORD ResCount  = BEWORD( &Buffer[ 4 ] );
			WORD ResOffset = BEWORD( &Buffer[ 6 ] );

			// We'll take the first icon
			DWORD RResOffset = typeListOffset + ResOffset;

			fileobj.Seek( RResOffset );
			fileobj.Read( Buffer, 12 );

			DWORD DataOffset = BEDWORD( &Buffer[ 4 ] );

			// This is 24 bit, so mask off the top byte (also, bigendian, remember)
			DataOffset &= 0x00FFFFFF;

			DataOffset += resDataOffset;

			// Now we can read the resource data!
			fileobj.Seek( DataOffset );
			fileobj.Read( Buffer, 4 );

			DWORD DataSize = BEDWORD( Buffer );

			AutoBuffer IconBuffer( DataSize );

			fileobj.Read( IconBuffer, DataSize );

			IconDef icon;

			icon.Aspect = AspectRatio( 512, 384 );

			MakeIconFromData( IconBuffer, &icon );

			ResolvedIcons[ pFile->fileID ] = icon;

			pFile->HasResolvedIcon = true;
		}
	}
}

void MacintoshMFSDirectory::MakeIconFromData( BYTE *InBuffer, IconDef *pIcon )
{
	pIcon->pImage = malloc( 32 * 4 * 32 );

	pIcon->bmi.biBitCount     = 32;
	pIcon->bmi.biClrImportant = 0;
	pIcon->bmi.biClrUsed      = 0;
	pIcon->bmi.biCompression  = BI_RGB;
	pIcon->bmi.biHeight       = 32;
	pIcon->bmi.biPlanes       = 1;
	pIcon->bmi.biSize         = sizeof( BITMAPINFOHEADER );
	pIcon->bmi.biSizeImage    = 32 * 4 * 32;
	pIcon->bmi.biWidth        = 32;

	pIcon->bmi.biXPelsPerMeter = 0;
	pIcon->bmi.biYPelsPerMeter = 0;

	for ( int y = 0; y < 32; y++ )
	{
		for ( int x=0; x < 32; x++ )
		{
			BYTE *pData = (BYTE *) pIcon->pImage;
			      pData = &pData[ ( y * 32 * 4 ) + ( x * 4 ) ];
			BYTE *pBits = &InBuffer[ ( y * 4 ) + (x / 8 ) ];

			BYTE bitPos = ( 7 - ( x % 8 ) );

			BYTE bit = *pBits & ( 1 << bitPos );
			
			if ( bit == 0 )
			{
				* (DWORD *) pData = 0xFFFFFFFF;
			}
			else
			{
				* (DWORD *) pData = 0x00000000;
			}
		}
	}
}

void MacintoshMFSDirectory::SetFileType( NativeFile* pFile, BYTE *pFinderData )
{
	// By reading the data this way, we can use MAKEFOURCC without reversing the FCC.
	DWORD fcc = * (DWORD *) pFinderData;

	if ( fcc == MAKEFOURCC( 'T', 'E', 'X', 'T' ) )
	{
		pFile->Type = FT_Text;
		pFile->Icon = DOC_ICON;
	}

	if (
		( fcc == MAKEFOURCC( 'G', 'I', 'F', 'f' ) ) ||
		( fcc == MAKEFOURCC( 'P', 'N', 'G', 'f' ) ) ||
		( fcc == MAKEFOURCC( 'J', 'P', 'E', 'G' ) ) ||
		( fcc == MAKEFOURCC( 'B', 'M', 'P', 'f' ) ) ||
		( fcc == MAKEFOURCC( 'T', 'I', 'F', 'F' ) ) ||
		( fcc == MAKEFOURCC( 'P', 'I', 'C', 'T' ) )
		)
	{
		pFile->Type = FT_Graphic;
	}

	if (
		( fcc == MAKEFOURCC( 'M', 'i', 'd', 'i' ) ) ||
		( fcc == MAKEFOURCC( 's', 'n', 'd', ' ' ) ) ||
		( fcc == MAKEFOURCC( 'W', 'A', 'V', 'E' ) ) ||
		( fcc == MAKEFOURCC( 'A', 'I', 'F', 'F' ) ) ||
		( fcc == MAKEFOURCC( 'M', 'p', '3', ' ' ) ) ||
		( fcc == MAKEFOURCC( 'U', 'L', 'A', 'W' ) )
		)
	{
		pFile->Type = FT_Sound;
	}

	if ( fcc == MAKEFOURCC( 'B', 'I', 'N', 'A' ) )
	{
		pFile->Type = FT_Binary;
	}

	if ( fcc == MAKEFOURCC( 'A', 'P', 'P', 'L' ) )
	{
		pFile->Type = FT_App;
		pFile->Icon = APPL_ICON;
	}

	if ( fcc == MAKEFOURCC( 'P', 'R', 'E', 'F' ) )
	{
		pFile->Type = FT_Pref;
	}
}

void MacintoshMFSDirectory::GetFinderCodes( DWORD FileID, BYTE *pType, BYTE *pFinder )
{
	memcpy( pType, (BYTE *) &(FinderInfo[ FileID ])[ 0 ], 4 );
	memcpy( pFinder, (BYTE *) &(FinderInfo[ FileID])[ 4 ], 4 );
}
