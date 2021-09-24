#include "StdAfx.h"
#include "FontBitmap.h"

#include <assert.h>

FontBitmap::FontBitmap( DWORD FontID, const BYTE *pText, const WORD MaxLen, const bool Ellipsis, const bool Selected )
{
	pData = nullptr;
	bmi   = nullptr;

	WORD sl = MaxLen;
	TextLen = MaxLen;

	if ( MaxLen == 0 )
	{
		w = 0;

		return;
	}

	// 4-byte alignment
	while ( sl % 4 ) { sl++; }

	w = sl * 8;

	pData = malloc( sl * 8 * 8 );
	bmi   = (BITMAPINFO *) malloc( sizeof(BITMAPINFOHEADER) + ( sizeof( RGBQUAD ) * 2U ) );

	BYTE *pBitmap = (BYTE *) pData;

	BYTE *pFontData = (BYTE *) FSPlugins.LoadFont( FontID );

	for ( BYTE r=0; r<8; r++ )
	{
		for ( WORD b=0; b<MaxLen; b++ )
		{
			pBitmap[ (r * sl) + b ] = pFontData[ ( pText[ b ] * 8 ) + r ];
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
	bmi->bmiHeader.biHeight        = 0 - 8;
	bmi->bmiHeader.biPlanes        = 1U;
	bmi->bmiHeader.biSize          = sizeof( BITMAPINFOHEADER );
	bmi->bmiHeader.biSizeImage     = sl * 8;
	bmi->bmiHeader.biWidth         = w;
	bmi->bmiHeader.biXPelsPerMeter = 0U;
	bmi->bmiHeader.biYPelsPerMeter = 0U;
}

void FontBitmap::SetButtonColor( BYTE r, BYTE g, BYTE b )
{
	if ( w == 0 ) { return; }

	bmi->bmiColors[ 0 ].rgbBlue  = b;
	bmi->bmiColors[ 0 ].rgbGreen = g;
	bmi->bmiColors[ 0 ].rgbRed   = r;
}

void FontBitmap::SetTextColor( BYTE r, BYTE g, BYTE b )
{
	if ( w == 0 ) { return; }

	bmi->bmiColors[ 1 ].rgbBlue  = b;
	bmi->bmiColors[ 1 ].rgbGreen = g;
	bmi->bmiColors[ 1 ].rgbRed   = r;
}

void FontBitmap::SetGrayed( bool grayed = true )
{
	if ( w == 0 ) { return; }

	if ( grayed )
	{
		bmi->bmiColors[ 1 ].rgbBlue  = 0xAA;
		bmi->bmiColors[ 1 ].rgbGreen = 0xAA;
		bmi->bmiColors[ 1 ].rgbRed   = 0xAA;
	}
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
	if ( w == 0 ) { return; }

	DWORD rw = TextLen * 8;
	DWORD dx = x;
	DWORD dy = y;

	if ( Displace & DT_CENTER )
	{
		dx -= rw / 2U;
	}

	if ( Displace & DT_RIGHT )
	{
		dx -= rw;
	}

	if ( Displace & DT_VCENTER )
	{
		dy -= 8;
	}

	if ( Displace & DT_TOP )
	{
		dy -= 8 * 2U;
	}

	StretchDIBits( hDC, dx, dy, TextLen * 8, 8 * 2, 0, 0, TextLen * 8, 8, pData, bmi, DIB_RGB_COLORS, SRCCOPY );
}
