#include "StdAfx.h"
#include "EncodingToolTip.h"
#include "FontBitmap.h"
#include "NUTSMacros.h"

#include <WindowsX.h>

std::map<HWND, EncodingToolTip *> EncodingToolTip::items;

bool EncodingToolTip::WndClassReg = false;

const wchar_t EncodingToolTipClass[]  = L"Encoding Tool Tip";

LRESULT CALLBACK EncodingToolTip::EncodingToolTipProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( items.find( hWnd ) != items.end() )
	{
		return items[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

EncodingToolTip::EncodingToolTip(void)
{
	if ( !WndClassReg )
	{
		WNDCLASS wc;

		ZeroMemory( &wc, sizeof( wc ) );

		wc.lpfnWndProc   = EncodingToolTip::EncodingToolTipProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = EncodingToolTipClass;
		wc.hCursor       = LoadCursor( NULL, IDC_ARROW );

		RegisterClass(&wc);

		WndClassReg = true;
	}

	hEdgePen      = NULL;
	hCanvas       = NULL;
	hCanvasBitmap = NULL;

	IgnoreEvents  = true;
}

EncodingToolTip::~EncodingToolTip(void)
{
	if ( hCanvas != NULL )
	{
		SelectObject( hCanvas, hOld );
	}

	NixObject( hEdgePen );
	NixObject( hCanvas );
	NixObject( hCanvasBitmap );

	NixWindow( hWnd );
}

int EncodingToolTip::Create( HWND Parent, HINSTANCE hInstance, int x, int y, TooltipDef &tip )
{
	DWORD l = 0;
	for ( TooltipDef_iter i = tip.begin(); i != tip.end(); i++ )
	{
		if ( i->Text.length() > l )
		{
			l = i->Text.length();
		}
	}

	int w = ( l * 8 ) + 4;
	int h = ( tip.size() * 17 ) + 3;

	_tip = tip;

	hWnd = CreateWindowEx(
		NULL,
		EncodingToolTipClass,
		L"Encoding Tool Tip",
		WS_CHILD|WS_VISIBLE,
		x, y, w, h,
		Parent, NULL, hInstance, NULL
	);

	EncodingToolTip::items[ hWnd ] = this;

	hParent = Parent;

	hEdgePen = CreatePen( PS_SOLID, 1, 0x00000000 );

	HDC hDC = GetDC( hWnd );

	hCanvas = CreateCompatibleDC( hDC );

	hCanvasBitmap = CreateCompatibleBitmap( hDC, w + 4, h + 4 );

	hOld = SelectObject( hCanvas, hCanvasBitmap );

	ReleaseDC( hWnd, hDC );

	SetTimer( hWnd, 0x17171, 10, NULL );

	return 0;
}

LRESULT	EncodingToolTip::WndProc(HWND hWindow, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		{
			PaintItem();
		}
		break;

	case WM_TIMER:
		if ( wParam == 0x17171 )
		{
			KillTimer( hWnd, 0x17171 );

			IgnoreEvents = false;
		}
		break;

	case WM_MOUSEMOVE:
		if ( !IgnoreEvents )
		{			
			DWORD mouseX	= GET_X_LPARAM(lParam);
			DWORD mouseY	= GET_Y_LPARAM(lParam);

			DWORD mouse  = ( mouseY + (DWORD) oy ) << 16;
			      mouse |= mouseX + (DWORD) ox;

			/* Proxy this to the parent so the tooltip goes away */
			PostMessage( hParent, WM_MOUSEMOVE, 0, (LPARAM) mouse );
		}
		break;

	case WM_LBUTTONDOWN:
		{			
			DWORD mouseX	= GET_X_LPARAM(lParam);
			DWORD mouseY	= GET_Y_LPARAM(lParam);

			DWORD mouse  = ( mouseY + (DWORD) oy ) << 16;
			      mouse |= mouseX + (DWORD) ox;

			/* Proxy this to the parent so the tooltip goes away */
			SendMessage( hParent, WM_MOUSEMOVE, 0, (LPARAM) mouse );
			SendMessage( hParent, WM_LBUTTONDOWN, 0, (LPARAM) mouse );
		}
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

void EncodingToolTip::PaintItem( void )
{
	HDC hDC = GetDC( hWnd );

	SelectObject( hCanvas, hEdgePen );

	RECT r;

	GetClientRect( hWnd, &r );

	FillRect( hCanvas, &r, GetSysColorBrush( COLOR_INFOBK ) );

	MoveToEx( hCanvas, 0, 0, NULL );
	LineTo( hCanvas, r.right - r.left - 1, 0 );
	LineTo( hCanvas, r.right - r.left - 1, r.bottom - r.top - 1 );
	LineTo( hCanvas, 0, r.bottom - r.top - 1 );
	LineTo( hCanvas, 0, 0 );

	int l = 0;

	for ( TooltipDef_iter i = _tip.begin(); i != _tip.end(); i++ )
	{
		FontBitmap tbitmap( i->FontID, i->Text, i->Text.length(), false, false );

		COLORREF b = GetSysColor( COLOR_INFOBK );
		COLORREF t = GetSysColor( COLOR_INFOTEXT );

		tbitmap.SetButtonColor( GetRValue( b ), GetGValue( b ), GetBValue( b ) );
		tbitmap.SetTextColor( GetRValue( t ), GetGValue( t ), GetBValue( t ) );

		tbitmap.DrawText( hCanvas, 2, l * 17 + 2, DT_LEFT | DT_TOP );

		l++;
	}

	BitBlt( hDC, 0, 0, r.right - r.left, r.bottom - r.top, hCanvas, 0, 0, SRCCOPY );

	ReleaseDC( hWnd, hDC );
}