#include "StdAfx.h"
#include "InfoIcon.h"
#include "../NUTS/libfuncs.h"

static BYTE *none = (BYTE *) "";

InfoIcon::InfoIcon( CTempFile &fileObj )
{
	pIcon     = nullptr;
	pIconTool = none;

	BYTE pIconData[ 8192 ];

	fileObj.Seek( 0 );
	fileObj.Read( pIconData, fileObj.Ext() );

	LoadIcon( pIconData, fileObj.Ext() );
}

InfoIcon::InfoIcon( BYTE *pIconData, DWORD DataLength )
{
	pIcon     = nullptr;
	pIconTool = none;

	LoadIcon( pIconData, DataLength );
}

InfoIcon::~InfoIcon(void)
{
	if ( pIcon != nullptr )
	{
		free( pIcon );
	}

	if ( pIconTool != none )
	{
		free( pIconTool );
	}
}

void InfoIcon::LoadIcon( BYTE *pIconData, DWORD DataLength )
{
	HasIcon = false;

	WORD Magic = BEWORD( pIconData );

	if ( Magic != 0xE310 )
	{
		return;
	}

	IconWidth  = BEWORD( &pIconData[ 12 ] );
	IconHeight = BEWORD( &pIconData[ 14 ] );

	IconVersion = BEDWORD( &pIconData[ 44 ] );

	DWORD Icon1DWORD = BEDWORD( &pIconData[ 22 ] );
	DWORD Icon2DWORD = BEDWORD( &pIconData[ 26 ] );

	if ( Icon1DWORD == 0 )
	{
		return;
	}

	DWORD DrawerDWORD = BEDWORD( &pIconData[ 66 ] );

	BYTE *pImage = &pIconData[ 78 ];

	if ( DrawerDWORD != 0) { pImage += 56; } /* Skip that */

	pIcon = (BYTE *) malloc( IconWidth * IconHeight );

	/* Now we have the pointer to the icon data, but the dimensions may not be the same.
	   Those above are authoritative for presentation, but the ones here define data storage. */

	WORD DataWidth  = BEWORD( &pImage[ 4 ] );
	WORD DataHeight = BEWORD( &pImage[ 6 ] );
	WORD DataPlanes = BEWORD( &pImage[ 8 ] );
	WORD DataLeft   = BEWORD( &pImage[ 0 ] );
	WORD DataTop    = BEWORD( &pImage[ 2 ] );

	BYTE *pPlanes = &pImage[ 20 ];

	DWORD PlaneWidth = ( ( DataWidth + 15 ) >> 4) << 1;
	DWORD PlaneSize  = ( PlaneWidth * DataHeight );

	ZeroMemory( pIcon, IconWidth * IconHeight );

	for ( WORD py=0; py<DataHeight; py++ )
	{
		for (WORD px=0; px<DataWidth; px++ )
		{
			int oy = DataTop + py;
			int ox = DataLeft + px;

			BYTE *pTarget = &pIcon[ oy * IconWidth + ox ];

			BYTE TargetPix = 0;

			for ( BYTE bit=DataPlanes; bit != 0x00; bit-- )
			{
				BYTE *pPlane = &pPlanes[ ( PlaneSize * (DataPlanes - bit ) ) + ( py * PlaneWidth )];

				WORD PixByte = px / 8;
				BYTE PixBit  = 7 - (px % 8 );

				TargetPix |= ( ( pPlane[PixByte] & ( 1 << PixBit ) ) >> PixBit ) << bit;
			}

			if ( ( oy>=0 ) && ( oy<IconHeight ) )
			{
				if ( ( ox>=0 ) && ( ox<IconWidth ) )
				{
					*pTarget = TargetPix;
				}
			}
		}
	}

	HasIcon = true;

	BYTE *pToolLocation = pPlanes + (PlaneSize * DataPlanes );

	if ( Icon2DWORD != 0 )
	{
		/* Skip this icon, as we don't use it */
		WORD Width2  = BEWORD( &pToolLocation[ 4 ] );
		WORD Height2 = BEWORD( &pToolLocation[ 6 ] );
		WORD Planes2 = BEWORD( &pToolLocation[ 8 ] );

		DWORD PlaneWidth2 = ( ( Width2 + 15 ) >> 4) << 1;
		DWORD PlaneSize2  = PlaneWidth2 * Planes2;

		pToolLocation += (PlaneSize2 * Planes2 ) + 20;
	}

	DWORD ToolPosition  = ( pToolLocation - pIconData );

	if ( ToolPosition < ( DataLength - 5 ) )
	{
		pIconTool = rstrndup( &pToolLocation[4], BEDWORD( pToolLocation ) );
	}
}

int InfoIcon::GetNaturalBitmap( BITMAPINFOHEADER *bmi, BYTE **pPixels )
{
	BYTE *pixels = (BYTE *) malloc( IconWidth * IconHeight * 4 );

	*pPixels = pixels;

	bmi->biBitCount     = 32;
	bmi->biClrImportant = 0;
	bmi->biClrUsed      = 0;
	bmi->biCompression  = BI_RGB;
	bmi->biHeight       = (LONG) IconHeight;
	bmi->biPlanes       = 1;
	bmi->biSize         = sizeof( BITMAPINFOHEADER );
	bmi->biSizeImage    = IconWidth * IconHeight * 4;
	bmi->biWidth        = (LONG) IconWidth;

	DWORD Palette[ 8 ];

	if ( IconVersion == 0 )
	{
		Palette[ 0 ] = RGB( 0x55, 0xaa, 0xFF );
		Palette[ 1 ] = RGB( 0xFF, 0xFF, 0xFF );
		Palette[ 2 ] = RGB( 0x00, 0x00, 0x00 );
		Palette[ 3 ] = RGB( 0xFF, 0x88, 0x00 );
	}
	else
	{
		Palette[ 0 ] = RGB( 0x95, 0x95, 0x95 );
		Palette[ 1 ] = RGB( 0x00, 0x00, 0x00 );
		Palette[ 2 ] = RGB( 0xFF, 0xFF, 0xFF );
		Palette[ 3 ] = RGB( 0x3B, 0x67, 0xA2 );
	}

	Palette[ 4 ] = RGB( 0x7B, 0x7B, 0x7B );
	Palette[ 5 ] = RGB( 0xAF, 0xAF, 0xAF );
	Palette[ 6 ] = RGB( 0xAA, 0x90, 0x7C );
	Palette[ 7 ] = RGB( 0xFF, 0xA9, 0x97 );

	for ( WORD py=0; py<IconHeight; py++ )
	{
		for ( WORD px=0; px<IconWidth; px++ )
		{
			DWORD *pTarget = (DWORD *) &pixels[ ( py * IconWidth * 4) + ( px * 4 ) ];
			BYTE  *pSource = &pIcon[ py * IconWidth + px ];
			DWORD PixDat   = Palette[ (*pSource) & 7 ];

			PixDat = ( ( PixDat & 0xFF0000 ) >> 16 ) | ( PixDat & 0xFF00 ) | ( ( PixDat & 0xFF ) << 16 );

			*pTarget = PixDat;
		}
	}

	return 0;
}