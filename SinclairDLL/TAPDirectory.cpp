#include "StdAfx.h"

#include "../NUTS/Defs.h"
#include "SinclairDefs.h"
#include "TAPDirectory.h"
#include "../NUTS/libfuncs.h"

TAPDirectory::TAPDirectory(DataSource *pDataSource) : Directory(pDataSource)
{
}


TAPDirectory::~TAPDirectory(void)
{
}


int TAPDirectory::ReadDirectory(void)
{
	Files.clear();

	DWORD TAPOffset = 0;
	DWORD TAPSize   = (DWORD) pSource->PhysicalDiskSize;

	BYTE ShortHeader[   3 ];
	BYTE HeaderBlock[ 256 ];

	DWORD FileID = 0;

	while ( 1 )
	{
		if ( ( TAPOffset + 3 ) >= TAPSize )
		{
			break;
		}

		pSource->ReadRaw( (DWORD) TAPOffset, 3, ShortHeader );

		/* The length is the first two bytes, then the flag byte follows */
		WORD BlockLen = * (WORD *) &ShortHeader;
		BYTE Flag     = ShortHeader[ 2 ];

		if ( Flag == 00 )
		{
			/* Header block, yeah baby */
			
			/* Read the full header */
			if ( ( TAPOffset + BlockLen + 2 ) >= TAPSize )
			{
				break;
			}

			pSource->ReadRaw( (DWORD) TAPOffset, BlockLen + 2, HeaderBlock );

			TAPOffset += BlockLen + 2;

			NativeFile File;

			ZeroMemory( &File, sizeof( File ) );

			File.Filename = BYTEString( &HeaderBlock[ 4 ], 10 );

			File.Filename[10] = 0x20;

			for ( BYTE i=10; i<=10; i--) {
				if ( File.Filename[ i ] != 0x20 )
				{
					break;
				}
				else
				{
					File.Filename[ i ] = 0;
				}
			}

			File.Flags           = FF_Audio;
			File.Attributes[ 0 ] = 0xFFFFFFFF;
			File.fileID          = FileID;
			File.FSFileType      = FT_SINCLAIR;
			File.EncodingID      = ENCODING_SINCLAIR;
			File.Length          = 0xFFFFFFFF;
			File.XlatorID        = NULL;
			File.HasResolvedIcon = false;

			File.Attributes[ 2 ] = HeaderBlock[ 3 ];

			switch ( HeaderBlock[ 3 ] )
			{
				case 0:
					File.Type = FT_BASIC;
					File.Icon = FT_BASIC;

					File.Attributes[ 3 ] = * (WORD *) &HeaderBlock[ 16 ];
					File.Attributes[ 4 ] = * (WORD *) &HeaderBlock[ 18 ];

					File.XlatorID = BASIC_SPECTRUM;

					break;

				case 1:
				case 2:
					File.Type = FT_Data;
					File.Icon = FT_Data;

					File.Attributes[ 5 ] = HeaderBlock[ 17 ];

					break;

				case 3:
					{
						WORD LoadAddr = * (WORD *) &HeaderBlock[ 16 ];
						WORD Length   = * (WORD *) &HeaderBlock[ 14 ];

						File.Attributes[ 1 ] = LoadAddr;

						if ( ( LoadAddr == 16384 ) && ( Length == 6912 ) )
						{
							File.Type = FT_Graphic;
							File.Icon = FT_Graphic;

							File.XlatorID = GRAPHIC_SPECTRUM;
						}
						else
						{
							File.Type = FT_Binary;
							File.Icon = FT_Binary;
						}
					}
					break;
			}

			Files.push_back( File );

			FileID++;
		}
		else
		{
			NativeFile File;

			bool IsOrphan = true;

			/* Data block - Could be the last header's gaff */
			if ( Files.size() != 0 )
			{
				if ( Files.back().Length == 0xFFFFFFFF )
				{
					File = Files.back();

					File.Length          = * (WORD *) &ShortHeader[ 0 ];
					File.Attributes[ 0 ] = TAPOffset;
				
					File.Length -= 2;

					Files[ File.fileID ] = File;

					IsOrphan = false;
				}
			}
			
			if ( IsOrphan )
			{
				File.Flags           = FF_Audio;
				File.Attributes[ 0 ] = TAPOffset;
				File.Length          = * (WORD *) &ShortHeader[ 0 ];
				File.fileID          = FileID;
				File.FSFileType      = FT_SINCLAIR;
				File.EncodingID      = ENCODING_SINCLAIR;
				File.Icon            = FT_Arbitrary;
				File.Type            = FT_Arbitrary;
				File.Attributes[ 2 ] = 0xFFFFFFFF;
				File.XlatorID        = NULL;

				File.Filename = (BYTE *) "ORPHAN BLK";

				File.Length -= 2;

				Files.push_back( File );

				FileID++ ;
			}

			WORD Blocklen = * (WORD *) &ShortHeader[ 0 ];

			TAPOffset += BlockLen + 2;
		}
	}

	return 0;
}

int TAPDirectory::WriteDirectory(void)
{
	return 0;
}
