#pragma once
class BlockMap
{
public:
	BlockMap(void);
	~BlockMap(void);

	HDC hBlockDC;

	DWORD MapSizeH;
	DWORD MapSizeW;

public:
	void CreateBlockMap( HWND hWnd, BYTE *pMap );

private:
	HBITMAP hBlockBitmap;
	HGDIOBJ hO;

	HBRUSH  hUsed;
	HBRUSH  hFree;
	HBRUSH  hFixed;
	HBRUSH  hNone;
};

