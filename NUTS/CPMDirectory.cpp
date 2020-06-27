#include "StdAfx.h"

#include "CPMDirectory.h"
#include "Defs.h"
#include "libfuncs.h"

void ImportCPMDirectoryEntry( NativeFile *pFile, BYTE *pEntry )
{
	memcpy( pFile->Filename,  &pEntry[ 1 ], 8 );
	memcpy( pFile->Extension, &pEntry[ 9 ], 3 );

	for ( BYTE i=0; i<8; i++ ) { pFile->Filename[ i ]  &= 0x7F; }
	for ( BYTE i=0; i<3; i++ ) { pFile->Extension[ i ] &= 0x7F; }

	for ( BYTE i=7; i>0; i-- )
	{
		if ( pFile->Filename[ i ] == 0x20 )
		{
			pFile->Filename[ i ] = 0;
		}
		else
		{
			break;
		}
	}

	for ( BYTE i=2; i>0; i-- )
	{
		if ( pFile->Extension[ i ] == 0x20 )
		{
			pFile->Extension[ i ] = 0;
		}
		else
		{
			break;
		}
	}

	if ( pEntry[ 0x09 ] & 0x80 ) { pFile->Attributes[ 1 ] = 0xFFFFFFFF; } else { pFile->Attributes[ 1 ] = 0x00000000; }
	if ( pEntry[ 0x0A ] & 0x80 ) { pFile->Attributes[ 2 ] = 0xFFFFFFFF; } else { pFile->Attributes[ 2 ] = 0x00000000; }
	if ( pEntry[ 0x0B ] & 0x80 ) { pFile->Attributes[ 3 ] = 0xFFFFFFFF; } else { pFile->Attributes[ 3 ] = 0x00000000; }
}

void ExportCPMDirectoryEntry( NativeFile *pFile, BYTE *pEntry )
{
	memset( pEntry, 0x20, 11 );

	rstrncpy( &pEntry[ 1 ], pFile->Filename,  8 );
	rstrncpy( &pEntry[ 9 ], pFile->Extension, 3 );

	for ( BYTE i=0; i<8; i++ )
	{
		if ( pEntry[ 1 + i ] == 0 )
		{
			pEntry[ 1 + i ] = 0x20;
		}
	}

	for ( BYTE i=0; i<3; i++ )
	{
		if ( pEntry[ 9 + i ] == 0 )
		{
			pEntry[ 9 + i ] = 0x20;
		}
	}

	if ( pFile->Attributes[ 1 ] ) { pEntry[ 0x09 ] |= 0x80; } else { pEntry[ 0x09 ] &= 0x7F; }
	if ( pFile->Attributes[ 2 ] ) { pEntry[ 0x0A ] |= 0x80; } else { pEntry[ 0x0A ] &= 0x7F; }
	if ( pFile->Attributes[ 3 ] ) { pEntry[ 0x0B ] |= 0x80; } else { pEntry[ 0x0B ] &= 0x7F; }
}

bool CPMDirectoryEntryCMP( BYTE *pEntry1, BYTE *pEntry2 )
{
	for ( BYTE i=1; i<12; i++ )
	{
		if ( ( pEntry1[i] & 0x7F ) != ( pEntry2[i] & 0x7F ) )
		{
			return false;
		}
	}

	return true;
}

void CPMDirectory::LoadDirectory( BYTE *Buffer )
{
	for ( BYTE i=0; i<dpb.DirSecs; i++ )
	{
		pSource->ReadSector( DirSector + i, &Buffer[ dpb.SecSize * i ], dpb.SecSize );
	}
}

void CPMDirectory::SaveDirectory( BYTE *Buffer )
{
	for ( BYTE i=0; i<dpb.DirSecs; i++ )
	{
		pSource->WriteSector( DirSector + i, &Buffer[ dpb.SecSize * i ], dpb.SecSize );
	}
}

int CPMDirectory::ReadDirectory(void)
{
	Files.clear();
	ResolvedIcons.clear();

	pBlockMap->InitialiseMap( pSource->PhysicalDiskSize / 1024 );

	AutoBuffer Buffer( dpb.DirSecs * dpb.SecSize );

	LoadDirectory( Buffer );

	DWORD Offset = 0;
	DWORD FileID = 0;

	while ( Offset < 0x800 )
	{
		BYTE *pEntry = &Buffer[ Offset ];

		if ( pEntry[ 0 ] < 32 )
		{
			WORD Extent = ( pEntry[ 0x0E ] << 8 ) + pEntry[ 0x0C ];

			/* Look for the file already existing */
			NativeFileIterator iFile;

			bool Exists = false;

			NativeFile *pFile = nullptr;

			for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
			{
				if ( rstrncmp( &pEntry[ 1 ], &Buffer[ iFile->Attributes[ 0 ] + 1 ], 0x0B ) )
				{
					Exists = true;
					pFile  = &*iFile;

					/* Found this. */
					if ( Extent == 0 )
					{
						iFile->Attributes[ 0 ] = Offset;
					}
				}
			}

			if ( !Exists )
			{
				NativeFile file;

				file.EncodingID = dpb.Encoding;
				file.fileID     = FileID;
				file.Flags      = FF_Extension;
				file.FSFileType = dpb.FSFileType;
				file.Icon       = FT_Arbitrary;
				file.Type       = FT_Arbitrary;
				file.Length     = ( pEntry[ 0x0F ] * 0x80 ) + pEntry[ 0x0D ];
				file.XlatorID   = 0;

				if ( pEntry[ 0x0D ] == 0 ) { file.Length += 128; }

				file.HasResolvedIcon = false;

				rstrncpy( file.Filename,  &pEntry[ 1 ], 8 );
				rstrncpy( file.Extension, &pEntry[ 9 ], 3 );

				/* Fix up the entries */
				ImportCPMDirectoryEntry( &file, pEntry );

				file.Attributes[ 0 ] = Offset; // For now. This may get updated.

				if ( rstrncmp( file.Extension, (BYTE *) "BAS", 3 ) ) { file.Type = FT_BASIC;  file.Icon = FT_BASIC;  }
				if ( rstrncmp( file.Extension, (BYTE *) "BIN", 3 ) ) { file.Type = FT_Binary; file.Icon = FT_Binary; }
				if ( rstrncmp( file.Extension, (BYTE *) "TXT", 3 ) ) { file.Type = FT_Text;   file.Icon = FT_Text; file.XlatorID = TUID_TEXT; }
				if ( rstrncmp( file.Extension, (BYTE *) "COM", 3 ) ) { file.Type = FT_Code;   file.Icon = FT_Code;   }

				ExtraReadDirectory( &file );

				Files.push_back( file );

				FileID++;
			}
			else
			{
				/* We found another fragment. pFile points to the original. */

				pFile->Length += ( pEntry[ 0x0F ] * 0x80 ) + pEntry[ 0x0D ];

				if ( pEntry[ 0x0D ] == 0 ) { pFile->Length += 128; }
			}

			/* Marks the blocks as in use */
			DWORD NumRecords = ( pEntry[ 0x0F ] * 0x80 ) + pEntry[ 0x0D ];

			if ( pEntry[ 0x0D ] == 0 ) { NumRecords += 128; }

			DWORD NumBlocks = NumRecords / 1024;

			if ( NumBlocks % 1024 ) { NumBlocks++; }

			for ( BYTE i=0; i<(BYTE) NumBlocks; i++ )
			{
				pBlockMap->SetBlock( pEntry[ 0x10 + i ] );
			}
		}

		Offset += 32;
	}

	/* These are held by the directory itself */
	pBlockMap->SetBlock( 0 );
	pBlockMap->SetBlock( 1 );

	return 0;
}

int CPMDirectory::WriteDirectory(void)
{
	return 0;
}
