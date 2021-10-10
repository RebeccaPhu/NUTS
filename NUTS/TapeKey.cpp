#include "StdAfx.h"
#include "TapeKey.h"

#include "resource.h"

std::map<HWND, TapeKey *> TapeKey::items;

bool TapeKey::WndClassReg = false;

const wchar_t TapeKeyClass[]  = L"NUTS Cassette Player Key";

LRESULT CALLBACK TapeKey::TapeKeyProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( items.find( hWnd ) != items.end() )
	{
		return items[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

TapeKey::TapeKey( HWND Parent, TapeKeyID KeyID, int x, int y, bool Reset )
{
	if ( !WndClassReg )
	{
		WNDCLASS wc;

		ZeroMemory( &wc, sizeof( wc ) );

		wc.lpfnWndProc   = TapeKey::TapeKeyProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = TapeKeyClass;
		wc.hCursor       = LoadCursor( NULL, IDC_ARROW );

		RegisterClass(&wc);

		WndClassReg = true;
	}

	hArea       = NULL;
	hOldArea    = NULL;
	hAreaCanvas = NULL;
	hKey        = NULL;
	hOldKey     = NULL;

	hWnd = CreateWindowEx(
		NULL,
		TapeKeyClass,
		L"Tape Key",
		WS_CHILD|WS_VISIBLE|WS_TABSTOP,
		x, y, 48, 48,
		Parent, NULL, hInst, NULL
	);

	items[ hWnd ] = this;

	Focus     = false;
	DoesReset = Reset;
	Pressed   = false;
	hParent   = Parent;
	keyID     = KeyID;
}

TapeKey::~TapeKey(void)
{
	items.erase( hWnd );

	SelectObject( hArea, hOldArea );

	NixWindow( hWnd );

	NixObject( hArea );
	NixObject( hAreaCanvas );

	NixObject( hOldKey );
	NixObject( hKey );
}

LRESULT	TapeKey::WndProc(HWND hWindow, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		{
			PaintKey();
		}
		break;

	case WM_CLOSE:
		{
			items.erase( hWnd );
		}
		break;

	case WM_LBUTTONDOWN:
		{
			Pressed = true;

			::PostMessage( hParent, WM_TAPEKEY_DOWN, (WPARAM) keyID, (LPARAM) hWnd );

			Refresh();
		}
		break;

	case WM_LBUTTONUP:
		{
			if ( DoesReset )
			{
				Pressed = false;			
			}

			::PostMessage( hParent, WM_TAPEKEY_UP, (WPARAM) keyID, (LPARAM) hWnd );

			Refresh();
		}
		break;

	case WM_SETFOCUS:
		{
			Focus = true;

			Refresh();
		}
		break;

	case WM_KILLFOCUS:
		{
			Focus = false;

			Refresh();
		}
		break;

	case WM_MOUSEACTIVATE:
		::SetFocus( hWnd );

		return MA_ACTIVATE;

	case WM_GETDLGCODE:
		{
			if ( ( wParam == VK_SPACE ) || ( wParam == VK_RETURN ) )
			{
				return DLGC_BUTTON;
			}
		}

		return 0;
		
	case WM_KEYDOWN:
		{
			if ( wParam == VK_SPACE )
			{
				Pressed = true;

				::PostMessage( hParent, WM_TAPEKEY_DOWN, (WPARAM) keyID, (LPARAM) hWnd );

				Refresh();
			}
		}
		break;

	case WM_KEYUP:
		{
			if ( wParam == VK_SPACE )
			{
				if ( DoesReset )
				{
					Pressed = false;			
				}

				::PostMessage( hParent, WM_TAPEKEY_UP, (WPARAM) keyID, (LPARAM) hWnd );

				Refresh();
			}
		}
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}


void TapeKey::PaintKey( void )
{
	RECT r;

	GetClientRect( hWnd, &r );

	HDC hDC = GetDC( hWnd );

	if ( hArea == NULL )
	{
		hArea = CreateCompatibleDC( hDC );

		hAreaCanvas = CreateCompatibleBitmap( hDC, r.right - r.left, r.bottom - r.top );

		hOldArea = SelectObject( hArea, hAreaCanvas );
	}

	if ( hKey == NULL )
	{
		hKey = CreateCompatibleDC( hDC );

		int RKEY = IDB_PLAYER_FIRST;

		switch ( keyID )
		{
		case TapeKeyStart:
			RKEY = IDB_PLAYER_FIRST;
			break;

		case TapeKeyBack:
			RKEY = IDB_PLAYER_REWIND;
			break;

		case TapeKeyStop:
			RKEY = IDB_PLAYER_STOP;
			break;

		case TapeKeyPlay:
			RKEY = IDB_PLAYER_PLAY;
			break;

		case TapeKeyForward:
			RKEY = IDB_PLAYER_FORWARD;
			break;

		case TapeKeyEject:
			RKEY = IDB_PLAYER_EJECT;
			break;

		default:
			RKEY = IDB_PLAYER_FIRST;
			break;
		}
	
		hOldKey = SelectObject( hKey, (HGDIOBJ) LoadBitmap( hInst, MAKEINTRESOURCE( RKEY ) ) );
	}

	BitBlt( hArea, 0, 0, 48, 48, hKey, 0, 0, SRCCOPY );

	if ( Pressed )
	{
		POINT lpts[ 3 ] = { { 0, 0 }, { 0, 47 }, { 2, 47 } };
		POINT rpts[ 3 ] = { { 47, 0 }, { 47, 47 }, { 45, 47 } };

		SelectObject( hArea, GetStockObject( BLACK_PEN ) );
		SelectObject( hArea, GetStockObject( BLACK_BRUSH ) );

		Polygon( hArea, lpts, 3 );
		Polygon( hArea, rpts, 3 );
	}

	if ( Focus )
	{
		SelectObject( hArea, GetStockObject( WHITE_PEN ) );

		MoveToEx( hArea, 0, 0, NULL );
		LineTo( hArea, 48, 0 );

		MoveToEx( hArea, 0, 1, NULL );
		LineTo( hArea, 48, 1 );
	}

	BitBlt( hDC, 0, 0, 48, 48, hArea, 0, 0, SRCCOPY );

	ReleaseDC( hWnd, hDC );
}

void TapeKey::Refresh()
{
	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );
}

void TapeKey::Reset(void)
{
	Pressed = false;

	Refresh();
}

bool TapeKey::IsPressed(void)
{
	return Pressed;
}
