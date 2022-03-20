#include "StdAfx.h"
#include "IconButton.h"
#include "libfuncs.h"

#include "NUTSMacros.h"

// If you've been following the git history, you'll notice I copy and paste
// this pattern a lot. I probably should just make a CWindow class or something.
//
// Don't judge me.

bool IconButton::_HasWindowClass = false;

std::map<HWND, IconButton *> IconButton::_IconButtons;

const wchar_t IconButtonClass[]  = L"IconButton";

LRESULT CALLBACK IconButton::IconButtonWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::map<HWND, IconButton *>::iterator iWindow = IconButton::_IconButtons.find( hWnd );

	if ( iWindow != IconButton::_IconButtons.end() )
	{
		return iWindow->second->WindowProc( uMsg, wParam, lParam );
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

IconButton::IconButton( HWND hParent, int x, int y, HICON icon )
{
	if ( !_HasWindowClass )
	{
		WNDCLASS wc = { };

		ZeroMemory( &wc, sizeof( wc ) );

		wc.lpfnWndProc   = IconButtonWindowProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = IconButtonClass;
		wc.hCursor       = LoadCursor( NULL, IDC_ARROW );

		ATOM atom = RegisterClass(&wc);

		_HasWindowClass = true;
	}

	hWnd = CreateWindowEx(
		0,
		IconButtonClass,
		L"Icon Button",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP,

		x, y, 24, 24,

		hParent, NULL, hInst, NULL
	);

	_IconButtons[ hWnd ] = this;

	hIcon   = icon;
	Parent  = hParent;
	hTip    = NULL;
	hDotted = NULL;

	MPressed  = false;
	KPressed  = false;
	IgnoreKey = false;
	Focus     = false;

	hArea    = NULL;
	hCanvas  = NULL;
	hAreaOld = NULL;
}

IconButton::~IconButton(void)
{
	NixWindow( hTip );
	NixWindow( hWnd );

	if ( hArea != NULL )
	{
		SelectObject( hArea, hAreaOld );

		NixObject( hCanvas );
		NixObject( hArea );
	}

	NixObject( hDotted );
}


LRESULT IconButton::WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
		case WM_PAINT:
			PaintButton();

			break;

		case WM_GETDLGCODE:
			if ( wParam == VK_SPACE )
			{
				return DLGC_BUTTON;
			}

			if ( wParam == VK_ESCAPE )
			{
				return DLGC_WANTALLKEYS;
			}
			
			return DLGC_STATIC;

		case WM_LBUTTONDOWN:
			{
				TRACKMOUSEEVENT tme;

				tme.cbSize    = sizeof( tme );
				tme.dwFlags   = TME_LEAVE;
				tme.hwndTrack = hWnd;

				TrackMouseEvent( &tme );
			}

			MPressed = true;

			Refresh();

			break;

		case WM_MOUSELEAVE:
			MPressed = false;

			Refresh();

			break;

		case WM_LBUTTONUP:
			if ( MPressed )
			{
				PostMessage( Parent, WM_COMMAND, 0, (LPARAM) hWnd );
			}

			MPressed = false;

			Refresh();

			break;

		case WM_SETFOCUS:
			Focus = true;

			IgnoreKey = false;

			Refresh();

			break;

		case WM_KILLFOCUS:
			Focus = false;

			IgnoreKey = false;

			Refresh();

			break;

		case WM_KEYDOWN:
			if ( wParam == VK_ESCAPE )
			{
				KPressed = false;
			}

			if ( ( wParam == VK_SPACE ) && ( !IgnoreKey ) )
			{
				KPressed = true;

				IgnoreKey = true;
			}
			
			Refresh();

			break;

		case WM_KEYUP:
			if ( wParam == VK_SPACE )
			{
				if ( KPressed )
				{
					PostMessage( Parent, WM_COMMAND, 0, (LPARAM) hWnd );
				}

				KPressed  = false;
				IgnoreKey = false;
			}

			Refresh();

			break;

		case WM_MOUSEACTIVATE:
			::SetFocus( hWnd );

			Refresh();

			return MA_ACTIVATE;

	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

void IconButton::SetTip( std::wstring Tip )
{
	hTip = CreateToolTip( hWnd, Parent, (WCHAR *) Tip.c_str(), hInst );
}

void IconButton::Refresh()
{
	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );
}

void IconButton::PaintButton()
{
	RECT r;

	GetClientRect( hWnd, &r );

	HDC hDC = GetDC( hWnd );

	if ( hArea == NULL )
	{
		hArea = CreateCompatibleDC( hDC );

		hCanvas = CreateCompatibleBitmap( hDC, r.right - r.left, r.bottom - r.top );

		hAreaOld = SelectObject( hArea, hCanvas );
	}

	FillRect( hArea, &r, GetSysColorBrush( COLOR_BTNFACE ) );

	if ( ( MPressed ) || ( KPressed ) )
	{
		DrawEdge( hArea, &r, EDGE_SUNKEN, BF_RECT );
	}
	else
	{
		DrawEdge( hArea, &r, EDGE_RAISED, BF_RECT );
	}

	DrawIconEx( hArea, 4, 4, hIcon, 16, 16, 0, NULL, DI_NORMAL );

	if ( Focus )
	{
		if ( hDotted == NULL )
		{
			hDotted = CreatePen( PS_DOT, 1, RGB( 0x88, 0x88, 0x88 ) );
		}

		SelectObject( hArea, hDotted );

		SetBkMode( hArea, TRANSPARENT );

		MoveToEx( hArea, 2, 2, NULL );
		LineTo( hArea, 21, 2 );
		LineTo( hArea, 21, 21 );
		LineTo( hArea, 2, 21 );
		LineTo( hArea, 2, 2 );
	}

	BitBlt( hDC, 0, 0, 24, 24, hArea, 0, 0, SRCCOPY );
	
	ReleaseDC( hWnd, hDC );
}