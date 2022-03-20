#include "StdAfx.h"
#include "SidePanel.h"

#include "BitmapCache.h"

#include "NUTSIDM.h"
#include "NUTSMacros.h"

#include "resource.h"

std::map<HWND, SidePanel *> SidePanel::panels;

bool SidePanel::WndClassReg = false;

const wchar_t SidePanelClass[]  = L"NUTS Side Panel";

LRESULT CALLBACK SidePanel::SidePanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( panels.find( hWnd ) != panels.end() )
	{
		return panels[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

SidePanel::SidePanel(void)
{
	if ( !WndClassReg )
	{
		WNDCLASS wc;

		ZeroMemory( &wc, sizeof( wc ) );

		wc.lpfnWndProc   = SidePanel::SidePanelProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = SidePanelClass;
		wc.hCursor       = LoadCursor( NULL, IDC_ARROW );

		RegisterClass(&wc);

		WndClassReg = true;
	}

	hWnd    = NULL;
	hParent = NULL;
	hCorner = NULL;
	hTopic  = NULL;
	
	hOldTopic  = NULL;
	hTopicFont = NULL;
	SepPen     = NULL;

	TopicIcon = FT_System;

	InitializeCriticalSection( &TopicLock );

	PanelFlags = SPF_RootFS;

	backItem = newdirItem = newimageItem = topItem = upItem = downItem = renameItem = deleteItem = copyItem = playItem = nullptr;
}


SidePanel::~SidePanel(void)
{
	for ( auto item = items.begin(); item != items.end(); item++ )
	{
		delete *item;
	}

	items.shrink_to_fit();

	NixObject( hCorner );

	if ( hOldTopic != NULL )
	{
		SelectObject( hTopic, hOldTopic );
	}

	NixObject( hTopic );
	NixObject( hTopicFont );

	SidePanel::panels.erase( hWnd );

	NixWindow( hWnd );

	DeleteCriticalSection( &TopicLock );
}

int SidePanel::Create( HWND Parent, HINSTANCE hInstance )
{
	RECT r;

	GetClientRect( hParent, &r );

	r.top += 24;

	hWnd = CreateWindowEx(
		WS_EX_CONTROLPARENT,
		SidePanelClass,
		L"NUTS Side Panel",
		WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|DS_CONTROL,
		0, 24, 128, r.bottom - r.top,
		Parent, NULL, hInstance, NULL
	);

	SidePanel::panels[ hWnd ] = this;

	hParent = Parent;

	hTopicFont	= CreateFont( 16,0,0,0,FW_DONTCARE,TRUE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));

	HDC hDC = GetDC( hWnd );

	hCorner = CreateCompatibleDC( hDC );
	hTopic  = CreateCompatibleDC( hDC );

	SelectObject( hCorner, LoadBitmap( hInst, MAKEINTRESOURCE( IDB_SIDEBARCORNER ) ) );
	
	ReleaseDC( hWnd, hDC );

	SepPen = CreatePen( PS_SOLID, 1, 0x00909090 );

	SidebarItem *item; int y = 160;

	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_BACK, std::wstring( L"Back" ), y, Parent, IDM_PARENT, true ); items.push_back( item ); y += 28; backItem = item;
	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_FONTSWITCH, std::wstring( L"Change Font" ), y, Parent, IDM_FONTSW, true ); items.push_back( item ); y += 28;
	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_NEWDIR, std::wstring( L"New Directory" ), y, Parent, IDM_NEWFOLDER, false ); items.push_back( item ); y += 28; newdirItem = item;
	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_NEWDIR, std::wstring( L"New Image" ), y, Parent, IDM_NEWIMAGE, false ); items.push_back( item ); y += 28; newimageItem = item;
	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_ROOTFS, std::wstring( L"Top Level" ), y, Parent, IDM_ROOTFS, false ); items.push_back( item ); y += 28; topItem = item;
	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_UPFILE, std::wstring( L"Move Up" ), y, Parent, IDM_MOVEUP, false ); items.push_back( item ); y += 28; upItem = item;
	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_DOWNFILE, std::wstring( L"Move Down" ), y, Parent, IDM_MOVEDOWN, false ); items.push_back( item ); y += 28; downItem = item;
	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_REFRESH, std::wstring( L"Refresh" ), y, Parent, IDM_REFRESH, false ); items.push_back( item ); y += 28;

	line1 = y + 8;

	y+=12;

	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_RENAME, std::wstring( L"Rename" ), y, Parent, IDM_RENAME, false ); items.push_back( item ); y += 28; renameItem = item;
	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_DELETEFILE, std::wstring( L"Delete" ), y, Parent, IDM_DELETE, false ); items.push_back( item ); y += 28; deleteItem = item;
	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_COPYFILE, std::wstring( L"Copy Files" ), y, Parent, IDM_COPY, false ); items.push_back( item ); y += 28; copyItem = item;

	line2 = y + 8;

	y+=12;

	item = new SidebarItem(); item->Create( hWnd, hInstance, IDI_AUDIO, std::wstring( L"Play Audio" ), y, Parent, IDM_PLAY_AUDIO, false ); items.push_back( item ); y += 28; playItem = item;

	SetFlags( PanelFlags );

	return 0;
}

int SidePanel::Resize()
{
	RECT r;

	GetClientRect( hParent, &r );

	r.top += 24;

	SetWindowPos( hWnd, NULL, 0, 24, 128, r.bottom - r.top, SWP_NOREPOSITION );

	return 0;
}

LRESULT	SidePanel::WndProc(HWND hWindow, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		{
			PaintPanel();
		}
		break;

	case WM_CLOSE:
		{
			panels.erase( hWnd );
		}
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		PostMessage( hParent, message, wParam, lParam );
		
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}


void SidePanel::PaintPanel( void )
{
	RECT r;

	GetClientRect( hWnd, &r );

	HDC hDC = GetDC( hWnd );

	HDC hArea = CreateCompatibleDC( hDC );

	HBITMAP hAreaCanvas = CreateCompatibleBitmap( hDC, r.right - r.left, r.bottom - r.top );

	HGDIOBJ hAreaOld = SelectObject( hArea, hAreaCanvas );

	FillRect( hArea, &r, (HBRUSH) GetStockObject( WHITE_BRUSH ) );

	BitBlt( hArea, 0, 0, 48, 48, hCorner, 0, 0, SRCCOPY );

	EnterCriticalSection( &TopicLock );

	HBITMAP hTopicIcon = BitmapCache.GetBitmap( TopicIcon );

	BITMAP bm; 
 
	GetObject( hTopicIcon, sizeof(BITMAP), &bm);
 
	hOldTopic = SelectObject( hTopic, hTopicIcon );

	BitBlt( hArea, 64 - (bm.bmWidth / 2), 64 - (bm.bmHeight / 2), bm.bmWidth, bm.bmHeight, hTopic, 0, 0, SRCCOPY );

	SelectObject( hTopic, hOldTopic );

	/* Draw out the descriptor (FS Name) */

	RECT tr = r;

	tr.top = 96;
	tr.bottom = tr.top + 48;
	tr.left += 12;
	tr.right -= 12;

	SelectObject( hArea, hTopicFont );

	DrawText( hArea, TopicDescriptor.c_str(), (int) TopicDescriptor.length(), &tr, DT_CENTER | DT_TOP | DT_WORDBREAK );

	SelectObject( hArea, SepPen );

	MoveToEx( hArea, 2, 154, NULL );
	LineTo( hArea, 120, 154 );

	MoveToEx( hArea, 2, line1, NULL );
	LineTo( hArea, 120, line1 );

	MoveToEx( hArea, 2, line2, NULL );
	LineTo( hArea, 120, line2 );

	LeaveCriticalSection( &TopicLock );

	BitBlt( hDC, 0, 0, r.right - r.left, r.bottom - r.top, hArea, 0, 0, SRCCOPY );

	SelectObject( hArea, hAreaOld );

	DeleteObject( hAreaCanvas );

	DeleteDC( hArea );

	ReleaseDC( hWnd, hDC );
}

int SidePanel::SetTopic( DWORD ft, std::wstring Descriptor )
{
	EnterCriticalSection( &TopicLock );

	TopicIcon = ft;

	TopicDescriptor = Descriptor;

	LeaveCriticalSection( &TopicLock );

	return 0;
}

int SidePanel::SetFlags( DWORD f )
{
	PanelFlags = f;

	backItem->SetState( f & SPF_RootFS );
	topItem->SetState( f & SPF_RootFS );
	upItem->SetState( ! ( f & SPF_ReOrderable ) );
	downItem->SetState( ! ( f & SPF_ReOrderable ) );
	newdirItem->SetState( ! ( f & SPF_NewDirs ) );
	newimageItem->SetState( ! ( f & SPF_NewImages ) );
	renameItem->SetState( ! ( f & SPF_Rename ) );
	deleteItem->SetState( ! ( f & SPF_Delete ) );
	copyItem->SetState( ! ( f & SPF_Copy ) );
	playItem->SetState( ! ( f & SPF_Playable ) );

	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );

	return 0;
}