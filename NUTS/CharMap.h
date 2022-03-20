#pragma once

#include <vector>
#include <map>

#include "NUTSTypes.h"

class CharMap
{
public:
	CharMap( HWND hParent, FontIdentifier FontID );
	~CharMap(void);

public:
	static std::map<HWND, CharMap *> _CharMapClassMap;
	static bool _HasWindowClass;

	static CharMap *TheCharMap;
	static HWND    hMainWnd;
	static HWND    hParentWnd;

	static void OpenTheMap( HWND hParent, FontIdentifier FontID );
	static void SetFocusWindow( HWND hParent );
	static void RemoveFocus( HWND hParent );

public:
	LRESULT WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );

	void SetFont( FontIdentifier FontID );

public:
	HWND  hWnd;

private:
	HDC     hArea;
	HGDIOBJ hAreaOld;
	HBITMAP hAreaCanvas;
	HWND    hFontList;
	HDC     hCharDesc;
	HGDIOBJ hCharOld;
	HBITMAP hCharDescCanvas;

	FontIdentifier CFontID;

	long    ci;

	std::vector<FontIdentifier> FontMap;

private:
	void PaintWindow( void );
};

