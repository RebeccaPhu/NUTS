#include "StdAfx.h"
#include "Sprite.h"

BYTE Sprite::BPPs[ 128 ] = {
	1, 2, 4, 1, 1, 2, 1, 4, // Technically Mode 7 can't support sprites
	2, 4, 8, 2, 4, 8, 4, 8,
	4, 4, 1, 2, 4, 8, 4, 1,
	8, 1, 2, 4, 8, 1, 2, 4,
	8, 1, 2, 4, 8, 1, 2, 4,
	8, 1, 2, 4, 1, 2, 4, 8,
	4, 8, 1, 2, 4, 8
};

AspectRatio Sprite::Res[ 128 ] = {
	AspectRatio( 640, 256 ),
	AspectRatio( 320, 256 ),
	AspectRatio( 160, 256 ),
	AspectRatio( 1, 1 ),
	AspectRatio( 320, 256 ),
	AspectRatio( 160, 256 ),
	AspectRatio( 1, 1 ),
	AspectRatio( 1, 1 ),

	AspectRatio( 640, 256 ),
	AspectRatio( 320, 256 ),
	AspectRatio( 160, 256 ),
	AspectRatio( 640, 250 ),
	AspectRatio( 640, 256 ),
	AspectRatio( 320, 256 ),
	AspectRatio( 640, 250 ),
	AspectRatio( 640, 256 ),

	AspectRatio( 1056, 256 ),
	AspectRatio( 1056, 250 ),
	AspectRatio( 640, 512 ),
	AspectRatio( 640, 512 ),
	AspectRatio( 640, 512 ),
	AspectRatio( 640, 512 ),
	AspectRatio( 768, 288 ),
	AspectRatio( 1152, 896 ),

	AspectRatio( 1056, 256 ),
	AspectRatio( 640, 480 ),
	AspectRatio( 640, 480 ),
	AspectRatio( 640, 480 ),
	AspectRatio( 640, 480 ),
	AspectRatio( 800, 600 ),
	AspectRatio( 800, 600 ),
	AspectRatio( 800, 600 ),

	AspectRatio( 800, 600 ),
	AspectRatio( 768, 288 ),
	AspectRatio( 768, 288 ),
	AspectRatio( 768, 288 ),
	AspectRatio( 768, 288 ),
	AspectRatio( 896, 352 ),
	AspectRatio( 896, 352 ),
	AspectRatio( 896, 352 ),

	AspectRatio( 896, 352 ),
	AspectRatio( 640, 352 ),
	AspectRatio( 640, 352 ),
	AspectRatio( 640, 352 ),
	AspectRatio( 640, 200 ),
	AspectRatio( 640, 200 ),
	AspectRatio( 640, 200 ),
	AspectRatio( 360, 480 ),

	AspectRatio( 320, 480 ),
	AspectRatio( 320, 480 ),
	AspectRatio( 320, 240 ),
	AspectRatio( 320, 240 ),
	AspectRatio( 320, 240 ),
	AspectRatio( 320, 240 ),
	
};

Sprite::Sprite( void *pSprite, DWORD SpriteSize )
{
	LoadSprite( pSprite, SpriteSize );
}

Sprite::Sprite( CTempFile &FileObj )
{
	DWORD Size    = (DWORD) FileObj.Ext();
	void *pBuffer = malloc( (size_t) Size );

	FileObj.Seek( 0 );

	FileObj.Read( pBuffer, (DWORD) Size );

	LoadSprite( pBuffer, Size );

	free( pBuffer );
}

Sprite::~Sprite(void)
{
	/* These are the internal data structures - not those return to the consumer! */
	if ( pBitmap )
	{
		free( pBitmap );
	}

	if ( pMask )
	{
		free( pMask );
	}
}

int Sprite::LoadSprite( void *pSprite, DWORD SpriteSize )
{
	BYTE *pSpriteData = (BYTE *) pSprite;

	/* Lets get some data first */

	/* First 16 bytes are the size and the offset to next sprite, which are irrelevant here */
	DWORD WidthWords = * (DWORD *) &pSpriteData[ 0x10 ];
	DWORD ScanLines  = * (DWORD *) &pSpriteData[ 0x14 ];
	DWORD LeftBit    = * (DWORD *) &pSpriteData[ 0x18 ];
	DWORD RightBit   = * (DWORD *) &pSpriteData[ 0x1C ];
	DWORD SprOffset  = * (DWORD *) &pSpriteData[ 0x20 ];
	DWORD MskOffset  = * (DWORD *) &pSpriteData[ 0x24 ];
	      Mode       = * (DWORD *) &pSpriteData[ 0x28 ];

	SpriteAspect = Res[ Mode ];

	if ( SprOffset == MskOffset )
	{
		HasMask = false;
	}
	else
	{
		HasMask = true;
	}

	if ( ( SprOffset != 0x0000002C ) && ( MskOffset != 0x0000002C ) )
	{
		/* We got a palette. Format is $ENTRIES running from 0x0000002C until
		   we hit SprOffset or MskOffset. Each entry is 8 bytes long, consisting
		   of two sets of 4 bytes, in turn made of S, R, G, B, where S is spare. */
		DWORD PaletteOffset = 0x0000002C;

		DWORD Entry = 0;

		while ( ( ( PaletteOffset < SprOffset ) || ( PaletteOffset < MskOffset ) ) && ( Entry < 256 ) )
		{
			BYTE *pEntry = &pSpriteData[ PaletteOffset ];

			SpritePalette[ Entry ] = (DWORD) RGB( pEntry[ 1 ], pEntry[ 2 ], pEntry [ 3 ] );

			PaletteOffset += 8;
			Entry++;
		}

		SuppliedPaletteEntries = (WORD) Entry;
	}
	else
	{
		SuppliedPaletteEntries = 0;

		DWORD Entries = 0;

		DWORD *Palette = GetPalette( Mode, &Entries );

		memcpy( SpritePalette, Palette, Entries * sizeof( DWORD ) );

		free( Palette );
	}

	/* The resolutions are quirky. The vertical is easy enough, add one to ScanLines and you're there.
	   But horizontal resolution is calculated from the number bits per pixel divided into the number of
	   words, which gives the pitch. Then the right-bit tells you the right-most pixel that is actually in use. */

	DWORD bpp   = (DWORD) BPPs[ (BYTE) Mode ];
	DWORD pitch = ( WidthWords + 1 ) * 4;

	Width  = ( ( WidthWords * 32 ) + RightBit + 1 - LeftBit ) / bpp;
	Height = ScanLines + 1;

	pBitmap = malloc( Width * Height );
	pMask   = malloc( Width * Height );

	BYTE *pBitmapData = (BYTE *) pBitmap;
	BYTE *pMaskData   = (BYTE *) pMask;

	/* Now munge the bits. We will create a byte map of colours here. */
	for ( DWORD py=0; py<(WORD) Height; py++ )
	{
		DWORD SrcOffset = SprOffset + ( py * pitch );
		DWORD MskSrc    = MskOffset + ( py * pitch );

		for ( DWORD px=0; px<(WORD) Width; px++ )
		{
			BYTE *pPixel = &pBitmapData[ py * Width + px ];
			BYTE *pMask  = &pMaskData[ py * Width + px ];

			if ( bpp == 1 )
			{
				DWORD Offset = px / 8;
				BYTE PixBit  = 1 << ( px % 8 );

				if ( pSpriteData[ SrcOffset + Offset ] & PixBit )
				{
					*pPixel = 0;
				}
				else
				{
					*pPixel = 1;
				}

				if ( pSpriteData[ MskSrc + Offset ] & PixBit )
				{
					*pMask = 0xFF;
				}
				else
				{
					*pMask = 0x00;
				}
			}

			if ( bpp == 2 )
			{
				DWORD Offset = px / 4;
				BYTE Pix     = pSpriteData[ SrcOffset + Offset ];
				BYTE Msk     = pSpriteData[ MskSrc + Offset ];

				switch ( px & 3 )
				{
				case 3:
					*pPixel = ( Pix & 0xC0 ) >> 6;
					*pMask  = ( Msk & 0xC0 ) >> 6;
					break;
				case 2:
					*pPixel = ( Pix & 0x30 ) >> 4;
					*pMask  = ( Msk & 0x30 ) >> 4;
					break;
				case 1:
					*pPixel = ( Pix & 0x0C ) >> 2;
					*pMask  = ( Msk & 0x0C ) >> 2;
					break;
				case 0:
					*pPixel = Pix & 0x03;
					*pMask  = Msk & 0x03;
					break;
				}
			}

			if ( bpp == 4 )
			{
				DWORD Offset = px / 2;
				BYTE Pix     = pSpriteData[ SrcOffset + Offset ];
				BYTE Msk     = pSpriteData[ MskSrc + Offset ];

				if ( px & 1 )
				{
					*pPixel = ( Pix & 0xF0 ) >> 4;
					*pMask  =   Msk & 0xF0;
				}
				else
				{
					*pPixel = Pix & 0x0F;
					*pMask  = Msk & 0x0F;
				}
			}

			if ( bpp == 8 )
			{
				*pPixel = pSpriteData[ SrcOffset + px ];
				*pMask  = pSpriteData[ MskSrc + px ];
			}
		}
	}

	return 0;
}

/* Technically this is the default palette, not the one that came with the sprite */
DWORD *Sprite::GetPalette( DWORD ModeID, DWORD *Entries )
{
	*Entries = 1 << BPPs[ ModeID ];

	DWORD *Palette = (DWORD *) malloc( (*Entries) * sizeof( DWORD ) );

	switch( BPPs[ ModeID ] )
	{
	case 1:
		Palette[ 0 ] = (DWORD) RGB( 0x00, 0x00, 0x00 );
		Palette[ 1 ] = (DWORD) RGB( 0xFF, 0xFF, 0xFF );
		break;
	case 2:
		Palette[ 0 ] = (DWORD) RGB( 0xFF, 0xFF, 0xFF );
		Palette[ 1 ] = (DWORD) RGB( 0xC0, 0xC0, 0xC0 );
		Palette[ 2 ] = (DWORD) RGB( 0x80, 0x80, 0x80 );
		Palette[ 3 ] = (DWORD) RGB( 0x00, 0x00, 0x00 );
		break;
	case 4:
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
		break;

	case 8:
		/* Ho boy */
		for (WORD i=0; i<256; i++)
		{
			BYTE PalColour = (BYTE) i;

			Palette[ i ] = RGB(
				( ( PalColour & 1 ) | ( PalColour & 2 ) | ( PalColour & 4 ) | ( ( PalColour & 16 ) >> 1 ) ) << 4,
				( ( PalColour & 1 ) | ( PalColour & 2 ) | ( ( PalColour & 32 ) >> 3 ) | ( ( PalColour & 64 ) >> 3 ) ) << 4,
				( ( PalColour & 1 ) | ( PalColour & 2 ) | ( ( PalColour & 8 ) >> 1 ) | ( ( PalColour & 128 ) >> 4 ) ) << 4
			);
//			Palette[ i ] = RGB(
//				( ( PalColour & 128 ) >> 6 ) | ( ( PalColour & 64 ) >> 6 ) | ( ( PalColour & 1  ) << 2 ) | ( ( PalColour & 2  ) << 2 ),
//				( ( PalColour & 128 ) >> 6 ) | ( ( PalColour & 64 ) >> 6 ) | ( ( PalColour & 4  ) << 0 ) | ( ( PalColour & 8  ) << 0 ),
//				( ( PalColour & 128 ) >> 6 ) | ( ( PalColour & 64 ) >> 6 ) | ( ( PalColour & 16 ) >> 3 ) | ( ( PalColour & 32 ) >> 2 )
//			);
		}
	}

	return Palette;
}

int Sprite::GetNaturalBitmap( BITMAPINFOHEADER *bmi, void **pImage, DWORD MaskColour )
{
	*pImage = malloc( Width * Height * 4 );

	DWORD Entries;
	DWORD *Palette = GetPalette( Mode, &Entries );

	bmi->biBitCount     = 32;
	bmi->biClrImportant = 0;
	bmi->biClrUsed      = 0;
	bmi->biCompression  = BI_RGB;
	bmi->biHeight       = Height;
	bmi->biPlanes       = 1;
	bmi->biSize         = sizeof( BITMAPINFOHEADER );
	bmi->biSizeImage    = Width * Height * 4;
	bmi->biWidth        = Width;

	BYTE *pSrcData   = (BYTE *) pBitmap;
	BYTE *pMskData   = (BYTE *) pMask;
	BYTE *pImageData = (BYTE *) *pImage;

	for (DWORD py=0; py<Height; py++)
	{
		DWORD by = ( Height - 1) - py;

		for (DWORD px=0; px<Width; px++)
		{
			BYTE  *pSrc  = &pSrcData[ py * Width + px ];
			BYTE  *pMsk  = &pMskData[ py * Width + px ];
			DWORD *pDest = (DWORD *) &pImageData[ (py * Width * 4) + (px * 4) ];

			DWORD PixDat = 0xFFFFFFFF;

			BYTE ColIndex = (*pSrc) & ( ( 1 << BPPs[ Mode ] ) - 1 );

			if ( ColIndex > Entries ) { ColIndex = 0; }

			if ( ( MaskColour != 0xFF000000 ) && (HasMask) )
			{
				BYTE MskIndex = *pMsk;

				if ( MskIndex != 0 )
				{
					PixDat = SpritePalette[ ColIndex ];
				}
				else
				{
					PixDat = MaskColour;
				}
			}
			else
			{
				PixDat = SpritePalette[ ColIndex ];
			}

			PixDat = ( ( PixDat & 0xFF0000 ) >> 16 ) | ( PixDat & 0xFF00 ) | ( ( PixDat & 0xFF ) << 16 );
			
			*pDest = PixDat;
		}
	}

	free( Palette );

	return 0;
}

bool Sprite::Valid( void )
{
	if ( pBitmap != nullptr )
	{
		return true;
	}

	return false;
}