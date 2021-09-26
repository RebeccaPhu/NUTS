#include "StdAfx.h"
#include "SidebarItem.h"

#include "resource.h"

std::map<HWND, SidebarItem *> SidebarItem::items;

bool SidebarItem::WndClassReg = false;

const wchar_t SidebarItemClass[]  = L"NUTS Side Panel Item";

LRESULT CALLBACK SidebarItem::SidebarItemProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( items.find( hWnd ) != items.end() )
	{
		return items[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

SidebarItem::SidebarItem(void)
{
	if ( !WndClassReg )
	{
		WNDCLASS wc;

		ZeroMemory( &wc, sizeof( wc ) );

		wc.lpfnWndProc   = SidebarItem::SidebarItemProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = SidebarItemClass;
		wc.hCursor       = LoadCursor( NULL, IDC_ARROW );

		RegisterClass(&wc);

		WndClassReg = true;
	}

	hWnd    = NULL;
	hParent = NULL;
	hFont   = NULL;

	Hover    = false;
	Disabled = false;
	Click    = false;
	Focus    = false;

	HoverBrush = NULL;
	ClickBrush = NULL;
	FocusPen   = NULL;
}


SidebarItem::~SidebarItem(void)
{
	NixObject( hFont );
	NixObject( HoverBrush );
	NixObject( ClickBrush );
	NixObject( FocusPen );

	SidebarItem::items.erase( hWnd );

	NixWindow( hWnd );
}

int SidebarItem::Create(  HWND Parent, HINSTANCE hInstance, int Icon, std::wstring &text, int y, HWND hTarget, DWORD param, bool bGroup )
{
	RECT r;

	GetClientRect( hParent, &r );

	r.top += 24;

	DWORD xStyle = NULL;

	if ( bGroup )
	{
		xStyle = WS_GROUP;
	}

	hWnd = CreateWindowEx(
		NULL,
		SidebarItemClass,
		L"NUTS Side Panel Item",
		WS_CHILD|WS_VISIBLE|WS_TABSTOP|xStyle,
		0, y, 128, 28,
		Parent, NULL, hInstance, NULL
	);

	SidebarItem::items[ hWnd ] = this;

	hParent       = Parent;
	hTargetWindow = hTarget;
	SourceCommand = param;

	hIcon = LoadIcon( hInstance, MAKEINTRESOURCE( Icon ) );

	Text  = text;

	hFont	= CreateFont( 16,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));

	LOGBRUSH brsh;

	brsh.lbColor = 0x00FFF0F0;
	brsh.lbStyle = BS_SOLID;

	HoverBrush = CreateBrushIndirect( &brsh );

	brsh.lbColor = 0x00EEE0E0;

	ClickBrush = CreateBrushIndirect( &brsh );

	FocusPen = CreatePen( PS_DOT, 1, 0x00A0A0A0 );

	Tracking = false;

	return 0;
}

LRESULT	SidebarItem::WndProc(HWND hWindow, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		{
			PaintItem();
		}
		break;

	case WM_CLOSE:
		{
			items.erase( hWnd );
		}
		break;

	case WM_MOUSEMOVE:
		{
			Hover = true;

			if ( !Tracking )
			{
				TRACKMOUSEEVENT tme;

				tme.cbSize    = sizeof( tme );
				tme.dwFlags   = TME_LEAVE;
				tme.hwndTrack = hWnd;

				TrackMouseEvent( &tme );

				Tracking = true;
			}

			Refresh();
		}
		break;

	case WM_MOUSELEAVE:
		{
			Hover    = false;
			Tracking = false;
			Click    = false;

			Refresh();
		}
		break;

	case WM_LBUTTONDOWN:
		{
			Click = true;

			Refresh();
		}
		break;

	case WM_LBUTTONUP:
		{
			Click = false;

			Refresh();

			if ( !Disabled )
			{
				::PostMessage( hTargetWindow, WM_COMMAND, (WPARAM) (WORD) SourceCommand, (LPARAM) hWnd );
			}
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

	case WM_KEYDOWN:
	case WM_KEYUP:
		PostMessage( hParent, message, wParam, lParam );
		
		break;
/*
	case WM_GETDLGCODE:
		switch ( wParam )
			{
				case VK_RETURN:
					return DLGC_BUTTON;

			}
		
		break;
/*
	case WM_KEYDOWN:
		{
			if ( wParam == VK_TAB )
			{
				SetFocus( GetNextDlgTabItem( hParent, hWnd, FALSE ) );

				return 0;
			}
		}
		break;
*/		
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}


void SidebarItem::PaintItem( void )
{
	RECT r;

	GetClientRect( hWnd, &r );

	HDC hDC = GetDC( hWnd );

	HDC hArea = CreateCompatibleDC( hDC );

	HBITMAP hAreaCanvas = CreateCompatibleBitmap( hDC, r.right - r.left, r.bottom - r.top );

	HGDIOBJ hAreaOld = SelectObject( hArea, hAreaCanvas );

	HBRUSH hBrsh = (HBRUSH) GetStockObject( WHITE_BRUSH );

	if ( Hover ) { hBrsh = HoverBrush; SetBkColor( hArea, 0x00FFF0F0 ); }
	if ( Click ) { hBrsh = ClickBrush; SetBkColor( hArea, 0x00EEE0E0 ); }

	RECT tr = r;

	tr.right -= 4;

	FillRect( hArea, &r, hBrsh );

	tr.right -= 4;

	tr.left += 28;

	SelectObject( hArea, hFont );

	if ( !Disabled )
	{
		DrawIconEx( hArea, 4, 7, hIcon, 18, 18, 0, NULL, DI_NORMAL );
		SetTextColor( hArea, 0x00000000 );
	}
	else
	{
//		DrawState( hArea, NULL, NULL, (LPARAM) hIcon, 0, 4, 7, 12, 16, DST_ICON | DSS_DISABLED );
		DrawIconEx( hArea, 4, 7, hIcon, 18, 18, 0, NULL, DI_NORMAL );
		SetTextColor( hArea, 0x00888888 );
	}

	tr.top += 4;

	DrawText( hArea, Text.c_str(), (int) Text.length(), &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS );

	if ( Focus )
	{
		SelectObject( hArea, FocusPen );

		MoveToEx( hArea, 0, 0, NULL );
		LineTo( hArea, 124, 0 );
		LineTo( hArea, 124, 27 );
		LineTo( hArea, 0, 27 );
		LineTo( hArea, 0, 0 );
	}

	BitBlt( hDC, 0, 0, r.right - r.left, r.bottom - r.top, hArea, 0, 0, SRCCOPY );

	SelectObject( hArea, hAreaOld );

	DeleteObject( hAreaCanvas );

	DeleteDC( hArea );

	ReleaseDC( hWnd, hDC );
}

void SidebarItem::Refresh()
{
	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );
}