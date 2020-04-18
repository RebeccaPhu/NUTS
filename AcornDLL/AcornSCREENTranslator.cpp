#include "StdAfx.h"
#include "AcornSCREENTranslator.h"

BYTE *AcornSCREENTranslator::pTeletextFont = nullptr;

AcornSCREENTranslator::AcornSCREENTranslator(void)
{
	Mode = 0;
}


AcornSCREENTranslator::~AcornSCREENTranslator(void)
{
}

ModeList AcornSCREENTranslator::GetModes()
{
	ModeList Modes;

	ScreenMode RawModes[] = {
		{ L"Mode 0: 640 x 256 x 2",  0 },
		{ L"Mode 1: 320 x 256 x 4",  0 },
		{ L"Mode 2: 160 x 256 x 16", 0 },
		{ L"Mode 3: 640 x 200 x 2",  0 },
		{ L"Mode 4: 320 x 256 x 2",  0 },
		{ L"Mode 5: 160 x 256 x 4",  0 },
		{ L"Mode 6: 320 x 200 x 2",  0 },
		{ L"Mode 7: Teletext", 0 },
	};

	for (int i=0; i<8; i++) { Modes.push_back( RawModes[i] ); }

	return Modes;
}

int AcornSCREENTranslator::TranslateGraphics( GFXTranslateOptions &Options, CTempFile &FileObj )
{
	Mode = (BYTE) Options.ModeID;

	if ( Options.ModeID == 7 )
	{
		DoTeletextTranslation( Options, FileObj );

		return 0;
	}

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
	Options.bmi->bmiHeader.biHeight			= 256;
	Options.bmi->bmiHeader.biPlanes			= 1;
	Options.bmi->bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	Options.bmi->bmiHeader.biSizeImage		= 640 * 256 * 4;
	Options.bmi->bmiHeader.biWidth			= 640;
	Options.bmi->bmiHeader.biYPelsPerMeter	= 0;
	Options.bmi->bmiHeader.biXPelsPerMeter	= 0;

	int	i	= 0;	//	Byte input
	int	r	= 0;	//	Row counter within 8-byte block
	int	x	= 0;	//	X coordinate
	int	y	= 0;	//	Y coordinate
	int	mx	= 0;	//	Maximum X
	int	my	= 0;	//	Maximum Y
	int	ppb = 0;	//	Pixels per memory byte

	my	= 256;

	switch ( Options.ModeID ) {
		case 0:
		case 3:
			mx	= 640;
			break;

		case 1:
		case 4:
		case 6:
			mx	= 320;
			break;

		case 2:
		case 5:
			mx	= 160;
			break;

		default:
			mx	= 1;
			break;
	}

	pixels	= (DWORD *) realloc(pixels, mx * my * 4);

	ZeroMemory( pixels, mx * my * 4 );

	switch ( Options.ModeID ) {
		case 0:
		case 3:
		case 4:
		case 6:
			ppb	= 8;
			break;

		case 1:
		case 5:
			ppb	= 4;
			break;

		case 2:
			ppb	= 2;
			break;

		default:
			ppb	= 8;
			break;
	}

	BYTE	b;
	BYTE	p1,p2,p3,p4;

	while (i < TotalMemory) {
		b	= pContent[i];

		if (( Options.ModeID == 5 ) || ( Options.ModeID == 1 )) {
			p1	= ((b & 128) >> 6) | ((b & 8) >> 3);
			p2	= ((b &  64) >> 5) | ((b & 4) >> 2);
			p3	= ((b &  32) >> 4) | ((b & 2) >> 1);
			p4	= ((b &  16) >> 3) | ((b & 1) >> 0);

			pixels[((256 - (y + r) - 1) * mx) + x + 0]	= GetColour( p1, &Options );
			pixels[((256 - (y + r) - 1) * mx) + x + 1]	= GetColour( p2, &Options );
			pixels[((256 - (y + r) - 1) * mx) + x + 2]	= GetColour( p3, &Options );
			pixels[((256 - (y + r) - 1) * mx) + x + 3]	= GetColour( p4, &Options );

			r	= r + 1;

			if (r == 8) {
				r	= 0;
				x	= x + ppb;
			}

			if (x >= mx) {
				y	= y + 8;
				x	= 0;
			}

			if (y == 256)
				break;
		}

		if (( Options.ModeID == 0 ) || ( Options.ModeID == 3 ) || ( Options.ModeID == 4 ) || ( Options.ModeID == 6 )) {
			if ((y + r) < 256) {
				for (int p=0; p<8; p++) {
					pixels[((256 - (y + r) - 1) * mx) + x + p] = GetColour( (pContent[i] & (1 << (7 - p))) >> (7 - p), &Options );
				}
			}

			r	= r + 1;

			if (r == 8) {
				r	= 0;
				x	= x + ppb;
			}

			if (x >= mx) {
				x	= 0;
				y	= y + ((( Options.ModeID == 3 ) || ( Options.ModeID == 6 ))?10:8);
			}

			if (y == 256)
				break;
		}

		if ( Options.ModeID == 2 ) {
			p1	= ((b & 128) >> 4) | ((b & 32) >> 3) | ((b & 8) >> 2) | ((b & 2) >> 1);
			p2	= ((b & 64)  >> 3) | ((b & 16) >> 2) | ((b & 4) >> 1) | ((b & 1) >> 0);

			pixels[((256 - (y + r) - 1) * 160) + x + 0] = GetColour( p1, &Options );
			pixels[((256 - (y + r) - 1) * 160) + x + 1] = GetColour( p2, &Options );
	
			r++;

			if (r == 8) {
				r	= 0;
				x	= x + 2;
			}

			if (x >= 160) {
				y	= y + 8;
				x	= 0;
			}

			if (y == 256)
				break;
		}

		i++;
	}

	Options.bmi->bmiHeader.biWidth		= mx;
	Options.bmi->bmiHeader.biHeight		= my;
	Options.bmi->bmiHeader.biSizeImage	= mx * my * 4;

	*Options.pGraphics = (void *) pixels;

	return 0;
}

LogPalette AcornSCREENTranslator::GetLogicalPalette( DWORD ModeID )
{
	LogPalette Logical;

	switch ( ModeID )
	{
	case 0:
	case 3:
	case 4:
	case 6:
		/* two-colour modes */
		Logical[ 0 ] = 0; // Black
		Logical[ 1 ] = 7; // White
		break;

	case 1:
	case 5:
		/* four-colour modes */
		Logical[ 0 ] = 0; // Black
		Logical[ 1 ] = 1; // Red
		Logical[ 2 ] = 3; // Yellow
		Logical[ 3 ] = 7; // White
		break;

	case 2:
		/* Sixteen-colour mode (sort of) */

		for ( WORD i=0; i<16; i++ )
		{
			Logical[ i ] = i;
		}
		break;

	default:
		break;

	}

	return Logical;
}

PhysColours AcornSCREENTranslator::GetPhysicalColours( void )
{
	PhysColours Colours;

	Colours[  0 ] = std::make_pair(0,0); // Black
	Colours[  1 ] = std::make_pair(1,1); // Red
	Colours[  2 ] = std::make_pair(2,2); // Green
	Colours[  3 ] = std::make_pair(3,3); // Yellow
	Colours[  4 ] = std::make_pair(4,4); // Blue
	Colours[  5 ] = std::make_pair(5,5); // Magenta
	Colours[  6 ] = std::make_pair(6,6); // Cyan
	Colours[  7 ] = std::make_pair(7,7); // White

	Colours[  8 ] = std::make_pair(0,7); // Black/White
	Colours[  9 ] = std::make_pair(1,6); // Red/Cyan
	Colours[ 10 ] = std::make_pair(2,5); // Green/Magenta
	Colours[ 11 ] = std::make_pair(3,4); // Yellow/Blue
	Colours[ 12 ] = std::make_pair(4,3); // Blue/Yellow
	Colours[ 13 ] = std::make_pair(5,2); // Magenta/Green
	Colours[ 14 ] = std::make_pair(6,1); // Cyan/Red
	Colours[ 15 ] = std::make_pair(7,0); // White/Black

	return Colours;
}

PhysPalette AcornSCREENTranslator::GetPhysicalPalette( void )
{
	PhysPalette Physical;

	Physical[ 0 ] = (DWORD) RGB( 0x00, 0x00, 0x00 ); // Black
	Physical[ 1 ] = (DWORD) RGB( 0xFF, 0x00, 0x00 ); // Red
	Physical[ 2 ] = (DWORD) RGB( 0x00, 0xFF, 0x00 ); // Green
	Physical[ 3 ] = (DWORD) RGB( 0xFF, 0xFF, 0x00 ); // Yellow
	Physical[ 4 ] = (DWORD) RGB( 0x00, 0x00, 0xFF ); // Blue
	Physical[ 5 ] = (DWORD) RGB( 0xFF, 0x00, 0xFF ); // Magenta
	Physical[ 6 ] = (DWORD) RGB( 0x00, 0xFF, 0xFF ); // Cyan
	Physical[ 7 ] = (DWORD) RGB( 0xFF, 0xFF, 0xFF ); // White

	return Physical;
}

DWORD AcornSCREENTranslator::GuessMode( CTempFile &FileObj )
{
	DWORD Ext = (DWORD) FileObj.Ext();

	if ( Ext <= 0x400 )
	{
		return 7;
	}

	if ( Ext <= 0x2000 )
	{
		return 6;
	}

	if ( Ext <= 0x2800 )
	{
		return 5; // Could also be 4, but this is statistically more likely
	}

	if ( Ext <= 0x4000 )
	{
		return 3;
	}

	if ( Ext <= 0x5000 )
	{
		return 1; // Could also be 0 or 2, but 1 is statistically more likely.
	}

	return 2; // For fun.
}

inline DWORD AcornSCREENTranslator::GetPhysicalColour( BYTE TXCol, GFXTranslateOptions *opts )
{
	DWORD pix;

	pix = opts->pPhysicalPalette->at( TXCol );

	return ( ( pix & 0xFF0000 ) >> 16 ) | ( pix & 0xFF00 ) | ( ( pix & 0xFF ) << 16 );
}

AspectRatio AcornSCREENTranslator::GetAspect( void )
{
	if ( Mode < 7 )
	{
		return std::make_pair<BYTE,BYTE>( 4, 3 );
	}

	return std::make_pair<BYTE,BYTE>( 16, 15 );
}

int AcornSCREENTranslator::DoTeletextTranslation( GFXTranslateOptions &Options, CTempFile &FileObj )
{
	DWORD lContent = (DWORD) FileObj.Ext();

	if ( lContent > (DWORD) Options.Length ) { lContent = (DWORD) Options.Length; }
	if ( lContent > 1024 ) { lContent = 1024; }

	DWORD TotalMemory;

	if ( Options.Offset > 0 )
	{
		FileObj.Seek( Options.Offset );
		TotalMemory = 1024;
	}
	else
	{
		TotalMemory = 1024 + ( 0 - (DWORD) Options.Offset );
		FileObj.Seek( 0 );
	}

	BYTE  *pContent = (BYTE *) malloc( TotalMemory );
	BYTE  *pixels   = nullptr;

	ZeroMemory( pContent, TotalMemory );

	if ( Options.Offset > 0 )
	{
		FileObj.Read( pContent, lContent );
	}
	else
	{
		FileObj.Read( &pContent[ 0 - (DWORD) Options.Offset ], lContent );
	}

	Options.bmi->bmiHeader.biBitCount		= 32;
	Options.bmi->bmiHeader.biClrImportant	= 0;
	Options.bmi->bmiHeader.biClrUsed		= 0;
	Options.bmi->bmiHeader.biCompression	= BI_RGB;
	Options.bmi->bmiHeader.biHeight			= 200;
	Options.bmi->bmiHeader.biPlanes			= 1;
	Options.bmi->bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	Options.bmi->bmiHeader.biSizeImage		= 320 * 200 * 4;
	Options.bmi->bmiHeader.biWidth			= 320;
	Options.bmi->bmiHeader.biYPelsPerMeter	= 0;
	Options.bmi->bmiHeader.biXPelsPerMeter	= 0;

	pixels = (BYTE *) malloc( 320 * 200 * 4 );

	ZeroMemory( pixels, 320 * 200 * 4 );

	bool TXFlash;
	BYTE TXFCol;
	BYTE TXBCol;
	bool TXGraph;
	bool TXContGraph;
	bool TXConceal;
	bool TXDH;
	bool TXHold;
	BYTE holdChar;
	BYTE TXLastDH;
	bool TXDidDH;
	
	DWORD TXCharBase = 0;
	DWORD Pitch      = 320 * 4;

	TXLastDH = 0;

	for ( BYTE ty=0; ty<25; ty++ )
	{
		/* Every line starts with these defaults */
		TXFlash     = false;
		TXFCol      = 7;
		TXBCol      = 0;
		TXGraph     = false;
		TXContGraph = true;
		TXConceal   = false;
		TXDH        = false;
		TXHold      = false;
		holdChar    = 32;
		TXDidDH     = false;

		for ( BYTE tx=0; tx<40; tx++ )
		{
			BYTE ttxChar   = pContent[ TXCharBase ] | 0x80; // Tricksy Acornsoft
			BYTE writeChar = ttxChar & 0x7F;

			if ( ( ttxChar >= 129 ) && ( ttxChar <= 135 ) )
			{
				TXGraph   = false;
				TXFCol    = ttxChar - 128; // Note - no alpha black!
				TXConceal = false;
			}

			if ( ( ttxChar >= 145 ) && ( ttxChar <= 151 ) )
			{
				TXGraph   = true;
				TXFCol    = ttxChar - 144; // Note - no graph black!
				TXConceal = false;
			}

			if ( ttxChar == 157 ) { TXBCol = TXFCol; }

			if ( ttxChar == 141 ) { TXDH = true; TXDidDH = true; }

			if ( ttxChar == 140 ) { TXDH = false; }

			if ( ttxChar == 136 ) { TXFlash = true; }

			if ( ttxChar == 137 ) { TXFlash = false; }

			if ( ttxChar == 153 ) { TXContGraph = true; }

			if ( ttxChar == 154 ) { TXContGraph = false; }

			if ( ttxChar == 152 ) { TXConceal = true; }

			if ( ttxChar == 158 ) { TXHold = true; }

			if ( ttxChar == 159 ) { TXHold = false; }

			if ( ( ( ttxChar & 0x7F ) >=32  ) && ( TXConceal ) ) { writeChar = 32; }

			if ( ( ttxChar & 0x7F ) < 32 ) {
				if ( TXHold )
				{
					writeChar = holdChar;
				}
				else
				{
					writeChar = 32;
				}
			}

			/* Now we draw the character */
			BYTE  *pSrc  = &pTeletextFont[ writeChar * 8 ];

			if ( ( TXGraph ) && ( ( ( writeChar >= 33 ) && ( writeChar <= 63 ) ) || ( ( writeChar >= 95 ) && ( writeChar <= 127 ) ) ) )
			{
				holdChar = writeChar;

				static BYTE gChar[8];

				BYTE Mask = (TXContGraph) ? 0xFF : 0xEE;

				BYTE tr = ( (writeChar & 1 ) ? 240 : 0 )  | ( (writeChar & 2) ? 15 : 0 );
				BYTE mr = ( (writeChar & 4 ) ? 240 : 0 )  | ( (writeChar & 8) ? 15 : 0 );
				BYTE br = ( (writeChar & 16 ) ? 240 : 0 ) | ( (writeChar & 64) ? 15 : 0 );

				gChar[0] = gChar[1] = tr & Mask;
				gChar[3] =            mr & Mask;
				gChar[5] = gChar[6] = br & Mask;

				gChar[2] = (TXContGraph) ? ( tr & Mask ) : 0;
				gChar[4] = (TXContGraph) ? ( mr & Mask ) : 0;
				gChar[7] = (TXContGraph) ? ( br & Mask ) : 0;

				pSrc = gChar;
			}

			for ( BYTE by=0; by<8; by++ )
			{
				WORD  scanline = 199 - ( ty * 8 + by );
				DWORD *pChar = (DWORD *) &pixels[ ( scanline * Pitch ) + ( tx * 8 * 4 ) ];

				BYTE srcpix = pSrc[ by ];

				if ( TXDH )
				{
					srcpix = pSrc[ by >> 1 ];

					if ( TXLastDH > 0 )
					{
						srcpix = pSrc[ ( by >> 1 ) + 4 ];
					}
				}

				for (BYTE bx=7; bx<8; bx-- )
				{
					bool drawFore = false;

					if ( srcpix & ( 1 << bx ) )
					{
						drawFore = true;
					}

					if ( TXFlash )
					{
						if (Options.FlashPhase) { drawFore = false; }
					}

					if ( drawFore )
					{
						*pChar = GetPhysicalColour( TXFCol, &Options );
					}
					else
					{
						*pChar = GetPhysicalColour( TXBCol, &Options );
					}

					pChar++;
				}
			}

			TXCharBase++;
		}

		if ( TXDidDH )
		{
			TXLastDH = 1 - TXLastDH;
		}
		else
		{
			TXLastDH = 0;
		}
	}

	*Options.pGraphics = (void *) pixels;

	return 0;
}