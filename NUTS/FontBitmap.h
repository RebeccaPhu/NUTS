#pragma once

#ifndef FONTBITMAP_PLUGIN
#include "Plugins.h"
#endif

class FontBitmap
{
public:
	FontBitmap( BYTE *pFontData, const BYTE *pText, const WORD MaxLen, const bool Proportionate, const bool Selected );

#ifndef FONTBITMAP_PLUGIN
	FontBitmap( DWORD FontID, const BYTE *pText, const WORD MaxLen, const bool Proportionate, const bool Selected );
#endif

	void Init( BYTE *pFontData, const BYTE *pText, const WORD MaxLen, const bool Proportionate, const bool Selected );

	~FontBitmap(void);

	void DrawText( HDC hDC, DWORD x, DWORD y, DWORD Displace );

	void SetButtonColor( BYTE r, BYTE g, BYTE b );
	void SetTextColor( BYTE r, BYTE g, BYTE b );

private:
	WORD w;
	WORD TextLen;

	BITMAPINFO *bmi;

	void *pData;

public:
	void SetGrayed( bool grayed );
};

