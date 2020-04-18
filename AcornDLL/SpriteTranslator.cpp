#include "StdAfx.h"
#include "SpriteTranslator.h"
#include "Sprite.h"

SpriteTranslator::SpriteTranslator(void)
{
}


SpriteTranslator::~SpriteTranslator(void)
{
}

ModeList SpriteTranslator::GetModes()
{
	ModeList ModeList;

	ScreenMode Modes[] = {
		{ L"2 Colour Sprite",  0 },
		{ L"4 Colour Sprite",  1 },
		{ L"16 Colour Sprite", 2 },
		{ L"256 Colour Sprite", 4 },
	};

	return ModeList;
}

int SpriteTranslator::TranslateGraphics( GFXTranslateOptions &Options, CTempFile &FileObj )
{
	Sprite sprite( FileObj );

	Options.bmi->bmiHeader.biBitCount     = 32;
	Options.bmi->bmiHeader.biClrImportant = 0;
	Options.bmi->bmiHeader.biClrUsed      = 0;
	Options.bmi->bmiHeader.biCompression  = BI_RGB;
	Options.bmi->bmiHeader.biHeight       = sprite.Height;
	Options.bmi->bmiHeader.biPlanes       = 1;
	Options.bmi->bmiHeader.biSize         = sizeof( BITMAPINFOHEADER );
	Options.bmi->bmiHeader.biSizeImage    = sprite.Height * sprite.Width * 4;
	Options.bmi->bmiHeader.biWidth        = sprite.Width;

	DWORD Multiplier = 1;

	/* TODO: Mode ratios properly */
	if ( ( sprite.Mode == 12 ) || ( sprite.Mode == 15 ) )
	{
		Multiplier = 2;
	}

	BYTE bpp = Sprite::BPPs[ sprite.Mode ];

	DWORD Pal256[ 256 ];

	for (WORD i=0; i<256; i++)
	{
		BYTE PalColour = (BYTE) i;

		Pal256[ i ] = RGB(
			( ( PalColour & 1 ) | ( PalColour & 2 ) | ( PalColour & 4 ) | ( ( PalColour & 16 ) >> 1 ) ) << 4,
			( ( PalColour & 1 ) | ( PalColour & 2 ) | ( ( PalColour & 32 ) >> 3 ) | ( ( PalColour & 64 ) >> 3 ) ) << 4,
			( ( PalColour & 1 ) | ( PalColour & 2 ) | ( ( PalColour & 8 ) >> 1 ) | ( ( PalColour & 128 ) >> 4 ) ) << 4
		);

//		Pal256[ i ] = RGB(
//			( ( PalColour & 128 ) >> 6 ) | ( ( PalColour & 64 ) >> 6 ) | ( ( PalColour & 1  ) << 2 ) | ( ( PalColour & 2  ) << 2 ),
//			( ( PalColour & 128 ) >> 6 ) | ( ( PalColour & 64 ) >> 6 ) | ( ( PalColour & 4  ) << 0 ) | ( ( PalColour & 8  ) << 0 ),
//			( ( PalColour & 128 ) >> 6 ) | ( ( PalColour & 64 ) >> 6 ) | ( ( PalColour & 16 ) >> 3 ) | ( ( PalColour & 32 ) >> 2 )
//		);
	}
	
	BYTE *pixels = (BYTE *) malloc( sprite.Height * sprite.Width * 4 * Multiplier );

	for ( WORD py=0; py< sprite.Height; py++ )
	{
		for ( WORD px=0; px< sprite.Width;px++ )
		{
			DWORD *pPixel;
			WORD  by   = ( ( (WORD) sprite.Height - 1) - py ) * Multiplier; // Windows. *sigh*.
			BYTE *pSrc = (BYTE *) sprite.pBitmap;

			pPixel = (DWORD *) &pixels[ ( by * (WORD) sprite.Width * 4 ) + ( px * 4 ) ];

			if ( bpp <= 4 )
			{
				*pPixel = GetPhysicalColour( pSrc[ py * sprite.Width + px ], &Options );
			}
			else
			{
				DWORD PixDat = Pal256[ pSrc[ py * sprite.Width + px ] ];

				PixDat = ( ( PixDat & 0xFF0000 ) >> 16 ) | ( PixDat & 0xFF00 ) | ( ( PixDat & 0xFF ) << 16 );

				*pPixel = PixDat;
			}

			for ( BYTE i=0; i< (Multiplier - 1); i++ )
			{
				pPixel += sprite.Width;

				if ( bpp <= 4 )
				{
					*pPixel = GetPhysicalColour( pSrc[ py * sprite.Width + px ], &Options );
				}
				else
				{
					DWORD PixDat = Pal256[ pSrc[ py * sprite.Width + px ] ];

					PixDat = ( ( PixDat & 0xFF0000 ) >> 16 ) | ( PixDat & 0xFF00 ) | ( ( PixDat & 0xFF ) << 16 );

					*pPixel = PixDat;
				}
			}
		}
	}

	Options.bmi->bmiHeader.biHeight    *= Multiplier;
	Options.bmi->bmiHeader.biSizeImage *= Multiplier;

	*Options.pGraphics = pixels;

	return 0;
}

LogPalette SpriteTranslator::GetLogicalPalette( DWORD ModeID )
{
	LogPalette Palette;

	return Palette;
}

PhysPalette SpriteTranslator::GetPhysicalPalette( void )
{
	PhysPalette Palette;

	// Mode 12 ( 16-colour) palette
	Palette[  0 ] = (DWORD) RGB( 0xF0, 0xF0, 0xF0 );
	Palette[  1 ] = (DWORD) RGB( 0xE0, 0xE0, 0xE0 );
	Palette[  2 ] = (DWORD) RGB( 0xC0, 0xC0, 0xC0 );
	Palette[  3 ] = (DWORD) RGB( 0xA0, 0xA0, 0xA0 );
	Palette[  4 ] = (DWORD) RGB( 0x80, 0x80, 0x80 );
	Palette[  5 ] = (DWORD) RGB( 0x60, 0x60, 0x60 );
	Palette[  6 ] = (DWORD) RGB( 0x40, 0x40, 0x40 );
	Palette[  7 ] = (DWORD) RGB( 0x10, 0x10, 0x10 );

	Palette[  8 ] = (DWORD) RGB( 0x00, 0x00, 0xC0 );
	Palette[  9 ] = (DWORD) RGB( 0xF0, 0xF0, 0x00 );
	Palette[ 10 ] = (DWORD) RGB( 0x00, 0xC0, 0x00 );
	Palette[ 11 ] = (DWORD) RGB( 0xD0, 0x00, 0x00 );
	Palette[ 12 ] = (DWORD) RGB( 0xF0, 0xF0, 0xB0 );
	Palette[ 13 ] = (DWORD) RGB( 0xC0, 0x90, 0x00 );
	Palette[ 14 ] = (DWORD) RGB( 0xF0, 0xB0, 0x00 );
	Palette[ 15 ] = (DWORD) RGB( 0x00, 0xC0, 0xE0 );

	return Palette;
}

PhysColours SpriteTranslator::GetPhysicalColours( void )
{
	PhysColours Colours;

	/* Valid for mode 12 */
	for ( BYTE i=0; i<15; i++ )
	{
		Colours[ i ] = std::make_pair<WORD,WORD>( i, i );
	}

	return Colours;
}

DWORD SpriteTranslator::GuessMode( CTempFile &FileObj )
{
	DWORD Mode = 2;

	FileObj.Seek( 0x28 );
	FileObj.Read( (void *) &Mode, 4 );

	switch ( Sprite::BPPs[ Mode ] )
	{
	case 1:
		Mode = 0;
		break;

	case 2:
		Mode = 1;
		break;

	case 4:
		Mode = 2;
		break;

	case 8:
		Mode = 3;
		break;
	}
	
	return Mode;
}

AspectRatio SpriteTranslator::GetAspect( void )
{
	return AspectRatio( 4, 3 );
}

inline DWORD SpriteTranslator::GetPhysicalColour( BYTE TXCol, GFXTranslateOptions *opts )
{
	DWORD pix;

	pix = opts->pPhysicalPalette->at( TXCol );

	return ( ( pix & 0xFF0000 ) >> 16 ) | ( pix & 0xFF00 ) | ( ( pix & 0xFF ) << 16 );
}

