#include "StdAfx.h"
#include "TapeBrowser.h"

#include "resource.h"

#include <CommCtrl.h>

#define TBW 180
#define TBH 250

std::map<HWND, TapeBrowser *> TapeBrowser::browsers;

bool TapeBrowser::WndClassReg = false;

LRESULT CALLBACK TapeBrowser::TapeBrowserProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( browsers.find( hWnd ) != browsers.end() )
	{
		return browsers[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

TapeBrowser::TapeBrowser( ) {
	if ( !WndClassReg )
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= TapeBrowser::TapeBrowserProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInst;
		wcex.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(IDI_SPRITE));
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
		wcex.lpszMenuName	= NULL;
		wcex.lpszClassName	= L"NUTS Tape Browser";
		wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SPRITE));

		RegisterClassEx(&wcex);

		WndClassReg = true;
	}
}

TapeBrowser::~TapeBrowser(void)
{
	NixWindow( hCueList );
	NixWindow( hWnd );
	NixObject( IndexFont );

	browsers.erase( hWnd );
}

int TapeBrowser::Create( HWND Parent, int x, int y, std::vector<TapeCue> *cues )
{
	hWnd = CreateWindowEx(
		NULL,
		L"NUTS Tape Browser",
		L"Tape Browser",
		WS_SYSMENU | WS_CLIPCHILDREN | WS_OVERLAPPED | WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED,
		x, y, TBW, TBH,
		Parent, NULL, hInst, NULL
	);

	browsers[ hWnd ] = this;

	hParent = Parent;
	pCues   = cues;

	RECT r;

	GetClientRect( hWnd, &r );

	hCueList = CreateWindowEx(
		NULL,
		WC_LISTBOX,
		L"Cue List",
		WS_VISIBLE | WS_TABSTOP | WS_GROUP | WS_CHILD | LBS_NOTIFY | WS_VSCROLL,
		0, 0, r.right - r.left, r.bottom - r.top,
		hWnd, NULL, hInst, NULL
	);

	for ( std::vector<TapeCue>::iterator iCue = cues->begin(); iCue != cues->end(); iCue++ )
	{
		::SendMessage( hCueList, LB_ADDSTRING, 0, (LPARAM) iCue->IndexName.c_str() );
	}

	IndexFont = CreateFont(12,6, 0,0,FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"Arial");

	::SendMessage( hCueList, WM_SETFONT, (WPARAM) IndexFont, (LPARAM) TRUE );

	SetFocus( hCueList );

	return 0;
}

LRESULT	TapeBrowser::WndProc(HWND hWindow, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_ACTIVATE:
		if ( wParam == 0 )
		{
			hActiveWnd = NULL;
		}
		else
		{
			hActiveWnd = hWindow;
		}
		return FALSE;

	case WM_CLOSE:
		{
			::PostMessage( hParent, WM_TBCLOSED, 0, 0 );

			delete this;
		}
		break;

	case WM_COMMAND:
		if ( HIWORD( wParam ) == LBN_DBLCLK )
		{
			int Index = ::SendMessage( hCueList, LB_GETCURSEL, 0, 0 );

			::PostMessage( hParent, WM_CUEINDEX_JUMP, 0, (LPARAM) Index );
		}
		break;
	}

	return DefWindowProc( hWindow, message, wParam, lParam );
}