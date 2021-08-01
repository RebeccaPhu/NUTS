#include "StdAfx.h"
#include "CharMap.h"
#include "FontBitmap.h"
#include "Defs.h"

#include <WindowsX.h>

#define MAP_CHARWIDTH  8
#define MAP_CHARHEIGHT 16
#define MAP_CHARPADDING 2
#define MAP_CHARBORDER 1
#define MAP_HCELLS 32
#define MAP_VCELLS 8
#define MAP_CELL_WIDTH ( MAP_CHARWIDTH + ( MAP_CHARPADDING * 2 ) )
#define MAP_CELL_HEIGHT ( MAP_CHARHEIGHT + ( MAP_CHARPADDING * 2 ) )
#define MAP_WINDOW_WIDTH ( ( ( MAP_CELL_WIDTH + MAP_CHARBORDER ) * MAP_HCELLS ) + MAP_CHARBORDER )
#define MAP_WINDOW_HEIGHT ( ( ( MAP_CELL_HEIGHT + MAP_CHARBORDER ) * MAP_VCELLS ) + MAP_CHARBORDER )


bool CharMap::_HasWindowClass = false;

std::map<HWND, CharMap *> CharMap::_CharMapClassMap;

const wchar_t CharMapClass[]  = L"Font Character Map";

CharMap *CharMap::TheCharMap = nullptr;
HWND    CharMap::hMainWnd    = NULL;
HWND    CharMap::hParentWnd  = NULL;

LRESULT CALLBACK CharMapWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::map<HWND, CharMap *>::iterator iWindow = CharMap::_CharMapClassMap.find( hWnd );

	if ( iWindow != CharMap::_CharMapClassMap.end() )
	{
		return iWindow->second->WindowProc( uMsg, wParam, lParam );
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

void CharMap::OpenTheMap( HWND hParent, DWORD FontID )
{
	if ( CharMap::TheCharMap == nullptr )
	{
		CharMap::TheCharMap = new CharMap( hParent, FontID );
	}
	else
	{
		CharMap::TheCharMap->SetFont( FontID );

		CharMap::SetFocusWindow( hParent );
	}
}

CharMap::CharMap( HWND hParent, DWORD FontID )
{
	if ( !_HasWindowClass )
	{
		WNDCLASS wc = { };

		ZeroMemory( &wc, sizeof( wc ) );

		wc.style         = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc   = CharMapWindowProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = CharMapClass;
		wc.hCursor       = LoadCursor( NULL, IDC_ARROW );

		ATOM atom = RegisterClass(&wc);

		_HasWindowClass = true;
	}

	hWnd = CreateWindowEx(
		0,
		CharMapClass,
		L"NUTS Character Map",
		WS_CLIPSIBLINGS | WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, MAP_WINDOW_WIDTH, MAP_WINDOW_HEIGHT,

		hMainWnd, NULL, hInst, NULL
	);

	/* This merry little dance is apparently needed because even when you take system metrics into account,
	   the resulting window STILL isn't bigger enough. Despite everything matching up. No I don't know why
	   either. */
	RECT r;

	r.top    = 0;
	r.left   = 0;
	r.right  = MAP_WINDOW_WIDTH;
	r.bottom = MAP_WINDOW_HEIGHT;

	AdjustWindowRect( &r, WS_CLIPSIBLINGS | WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_SYSMENU, FALSE );

	SetWindowPos( hWnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOREPOSITION | SWP_NOMOVE | SWP_NOZORDER );

	CharMap::_CharMapClassMap[ hWnd ] = this;

	hArea         = NULL;
	hAreaOld      = NULL;
	hAreaCanvas   = NULL;
	hParentWnd    = hParent;

	CFontID = FontID;

	ci = -1;
}


CharMap::~CharMap(void)
{
	if ( hAreaOld )
	{
		SelectObject( hArea, hAreaOld );
	}

	NixObject( hAreaCanvas );

	if ( hArea )
	{
		DeleteDC( hArea );
	}

	NixWindow( hWnd );
}

LRESULT CharMap::WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
		case WM_PAINT:
			PaintWindow();
			break;
		
		case WM_CLOSE:
			{
				CharMap::TheCharMap = nullptr;

				delete this;
			}
			break;

		case WM_MOUSEMOVE:
			{
				long xc = GET_X_LPARAM( lParam );
				long yc = GET_Y_LPARAM( lParam );

				yc /= ( MAP_CELL_HEIGHT + MAP_CHARBORDER );
				xc /= ( MAP_CELL_WIDTH + MAP_CHARBORDER );

				ci = yc * MAP_HCELLS + xc;

				RECT rc;

				GetClientRect( hWnd, &rc );

				InvalidateRect( hWnd, &rc, FALSE );
			}
			break;

		case WM_LBUTTONUP:
			{
				::SendMessage( hParentWnd, WM_CHARMAPCHAR, (WPARAM) ci, 0 );
			}
			break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

void CharMap::PaintWindow( void )
{
	RECT rc;
	HDC hDC = GetDC( hWnd );

	GetClientRect( hWnd, &rc );

	if ( hArea == NULL )
	{
		hArea       = CreateCompatibleDC( hDC );
		hAreaCanvas = CreateCompatibleBitmap( hDC, rc.right - rc.left, rc.bottom - rc.top );
		hAreaOld    = SelectObject( hArea, hAreaCanvas );

		FillRect( hArea, &rc, (HBRUSH) GetStockObject( WHITE_BRUSH ) );
	}

	// TODO: Regenerate this if the font changes, rather than on every paint.
	BYTE fc[ 2 ] = { 0, 0};

	SelectObject( hArea, GetStockObject( BLACK_PEN ) );

	for ( int y=0; y<MAP_VCELLS; y++ )
	{
		for ( int x=0; x<MAP_HCELLS; x++ )
		{
			FontBitmap *fb = new FontBitmap( CFontID, fc, 1, false, ( ci == (long) fc[0] ) );

			fb->DrawText( hArea,
				( x * (MAP_CELL_WIDTH  + MAP_CHARBORDER ) ) + MAP_CHARBORDER + MAP_CHARPADDING,
				( y * (MAP_CELL_HEIGHT + MAP_CHARBORDER ) ) + MAP_CHARBORDER + MAP_CHARPADDING,
				DT_TOP | DT_LEFT
			);

			fc[ 0 ]++;

			delete fb;
		}
	}

	for ( int y=0; y<MAP_VCELLS; y++ )
	{
		MoveToEx( hArea, 0, y * ( MAP_CELL_HEIGHT + MAP_CHARBORDER ), NULL );
		LineTo( hArea, MAP_WINDOW_WIDTH, y * ( MAP_CELL_HEIGHT + MAP_CHARBORDER ) );
	}

	MoveToEx( hArea, 0, MAP_WINDOW_HEIGHT - 1, NULL );
	LineTo( hArea, MAP_WINDOW_WIDTH, MAP_WINDOW_HEIGHT -1 );

	for ( int x=0; x<MAP_HCELLS; x++ )
	{
		MoveToEx( hArea, x * ( MAP_CELL_WIDTH + MAP_CHARBORDER ), 0, NULL );
		LineTo( hArea, x * ( MAP_CELL_WIDTH + MAP_CHARBORDER ), MAP_WINDOW_HEIGHT );
	}

	MoveToEx( hArea, MAP_WINDOW_WIDTH - 1, 0, NULL );
	LineTo( hArea, MAP_WINDOW_WIDTH - 1, MAP_WINDOW_HEIGHT );

	BitBlt( hDC, 0, 0, MAP_WINDOW_WIDTH, MAP_WINDOW_HEIGHT, hArea, 0, 0, SRCCOPY );

	ReleaseDC( hWnd, hDC );
}

void CharMap::SetFocusWindow( HWND hParent )
{
	CharMap::hParentWnd = hParent;
}

void CharMap::SetFont( DWORD FontID )
{
	CFontID = FontID;

	RECT rc;

	GetClientRect( hWnd, &rc );

	InvalidateRect( hWnd, &rc, FALSE );
}
