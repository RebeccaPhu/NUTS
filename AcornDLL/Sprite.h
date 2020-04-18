#pragma once

#include "../NUTS/Tempfile.h"
#include "../NUTS/Defs.h"

class Sprite
{
public:
	Sprite( void *pSprite, DWORD SpriteSize );
	Sprite( CTempFile &FileObj );
	~Sprite(void);

private:
	int LoadSprite( void *pSprite, DWORD SpriteSize );

public:
	void *pBitmap;
	void *pMask;

	DWORD Width;
	DWORD Height;

	DWORD Mode;

	bool HasMask;

	int GetNaturalBitmap( BITMAPINFOHEADER *bmi, void **pImage, DWORD MaskColour = 0xFF000000 );

	static DWORD *GetPalette( DWORD ModeID, DWORD *Entries );

	static BYTE BPPs[ 128 ];

	static AspectRatio Res[ 128 ];

	AspectRatio SpriteAspect;

private:
	DWORD SpritePalette[ 256 ];
	WORD  SuppliedPaletteEntries;
};

