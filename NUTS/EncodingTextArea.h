#pragma once

#include "Defs.h"

#include <vector>

typedef struct _LineDef
{
	DWORD StartPos;
	DWORD NumChars;
} LineDef;

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
	int     SetTextBody( DWORD FontID, BYTE *pBody, DWORD lBody, std::vector<DWORD> &rLinePointers );
	int     DoResize( int w, int h );
	int     SetFont( DWORD FontID );
	int     SaveText( FILE *fFile );
	int     PrintText( HDC hDC, int pw, int ph );
	int     CopySelection( void );

public:
	HWND  hWnd;

private:
	HWND  Parent;
	HWND  hScrollBar;
	HPEN  hCursor;
	HDC   hArea;
	HBITMAP hAreaCanvas;
	HGDIOBJ hAreaOld;
	BYTE  *pTextBody;
	DWORD lTextBody;
	DWORD Font;

	DWORD StartLine;
	DWORD MaxWinLines;
	DWORD MaxWinChars;

	std::vector<LineDef> LineDefs;
	std::vector<DWORD>   LinePointers;

	CRITICAL_SECTION cs;

	DWORD TextPtrStart;
	DWORD TextPtrEnd;

	DWORD SelStart;
	DWORD SelEnd;

	LPARAM MouseParam;

	bool  Selecting;

	bool  Blink;
	bool  Shift;
	bool  Ctrl;
	bool  Focus;

private:
	void  PaintTextArea( void );
	void  DoScroll(WPARAM wParam, LPARAM lParam);
	void  Update( void );
	DWORD GetMouseTextPtr( LPARAM lParam );
	void  SafeByteCopy( BYTE *dest, BYTE *src, DWORD len );
	void  DoKey( const int virtualKey, const int scanCode, bool Down );
	void  SetPtrs();
	void  ScrollToLine( int CLine );
	DWORD GetCursorLine( void );
	void  SetCursorLine( DWORD cline, DWORD line );

	void  ReGenerateLineDefs( void );
	void  SetScrollbar( DWORD Max, DWORD Page );
};

