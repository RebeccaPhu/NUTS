#pragma once

#include "Plugins.h"

class FontBitmap
{
public:
	FontBitmap( DWORD FontID, const BYTE *pText, const BYTE MaxLen, const bool Ellipsis, const bool Selected );
	~FontBitmap(void);

	void DrawText( HDC hDC, DWORD x, DWORD y, DWORD Displace );

	void SetButtonColor( BYTE r, BYTE g, BYTE b );
	void SetTextColor( BYTE r, BYTE g, BYTE b );

private:
	WORD w;
	WORD h;

	BITMAPINFO *bmi;

	void *pData;

public:
	void SetGrayed( bool grayed );
};

