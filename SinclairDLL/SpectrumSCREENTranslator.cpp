#include "StdAfx.h"
#include "SpectrumSCREENTranslator.h"


SpectrumSCREENTranslator::SpectrumSCREENTranslator(void)
{
}


SpectrumSCREENTranslator::~SpectrumSCREENTranslator(void)
{
}

int SpectrumSCREENTranslator::TranslateGraphics( GFXTranslateOptions &Options, CTempFile &FileObj )
{
	QWORD  lContent   = FileObj.Ext();
	DWORD *pixels     = nullptr;
	long  TotalMemory = (long) lContent;
	      TotalMemory-= (long) Options.Offset;
	BYTE  *pContent   = (BYTE *) malloc( TotalMemory );

	ZeroMemory( pContent, TotalMemory );

	if ( Options.Offset >= 0 )
	{
		FileObj.Seek( Options.Offset );
		FileObj.Read( pContent, (DWORD) lContent - (long) Options.Offset );
	}
	else
	{
		FileObj.Seek( 0 );
		FileObj.Read( &pContent[ (DWORD) abs( Options.Offset) ], (DWORD) Options.Length );
	}

	Options.bmi->bmiHeader.biBitCount		= 32;
	Options.bmi->bmiHeader.biClrImportant	= 0;
	Options.bmi->bmiHeader.biClrUsed		= 0;
	Options.bmi->bmiHeader.biCompression	= BI_RGB;
	Options.bmi->bmiHeader.biHeight			= 192;
	Options.bmi->bmiHeader.biPlanes			= 1;
	Options.bmi->bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	Options.bmi->bmiHeader.biSizeImage		= 256 * 192 * 4;
	Options.bmi->bmiHeader.biWidth			= 256;
	Options.bmi->bmiHeader.biYPelsPerMeter	= 0;
	Options.bmi->bmiHeader.biXPelsPerMeter	= 0;

	int	fg  = 0;
	int bg  = 0;
	int amp = 0;

	int	sr	= 0;
	int	bit = 0;
	WORD i = 0;
	const WORD mx = 256;
	const WORD my = 192;
	WORD  x = 0;
	WORD  y = 0;
	BYTE  r = 0;

	pixels	= (DWORD *) realloc(pixels, mx * my * 4);

	while ( i < (WORD) lContent ) {
		amp	= (((y / 8) * 32) + (x / 8)) + 6144;

		if (amp < lContent) {
			fg	= (pContent[amp] & 7) | ((pContent[amp] & 64) >> 3);
			bg	= ((pContent[amp] & 56) >> 3) | ((pContent[amp] & 64) >> 3);

			if (( Options.FlashPhase ) && ( pContent[amp] & 128 ) ) {
				bit	= fg;
				fg	= bg;
				bg	= bit;
			}
		}

		for (int p=0; p<8; p++) {
			bit	= (pContent[i] & (1 << (7 - p))) >> (7 - p);

			if (bit)
				pixels[((my - y - 1) * mx) + x + p] = GetColour( fg, &Options );
			else
				pixels[((my - y - 1) * mx) + x + p] = GetColour( bg, &Options );
		}

		x = x + 8;

		if (x == 256) {
			x = 0;
			y = y + 8;
			r = r + 1;
		}

		if (r == 8) {
			r  = 0;
			y  = y - 63;
			sr = sr + 1;
		}

		if (sr == 8) {
			sr = 0;
			y  = y + 56;
		}

		if (y >= 192) {
			y = 191;
		}

		i++;

		if (i >= 6144)
			break;
	}

	*Options.pGraphics = (void *) pixels;

	return 0;
}

PhysPalette SpectrumSCREENTranslator::GetPhysicalPalette( void )
{
	PhysPalette Physical;

	Physical[0]  = (DWORD) RGB( 0x00, 0x00, 0x00 );
	Physical[1]  = (DWORD) RGB( 0x00, 0x00, 0xCF );
	Physical[2]  = (DWORD) RGB( 0xCF, 0x00, 0x00 );
	Physical[3]  = (DWORD) RGB( 0xCF, 0x00, 0xCF );
	Physical[4]  = (DWORD) RGB( 0x00, 0xCF, 0x00 );
	Physical[5]  = (DWORD) RGB( 0x00, 0xCF, 0xCF );
	Physical[6]  = (DWORD) RGB( 0xCF, 0xCF, 0x00 );
	Physical[7]  = (DWORD) RGB( 0xCF, 0xCF, 0xCF );
	Physical[8]  = (DWORD) RGB( 0x00, 0x00, 0x00 );
	Physical[9]  = (DWORD) RGB( 0x00, 0x00, 0xFF );
	Physical[10] = (DWORD) RGB( 0xFF, 0x00, 0x00 );
	Physical[11] = (DWORD) RGB( 0xFF, 0x00, 0xFF );
	Physical[12] = (DWORD) RGB( 0x00, 0xFF, 0x00 );
	Physical[13] = (DWORD) RGB( 0x00, 0xFF, 0xFF );
	Physical[14] = (DWORD) RGB( 0xFF, 0xFF, 0x00 );
	Physical[15] = (DWORD) RGB( 0xFF, 0xFF, 0xFF );


	return Physical;
}

PhysColours SpectrumSCREENTranslator::GetPhysicalColours( void )
{
	PhysColours Colours;

	for ( BYTE i=0; i<16; i++ )
	{
		Colours[ i ] = std::make_pair<WORD,WORD>( i, i );
	}

	return Colours;
}

