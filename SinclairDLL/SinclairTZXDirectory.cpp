#include "StdAfx.h"
#include "SinclairTZXDirectory.h"

#include "SinclairDefs.h"

#include "../NUTS/libfuncs.h"

TZXPB SinclairPB = {
	FSID_SPECTRUM_TZX,
	L"Sinclair TZX",
	"Sinclair TZX Image",
	ENCODING_SINCLAIR,
	FT_SINCLAIR_TZX,
	"TZX",
	L"SINCLAIRTZX"
};

void SinclairTZXDirectory::ResolveFiles( void )
{
	/* Examine the Files vector and find a pair of standard blocks where the 
	   first is a header and the second is a data block, and replace them
	   both with a TAP file type. */

	/* If there is one block but without the other, Convert it to ORPHAN BLK */

	if ( Files.size() == 0 )
	{
		/* this won't fly */
		return;
	}

	NativeFileIterator iFile = Files.begin();

	while ( iFile != Files.end() )
	{
		NativeFileIterator iNext = iFile;

		iNext++;

		bool Header1 = false;
		bool Data1   = false;
		bool Data2   = false;

		BYTE HeaderData1[ 32 ];
		BYTE HeaderData2[ 32 ];

		if ( iFile->Attributes[ 14 ] == 0x10 )
		{
			pSource->ReadRaw( iFile->Attributes[ 15 ], 32, HeaderData1 );

			if ( HeaderData1[ 5 ] < 0x80 )
			{
				Header1 = true;
			}
			else
			{
				Data1 = true;
			}
		}

		if ( ( iNext != Files.end() ) && ( iNext->Attributes[ 14 ] == 0x10 ) )
		{
			pSource->ReadRaw( iNext->Attributes[ 15 ], 32, HeaderData2 );

			if ( HeaderData2[ 5 ] >= 0x80 )
			{
				Data2 = true;
			}
		}

		if ( Data1 )
		{
			/* Oprhan data block - make an ORPHAN BLK */
			iFile->Filename = (BYTE *) "ORPHAN BLK";

			iFile->FSFileType      = FT_SINCLAIR;
			iFile->Type            = FT_Binary;
			iFile->Icon            = FT_Binary;
			iFile->Attributes[ 0 ] = iFile->Attributes[ 15 ] + 0x06;
			iFile->Length          = iFile->Length - 0x07; /* Checksum byte */
			iFile->Attributes[ 2 ] = 0xFFFFFFFF;
			iFile->Attributes[ 3 ] = 0;
			iFile->Attributes[ 4 ] = 0;

			iFile->Attributes[ 14 ] = 0;
			iFile->Flags            &= ~FF_NotRenameable;
		}
		else if ( Header1 )
		{
			/* FILE! */
			iFile->Filename = BYTEString( &HeaderData1[ 7 ], 10 );

			iFile->Filename[10] = 0x20;

			for ( BYTE i=10; i<=10; i--) {
				if ( iFile->Filename[ i ] != 0x20 )
				{
					break;
				}
				else
				{
					iFile->Filename[ i ] = 0;
				}
			}

			iFile->FSFileType = FT_SINCLAIR;
			iFile->Flags     &= ~FF_NotRenameable;

			iFile->Attributes[ 2 ] = HeaderData1[ 6 ];

			switch ( HeaderData1[ 6 ] )
			{
				case 0:
					iFile->Type = FT_BASIC;
					iFile->Icon = FT_BASIC;

					iFile->Attributes[ 3 ] = * (WORD *) &HeaderData1[ 19 ];
					iFile->Attributes[ 4 ] = * (WORD *) &HeaderData1[ 21 ];

					iFile->XlatorID = BASIC_SPECTRUM;

					break;

				case 1:
				case 2:
					iFile->Type = FT_Data;
					iFile->Icon = FT_Data;

					iFile->Attributes[ 5 ] = HeaderData2[ 17 ];

					break;

				case 3:
					{
						WORD LoadAddr = * (WORD *) &HeaderData1[ 19 ];
						WORD Length   = * (WORD *) &HeaderData1[ 17 ];

						iFile->Attributes[ 1 ] = LoadAddr;

						if ( ( LoadAddr == 16384 ) && ( Length == 6912 ) )
						{
							iFile->Type = FT_Graphic;
							iFile->Icon = FT_Graphic;

							iFile->XlatorID = GRAPHIC_SPECTRUM;
						}
						else
						{
							iFile->Type = FT_Binary;
							iFile->Icon = FT_Binary;
						}
					}
					break;
			}

			iFile->Attributes[ 14 ] = 0;

			if ( Data2 ) 
			{
				iFile->Attributes[ 0 ] = iNext->Attributes[ 15 ] + 0x06;
				iFile->Length          = iNext->Length - 0x07; /* Checksum byte */

				DWORD fid = iNext->fileID;

				BlockLengths.erase( BlockLengths.begin() + fid );

				Files.erase( iNext );

				/* The file IDs must now be corrected */
				DWORD FileID = 0;

				for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
				{
					iFile->fileID = FileID;

					FileID++;
				}

				iFile = Files.begin();

				continue;
			}
			else
			{
				/* Orphan header block - Make a zero-length file */

				iFile->Attributes[ 0 ] = iFile->Attributes[ 15 ] + 0x06;
				iFile->Attributes[ 2 ] = 0xFFFFFFFD;
				iFile->Filename        = (BYTE *) "ORPHAN HDR";
				iFile->Length          = iFile->Length - 0x07; /* Checksum byte */
				iFile->Flags          |= FF_NotRenameable;

				iFile++;

				continue;
			}
		}

		iFile++;
	}
}
