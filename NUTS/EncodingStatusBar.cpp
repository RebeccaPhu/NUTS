#include "StdAfx.h"
#include "EncodingStatusBar.h"
#include "FontBitmap.h"
#include "libfuncs.h"

#include "NUTSMacros.h"

bool EncodingStatusBar::_HasWindowClass = false;

std::map<HWND, EncodingStatusBar *> EncodingStatusBar::StatusBars;

const wchar_t EncodingStatusBarClass[]  = L"Encoding Status Bar";

LRESULT CALLBACK EncodingStatusBarWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::map<HWND, EncodingStatusBar *>::iterator iWindow = EncodingStatusBar::StatusBars.find( hWnd );

	if ( iWindow != EncodingStatusBar::StatusBars.end() )
	{
		return iWindow->second->WindowProc( uMsg, wParam, lParam );
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

EncodingStatusBar::EncodingStatusBar( HWND hParent )
{
	if ( !_HasWindowClass )
	{
		WNDCLASS wc = { };

		ZeroMemory( &wc, sizeof( wc ) );

		wc.lpfnWndProc   = EncodingStatusBarWindowProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = EncodingStatusBarClass;

		ATOM atom = RegisterClass(&wc);

		_HasWindowClass = true;
	}

	ParentWnd = hParent;

	RECT r;

	GetClientRect( hParent, &r );

	hWnd = CreateWindowEx(
		0,
		EncodingStatusBarClass,
		L"Encoding Status Bar",
		WS_CHILD | WS_VISIBLE,

		0, r.bottom - 26, r.right - r.left, 24,

		hParent, NULL, hInst, NULL
	);

	StatusBars[ hWnd ] = this;
}


EncodingStatusBar::~EncodingStatusBar(void)
{
	Panels.clear();

	StatusBars.erase( hWnd );

	NixWindow( hWnd );
}

LRESULT EncodingStatusBar::WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if ( uMsg == WM_PAINT )
	{
		return DoPaint();
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

int EncodingStatusBar::DoPaint( void )
{
	HDC hDC = GetDC( hWnd );
	PanelIterator iPanel;

	int	brdx = GetSystemMetrics(SM_CXBORDER);

	DWORD px = brdx;
	RECT  wr;

	GetClientRect( hWnd, &wr );

	LOGBRUSH brsh;

	brsh.lbStyle = BS_SOLID;
	brsh.lbColor = GetSysColor( COLOR_BTNFACE );

	HBRUSH PanelBack = CreateBrushIndirect( &brsh );

	for ( iPanel = Panels.begin(); iPanel != Panels.end(); iPanel++ )
	{
		RECT r;

		r.left   = px;
		r.right  = px + iPanel->second.Width;
		r.top    = 0;
		r.bottom = 24;

		if ( iPanel->second.Flags & PF_SPARE )
		{
			r.right = wr.right - brdx;
		}

		FillRect( hDC, &r, PanelBack );

		DrawEdge( hDC, &r, EDGE_SUNKEN, BF_RECT );

		FontBitmap text( iPanel->second.FontID, iPanel->second.Text, rstrnlen( iPanel->second.Text, 255 ), false, false );

		text.SetButtonColor( GetRValue( brsh.lbColor ), GetGValue( brsh.lbColor ), GetBValue( brsh.lbColor ) );

		WORD tl = rstrnlen( iPanel->second.Text, 255 );
		WORD pl = r.right - r.left;
		WORD tx = px;

		if ( iPanel->second.Flags & PF_RIGHT )
		{
			tx = px + ( pl - ( tl * 8 ) );

			if ( iPanel->second.Flags & PF_SPARE )
			{
				tx -= 8;
			}

			if ( iPanel->second.Flags & PF_SIZER )
			{
				tx -= 8;
			}
		}

		if ( iPanel->second.Flags & PF_CENTER )
		{
			tx = px + ( ( pl / 2 ) - ( ( tl * 8 ) / 2) );

			tx -= 3;
		}

		text.DrawText( hDC, tx + 6, 6, DT_TOP );

		if ( iPanel->second.Flags & PF_SIZER )
		{
			HGDIOBJ hOld = SelectObject( hDC, GetStockObject( BLACK_PEN ) );

			MoveToEx( hDC, px + pl - 8, r.bottom - 4, NULL );
			LineTo( hDC, px +pl - 4, r.bottom - 8 );

			SelectObject( hDC, hOld );
		}

		px += iPanel->second.Width;
	}

	ReleaseDC( hWnd, hDC );

	DeleteObject( (HGDIOBJ) PanelBack );

	ValidateRect( hWnd, &wr );

	return FALSE;
}

int EncodingStatusBar::AddPanel( DWORD PanelID, DWORD Width, BYTE *Text, FontIdentifier FontID, DWORD Flags )
{
	Panel panel;

	panel.ID     = PanelID;
	panel.Flags  = Flags;
	panel.Width  = Width;
	panel.FontID = FontID;

	rstrncpy( panel.Text, Text, 255 );

	Panels[ PanelID ] = panel;

	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );

	return 0;
}

int EncodingStatusBar::SetPanelWidth( DWORD PanelID, DWORD NewWidth )
{
	if ( Panels.find( PanelID ) != Panels.end() )
	{
		Panels[ PanelID ].Width = NewWidth;
	}

	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );

	return 0;
}

int EncodingStatusBar::SetPanelText( DWORD PanelID, FontIdentifier FontID, BYTE *Text )
{
	if ( Panels.find( PanelID ) != Panels.end() )
	{
		Panels[ PanelID ].FontID = FontID;

		rstrncpy( Panels[ PanelID ].Text, Text, 255 );
	}

	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );

	return 0;
}

int EncodingStatusBar::SetPanelFont( DWORD PanelID, FontIdentifier FontID )
{
	if ( Panels.find( PanelID ) != Panels.end() )
	{
		Panels[ PanelID ].FontID = FontID;
	}

	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );

	return 0;
}

int EncodingStatusBar::NotifyWindowSizeChanged( void )
{
	RECT r;

	GetClientRect( ParentWnd, &r );

	SetWindowPos( hWnd, NULL, 0, r.bottom - 26, r.right - r.left, 24, SWP_NOREPOSITION | SWP_NOZORDER );

	return 0;
}