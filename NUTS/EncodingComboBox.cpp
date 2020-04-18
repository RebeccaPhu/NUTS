#include "StdAfx.h"
#include "EncodingComboBox.h"

#include <WindowsX.h>

bool EncodingComboBox::_HasWindowClass = false;

std::map<HWND, EncodingComboBox *> EncodingComboBox::boxes;

const wchar_t EncodingComboBoxClass[]   = L"Encoding Combo Box";
const wchar_t EncodingComboBoxClassDD[] = L"Encoding Combo Box Drop Down";

LRESULT CALLBACK EncodingComboBoxWindowProc(HWND wnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::map<HWND, EncodingComboBox *>::iterator iWindow = EncodingComboBox::boxes.find( wnd );

	if ( iWindow != EncodingComboBox::boxes.end() )
	{
		return iWindow->second->WindowProc( wnd, uMsg, wParam, lParam );
	}

	return DefWindowProc( wnd, uMsg, wParam, lParam );
}

EncodingComboBox::EncodingComboBox( HWND hParent, int x, int y, int w )
{
	if ( !_HasWindowClass )
	{
		WNDCLASS wc = { };

		ZeroMemory( &wc, sizeof( wc ) );

		wc.lpfnWndProc   = EncodingComboBoxWindowProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = EncodingComboBoxClass;
		wc.hCursor       = LoadCursor( hInst, IDC_ARROW );

		RegisterClass(&wc);

		WNDCLASS ddwc = { };

		ZeroMemory( &ddwc, sizeof( ddwc ) );

		ddwc.lpfnWndProc   = EncodingComboBoxWindowProc;
		ddwc.hInstance     = hInst;
		ddwc.lpszClassName = EncodingComboBoxClassDD;
		ddwc.hCursor       = LoadCursor( hInst, IDC_ARROW );

		RegisterClass(&ddwc);

		_HasWindowClass = true;
	}

	ParentWnd = hParent;

	RECT wr;

	GetWindowRect( hParent, &wr );

	hWnd = CreateWindowEx(
		0,
		EncodingComboBoxClass,
		L"Encoding Combo Box",
		WS_POPUP,

		wr.left + x, wr.top + y + 23, w, 24,

		hParent, NULL, hInst, NULL
	);

	wx = x;
	wy = y;

	boxes[ hWnd ] = this;

	hDropDown = CreateWindowEx(
		0,
		EncodingComboBoxClassDD,
		L"Encoding Combo Box Drop Down Button",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP,

		x + (w - 18 ), y, 18, 23,

		hParent, NULL, hInst, NULL
	);

	boxes[ hDropDown ] = this;

	pTextArea  = new EncodingEdit( hParent, x, y, w - 18, false );
	DDPressed  = false;
	iIndex     = 0;
	Tracking   = false;
	HoverID    = 0x7FFFFF;
	SelectOnly = true;

	pTextArea->Disabled    = true;
	pTextArea->SoftDisable = true;
}


EncodingComboBox::~EncodingComboBox(void)
{
}

LRESULT EncodingComboBox::WindowProc( HWND wnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	bool Invalidate = false;

	switch ( uMsg )
	{
	case WM_PAINT:
		DoPaint( wnd );
		break;

	case WM_LBUTTONDOWN:
		{
			if ( wnd == hDropDown )
			{
				DDPressed  = true;
				Invalidate = true;
			}

			if ( wnd == hWnd )
			{
				if ( (HoverID >= 0 ) && ( HoverID < ListEntries.size() ) )
				{
					if ( SelectOnly )
					{
						/* Combo boxes should let the caller set this */
						pTextArea->SetText( ListEntries[ HoverID ] );
					}

					::PostMessage( ParentWnd, WM_COMMAND, (WPARAM) ( CBN_SELCHANGE << 16U ) | ( HoverID & 0xFFFF ), (LPARAM) hWnd );
				}

				ShowWindow( hWnd, SW_HIDE );

				ReleaseCapture();
			}
		}
		break;

	case WM_LBUTTONUP:
		{
			if ( wnd == hDropDown )
			{
				DoDropdown();

				DDPressed  = false;
				Invalidate = true;
			}
		}
		break;

	case WM_CANCELMODE:
		{
			if ( wnd == hWnd )
			{
				ShowWindow( hWnd, SW_HIDE );

				ReleaseCapture();
			}
		}
		break;

	case WM_MOUSEMOVE:
		{
			HoverID = ( GET_Y_LPARAM( lParam ) - 8 ) / 20;

			Invalidate = true;
		}
		break;

	case WM_ERASEBKGND:
		{
			if ( wnd == hWnd )
			{
				RECT r;

				GetClientRect( wnd, &r );

				HDC hDC = GetDC( wnd );

				FillRect( hDC, &r, (HBRUSH) GetStockObject( WHITE_BRUSH ) );
				DrawEdge( hDC, &r, EDGE_BUMP, BF_RECT );

				ReleaseDC( wnd, hDC );
			}
		}
		break;

	}

	if ( Invalidate )
	{
		RECT r;
		GetClientRect( wnd, &r );
		InvalidateRect( wnd, &r, FALSE );
	}

	return DefWindowProc( wnd, uMsg, wParam, lParam );
}

int EncodingComboBox::DoPaint( HWND wnd )
{
	HDC hDC = GetDC( wnd );

	RECT r;

	GetClientRect( wnd, &r );

	if ( wnd == hDropDown )
	{
		LOGBRUSH brsh;

		brsh.lbStyle = BS_SOLID;
		brsh.lbColor = GetSysColor( COLOR_BTNFACE );

		HBRUSH DDBrush = CreateBrushIndirect( &brsh );

		FillRect( hDC, &r, DDBrush );

		if (DDPressed)
		{
			DrawEdge( hDC, &r, EDGE_SUNKEN, BF_RECT );
		}
		else
		{
			DrawEdge( hDC, &r, EDGE_RAISED, BF_RECT );
		}

		DeleteObject( (HGDIOBJ) DDBrush );
	}

	if ( wnd == hWnd )
	{
		std::map<WORD, BYTE*>::iterator iItem;

		DWORD y = 0;
	
		for ( iItem = ListEntries.begin(); iItem != ListEntries.end(); iItem++ )
		{
			FontBitmap Item( FONTID_PC437, iItem->second, 64, false, ( y == HoverID ) );

			Item.DrawText( hDC, 4, 8 + y * 20, DT_LEFT | DT_TOP );

			y++;
		}
	}

	ValidateRect( wnd, &r );

	ReleaseDC( wnd, hDC );

	return 0;
}

int EncodingComboBox::DoDropdown( void )
{
	HoverID = 0x7FFFFFFF;

	std::map<WORD, BYTE*>::iterator iItem;
	
	DWORD ww = 16;

	for ( iItem = ListEntries.begin(); iItem != ListEntries.end(); iItem++ )
	{
		DWORD iw = rstrnlen( iItem->second, 64 );

		if ( iw > ww ) { ww = iw; }
	}

	RECT wr;

	GetWindowRect( ParentWnd, &wr );

	::SetWindowPos( hWnd, NULL, wr.left + wx, wr.top + wy + 23, 8 + ww * 8, 8 + max( 4, ListEntries.size() ) * 20, SWP_NOREPOSITION | SWP_NOZORDER );

	ShowWindow( hWnd, SW_SHOW );

	SetCapture( hWnd );

	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, TRUE );

	return 0;
}

void EncodingComboBox::TrackMouse( void )
{
	if ( !Tracking )
	{
		TRACKMOUSEEVENT tme;

		tme.cbSize      = sizeof( TRACKMOUSEEVENT );
		tme.dwFlags     = TME_NONCLIENT;
		tme.dwHoverTime = HOVER_DEFAULT;
		tme.hwndTrack   = hWnd;

		TrackMouseEvent( &tme );

		Tracking = true;
	}
}
