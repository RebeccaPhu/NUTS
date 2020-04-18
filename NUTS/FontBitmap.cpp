#include "StdAfx.h"
#include "FontBitmap.h"


FontBitmap::FontBitmap( DWORD FontID, const BYTE *pText, const BYTE MaxLen, const bool Ellipsis, const bool Selected )
{
	std::string s = std::string( (char *) pText );

	if ( s.length() > (size_t) MaxLen )
	{
		s = s.substr( 0, (size_t) MaxLen );

		if ( Ellipsis )
		{
			s += "...";
		}
	}

	size_t l = s.length();
	BYTE *pC = (BYTE *) s.c_str();
	WORD sl = l & 0xFC;

	if ( sl < l ) { sl += 4; }

	w = l * 8;
	h = 8;

	pData = malloc( sl * h );
	bmi   = (BITMAPINFO *) malloc( sizeof(BITMAPINFOHEADER) + ( sizeof( RGBQUAD ) * 2U ) );

	BYTE *pBitmap = (BYTE *) pData;

	BYTE *pFontData = (BYTE *) FSPlugins.LoadFont( FontID );

	for ( BYTE r=0; r<8; r++ )
	{
		for ( BYTE b=0; b<l; b++ )
		{
			pBitmap[ (r * sl) + b ] = pFontData[ ( pC[ b ] * 8 ) + r ];
		}
	}

	if ( Selected )
	{
		bmi->bmiColors[ 0 ].rgbBlue  = 0xFF;
		bmi->bmiColors[ 0 ].rgbGreen = 0x00;
		bmi->bmiColors[ 0 ].rgbRed   = 0x00;
		bmi->bmiColors[ 0 ].rgbReserved = 0x00;

		bmi->bmiColors[ 1 ].rgbBlue  = 0xFF;
		bmi->bmiColors[ 1 ].rgbGreen = 0xFF;
		bmi->bmiColors[ 1 ].rgbRed   = 0xFF;
		bmi->bmiColors[ 1 ].rgbReserved = 0x00;
	}
	else
	{
		bmi->bmiColors[ 0 ].rgbBlue  = 0xFF;
		bmi->bmiColors[ 0 ].rgbGreen = 0xFF;
		bmi->bmiColors[ 0 ].rgbRed   = 0xFF;
		bmi->bmiColors[ 0 ].rgbReserved = 0x00;

		bmi->bmiColors[ 1 ].rgbBlue  = 0x00;
		bmi->bmiColors[ 1 ].rgbGreen = 0x00;
		bmi->bmiColors[ 1 ].rgbRed   = 0x00;
		bmi->bmiColors[ 1 ].rgbReserved = 0x00;
	}

	bmi->bmiHeader.biBitCount      = 1U;
	bmi->bmiHeader.biClrImportant  = 0;
	bmi->bmiHeader.biClrUsed       = 2U;
	bmi->bmiHeader.biCompression   = BI_RGB;
	bmi->bmiHeader.biHeight        = 0 - h;
	bmi->bmiHeader.biPlanes        = 1U;
	bmi->bmiHeader.biSize          = sizeof( BITMAPINFOHEADER );
	bmi->bmiHeader.biSizeImage     = sl * h;
	bmi->bmiHeader.biWidth         = w;
	bmi->bmiHeader.biXPelsPerMeter = 0U;
	bmi->bmiHeader.biYPelsPerMeter = 0U;
}

void FontBitmap::SetButtonColor( BYTE r, BYTE g, BYTE b )
{
	bmi->bmiColors[ 0 ].rgbBlue  = b;
	bmi->bmiColors[ 0 ].rgbGreen = g;
	bmi->bmiColors[ 0 ].rgbRed   = r;
}

FontBitmap::~FontBitmap(void)
{
	if ( pData != nullptr )
	{
		free( pData );
	}

	pData = nullptr;

	if ( bmi != nullptr )
	{
		free( bmi );
	}

	bmi = nullptr;
}

void FontBitmap::DrawText( HDC hDC, DWORD x, DWORD y, DWORD Displace )
{
	DWORD dx = x;
	DWORD dy = y;

	if ( Displace & DT_CENTER )
	{
		dx -= w / 2U;
	}

	if ( Displace & DT_RIGHT )
	{
		dx -= w;
	}

	if ( Displace & DT_VCENTER )
	{
		dy -= h;
	}

	if ( Displace & DT_TOP )
	{
		dy -= h * 2U;
	}

	StretchDIBits( hDC, dx, dy, w, h * 2, 0, 0, w, h, pData, bmi, DIB_RGB_COLORS, SRCCOPY );
}