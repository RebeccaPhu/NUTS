#pragma once

#include "../NUTS/TempFile.h"
#include "../NUTS/Defs.h"

class InfoIcon
{
public:
	InfoIcon( CTempFile &FileObj );
	InfoIcon( BYTE *pIconData, DWORD DataLength );
	~InfoIcon(void);

public:
	int GetNaturalBitmap( BITMAPINFOHEADER *bmi, BYTE **pPixels );

private:
	BYTE *pIcon;

public:
	bool HasIcon;

	WORD IconHeight;
	WORD IconWidth;

	BYTE IconVersion;

	BYTE *pIconTool;

	std::vector<BYTE *> ToolTypes;

private:
	void LoadIcon( BYTE *pIconData, DWORD DataLength );

};

