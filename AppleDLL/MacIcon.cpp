#include "stdafx.h"

#include "MacIcon.h"
#include "../NUTS/libfuncs.h"

#include <MMSystem.h>

void MakeIconFromData( BYTE *InBuffer, IconDef *pIcon )
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

void ExtractIcon( NativeFile *pFile, CTempFile &fileobj, Directory *pDirectory )
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

			pDirectory->ResolvedIcons[ pFile->fileID ] = icon;

			pFile->HasResolvedIcon = true;
		}
	}
}

