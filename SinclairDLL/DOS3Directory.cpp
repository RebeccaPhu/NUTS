#include "StdAfx.h"
#include "DOS3Directory.h"

#include "SinclairDefs.h"
#include "../NUTS/Preference.h"
#include "../NUTS/libfuncs.h"

void DOS3Directory::ExtraReadDirectory( NativeFile *pFile )
{
	if ( pFile->Flags & FF_Extension )
	{
		if ( rstrnicmp( pFile->Extension, (BYTE *) "BAS", 3 ) )
		{
			pFile->Type = FT_BASIC;
			pFile->Icon = FT_BASIC;

			pFile->XlatorID = BASIC_SPECTRUM;
		}
	}

	bool SlowEnhance = (bool) Preference( L"SlowEnhance", false );

	if ( ( pSource->Flags & DS_SlowAccess ) && ( !SlowEnhance ) )
	{
		/* This will cause mondo extra disk reads, so don't do it if the data source is slow,
		   e.g. an actual floppy disk */
		return;
	}

	/* First we'll need the start block. We may be called multiple times, so check the attributes if
	    we've been here before. */

	if ( pFile->Attributes[ 0xF ] == 0 )
	{
		DWORD Adx = pFile->Attributes[ 1 ] * 1024;

		BYTE Header[ 128 ];

		pSource->ReadRaw( ( DirSector * 0x200 ) + Adx, 128, Header );

		/* Checksum the header, as +3DOS uses headerless files sometimes because **** you, that's why */
		WORD sum = 0;

		for ( BYTE i=0; i<127; i++ )
		{
			sum += Header[ i ];
		}

		BYTE expected = Header[ 127 ];

		sum &= 0xFF;

		if ( ( sum == expected ) && ( rstrncmp( Header, (BYTE *) "PLUS3DOS", 8 ) ) && ( Header[ 8 ] == 0x1A ) )
		{
			/* We can now enhance the length */
			pFile->Length = * (DWORD *) &Header[ 11 ];

			pFile->Attributes[ 6 ] = Header[ 15 ];

			switch ( Header[ 15 ] )
			{
			case 0:
				pFile->Attributes[ 7 ] = * (WORD *) &Header[ 18 ];
				pFile->Attributes[ 8 ] = * (WORD *) &Header[ 20 ];

				pFile->Type = FT_BASIC;
				pFile->Icon = FT_BASIC;
				break;

			case 1:
			case 2:
				pFile->Attributes[ 9 ] = Header[ 19 ];

				pFile->Type = FT_Data;
				pFile->Icon = FT_Data;
				break;

			default:
				pFile->Attributes[ 5 ] = * (WORD *) &Header[ 18 ];

				pFile->Type = FT_Binary;
				pFile->Icon = FT_Binary;
				break;
			}

			pFile->Attributes[ 10 ] = 0xFFFFFFFF;

			/* We can be a little cleverer now */
			if ( ( Header[ 15 ] == 3 ) && ( pFile->Attributes[ 5 ] == 0x4000 ) && ( pFile->Length == 6912 ) )
			{
				pFile->Type = FT_Graphic;
				pFile->Icon = FT_Graphic;

				pFile->XlatorID = GRAPHIC_SPECTRUM;
			}
		}
		else
		{
			pFile->Attributes[ 10 ] = 0x00000000;
		}

		/* Prevent redoing this */
		pFile->Attributes[ 0xF ] = 0xFFFFFFFF;
	}
}