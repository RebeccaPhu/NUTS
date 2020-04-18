#pragma once

#include "Defs.h"

#include <vector>

class EncodingTextArea
{
public:
	EncodingTextArea( HWND hParent, int x, int y, int w, int h );
	~EncodingTextArea(void);

public:
	static std::map<HWND, EncodingTextArea *> _EncodingTextAreaClassMap;
	static bool _HasWindowClass;

public:
	static LRESULT CALLBACK EncodingTextAreaWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	LRESULT WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );
	int     SetTextBody( DWORD EncodingID, BYTE *pBody, DWORD lBody, std::vector<DWORD> &rLinePointers );
	int     DoResize( int w, int h );

public:
	HWND  hWnd;

private:
	HWND  Parent;
	HWND  hScrollBar;
	BYTE  *pTextBody;
	DWORD lTextBody;
	DWORD Encoding;
	DWORD MaxLine;
	DWORD StartLine;

	std::vector<DWORD> LinePointers;

private:
	void  PaintTextArea( void );
	void  DoScroll(WPARAM wParam, LPARAM lParam);
	void  Update( void );
};

