#pragma once

#include "Defs.h"
#include <vector>
#include <map>

class TapeBrowser
{
public:
	int Create( HWND Parent, int x, int y, std::vector<TapeCue> *cues );

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	static std::map<HWND, TapeBrowser *> browsers;
	static bool WndClassReg;
	static LRESULT CALLBACK TapeBrowserProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	TapeBrowser(void);
	~TapeBrowser(void);

public:
	HWND hWnd;

private:
	HWND  hParent;
	HWND  hCueList;
	HFONT IndexFont;

	std::vector<TapeCue> *pCues;
};

