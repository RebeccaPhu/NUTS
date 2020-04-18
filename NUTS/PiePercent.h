#pragma once

#include <WinGDI.h>

class PiePercent
{
public:
	PiePercent(void);
	~PiePercent(void);

	void CreatePieChart( int Percent, HWND hPropsWnd );

public:
	HDC hPieDC;

private:
	HBITMAP hPieBitmap;

	HPEN   hPDarkBlue;
	HPEN   hPDarkMag;
	HPEN   hPBlue;
	HPEN   hPMag;
	HBRUSH hBG;

	HGDIOBJ hOld;

	BYTE   *pPieBits;
};

