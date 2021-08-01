#pragma once

#include <map>

class CharMap
{
public:
	CharMap( HWND hParent, DWORD FontID );
	~CharMap(void);

public:
	static std::map<HWND, CharMap *> _CharMapClassMap;
	static bool _HasWindowClass;

	static CharMap *TheCharMap;
	static HWND    hMainWnd;
	static HWND    hParentWnd;

	static void OpenTheMap( HWND hParent, DWORD FontID );
	static void SetFocusWindow( HWND hParent );
public:
	LRESULT WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );

	void SetFont( DWORD FontID );

public:
	HWND  hWnd;

private:
	HDC     hArea;
	HGDIOBJ hAreaOld;
	HBITMAP hAreaCanvas;

	DWORD   CFontID;

	long    ci;

private:
	void PaintWindow( void );
};

