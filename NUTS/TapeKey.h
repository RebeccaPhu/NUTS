#pragma once

#include "Defs.h"

#include <map>

typedef enum _TapeKeyID
{
	TapeKeyStart    = 0,
	TapeKeyBack     = 1,
	TapeKeyPlay     = 2,
	TapeKeyForward  = 3,
	TapeKeyStop     = 4,
	TapeKeyEject    = 5
} TapeKeyID;

class TapeKey
{
public:
	TapeKey( HWND Parent, TapeKeyID KeyID, int x, int y, bool Reset );
	~TapeKey(void);

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	static std::map<HWND, TapeKey *> items;
	static bool WndClassReg;
	static LRESULT CALLBACK TapeKeyProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	void Reset(void);
	void Press(void);
	bool IsPressed(void);

public:
	HWND hWnd;

private:
	HWND hParent;

	TapeKeyID keyID;

	bool Focus;
	bool DoesReset;
	bool Pressed;

	HDC  hArea;
	HBITMAP hAreaCanvas;
	HGDIOBJ hOldArea;

	HDC  hKey;
	HGDIOBJ hOldKey;

private:
	void PaintKey(void);
	void Refresh(void);
};

