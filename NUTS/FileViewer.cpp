#include "StdAfx.h"
#include "FileViewer.h"
#include "FormatWizard.h"
#include "FontBitmap.h"
#include "Plugins.h"
#include "libfuncs.h"
#include "IconRatio.h"

#include <CommCtrl.h>

#include <WindowsX.h>

std::map<HWND, CFileViewer *> CFileViewer::viewers;

bool CFileViewer::WndClassReg = false;

LRESULT CALLBACK CFileViewer::FileViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( viewers.find( hWnd ) != viewers.end() )
	{
		return viewers[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

CFileViewer::CFileViewer(void) {
	if ( !WndClassReg )
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= CFileViewer::FileViewerProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInst;
		wcex.hIcon			= NULL;
		wcex.hCursor		= NULL;
		wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
		wcex.lpszMenuName	= NULL; // MAKEINTRESOURCE(IDC_NUTS);
		wcex.lpszClassName	= L"NUTS File Browser Pane";
		wcex.hIconSm		= NULL;

		RegisterClassEx(&wcex);

		WndClassReg = true;
	}

	KnownX	= 0;
	KnownY	= 0;

	viewBuffer	= NULL;
	viewDC		= NULL;

	FS			= NULL;

	FileEntries    = 0;
	ParentSelected = false;
	Updated        = true;
	ScrollStartY   = 0;
	ScrollMax      = 0;
	LastClick      = 0;
	Dragging       = false;
	Bounding       = false;
	MouseDown      = false;
	IsSearching    = false;
	HasFocus       = false;
	DragType       = 0;
	dragX          = -1;
	dragY          = -1;
	CurrentFSID    = FS_Root;
	PaneIndex      = 0;
	Displaying     = DisplayLargeIcons;

	SelectionAtMouseDown = false;

	MenuFSMap.clear();
	ControlButtons.clear();
	FileSelections.clear();
	SelectionStack.clear();
	SelectionStack.push_back( -1 );

	/* These values will be overridden during the resize */
	ItemWidth       = 64U;
	ItemHeight      = 100U;
	IconsPerLine    = 6U;
	LinesPerWindow  = 12U;
	IconsPerWindow  = 72U;
	IconYOffset     = 6U;
	WindowWidth     = 500U;
	WindowHeight    = 700U;
	CalculatedY     = 0U;

	LOGBRUSH brsh;

	brsh.lbColor = RGB( 0x55, 0x55, 0x55 );
	brsh.lbStyle = BS_SOLID;

	DWORD Pen1[4] = { 5,  5, 10, 5 };
	DWORD Pen2[4] = { 7,  5, 7,  5 };
	DWORD Pen3[4] = { 10, 5, 5,  5 };

	Pens[0] = ExtCreatePen( PS_COSMETIC | PS_USERSTYLE, 1, &brsh, 4, Pen1 );
	Pens[1] = ExtCreatePen( PS_COSMETIC | PS_USERSTYLE, 1, &brsh, 4, Pen2 );
	Pens[2] = ExtCreatePen( PS_COSMETIC | PS_USERSTYLE, 1, &brsh, 4, Pen3 );

	hViewerBrush = NULL;

	CurrentPen = 0;

	LastItemIndex = 0x7FFFFFFF;
}

CFileViewer::~CFileViewer(void) {
	if ( hViewerBrush != NULL )
	{
		DeleteObject( (HGDIOBJ) hViewerBrush );
	}

	if (viewBuffer) {
		SelectObject(viewDC, viewObj);

		DeleteObject(viewBuffer);
		DeleteDC(viewDC);
	}
}

int CFileViewer::Create(HWND Parent, HINSTANCE hInstance, int x, int w, int h) {
	titleBarFont	= CreateFont(16,6,0,0,FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"MS Shell Dlg");
	filenameFont	= CreateFont(12,5,0,0,FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"MS Shell Dlg");

	hWnd = CreateWindowEx(NULL, L"NUTS File Browser Pane", L"", SS_BLACKFRAME|SS_NOTIFY|SS_OWNERDRAW|WS_CHILD|WS_VISIBLE, x, 0, w, h, Parent, NULL, hInstance, NULL);
	//SS_WHITERECT|

	viewers[ hWnd ] = this;

	ParentWnd	= Parent;

	int tw = w - 140;

	hScrollBar	= CreateWindowEx(NULL, L"SCROLLBAR", L"", WS_CHILD|WS_VISIBLE|SBS_VERT|WS_DISABLED, w - 24, 24, 16, h, hWnd, NULL, hInst, NULL);

	ControlButtons.push_back( CreateWindowEx(NULL, L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_ICON, tw,       0, 24, 24, hWnd, NULL, hInst, NULL) );
	ControlButtons.push_back( CreateWindowEx(NULL, L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_ICON, tw + 25,  0, 24, 24, hWnd, NULL, hInst, NULL) );
	ControlButtons.push_back( CreateWindowEx(NULL, L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_ICON, tw + 50,  0, 24, 24, hWnd, NULL, hInst, NULL) );
	ControlButtons.push_back( CreateWindowEx(NULL, L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_ICON, tw + 75,  0, 24, 24, hWnd, NULL, hInst, NULL) );
	ControlButtons.push_back( CreateWindowEx(NULL, L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_ICON, tw + 100, 0, 24, 24, hWnd, NULL, hInst, NULL) );

	HICON hFontIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FONTSWITCH));

	SendMessage( ControlButtons[0], BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hFontIcon );

	HICON hRootIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ROOTFS));

	SendMessage( ControlButtons[1], BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hRootIcon );

	HICON hUpIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_UPFILE));

	SendMessage( ControlButtons[2], BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hUpIcon );

	HICON hDownIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DOWNFILE));

	SendMessage( ControlButtons[3], BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hDownIcon );

	HICON hNewIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NEWDIR));

	SendMessage( ControlButtons[4], BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hNewIcon );

	EnableWindow( ControlButtons[2], FALSE );
	EnableWindow( ControlButtons[3], FALSE );

	hSearchAVI = Animate_Create( hWnd, 42995, WS_CHILD | ACS_TRANSPARENT, hInst );
	
	Animate_Open( hSearchAVI, MAKEINTRESOURCE( IDV_SEARCH ) );

	Animate_Play( hSearchAVI, 0, -1, -1 );

	return 0;
}

void CFileViewer::SetDragType(int DType) {
	DType	= DragType;
}

bool CFileViewer::CheckClick() {
	DWORD	thisTime	= GetTickCount();

	char err[256];
	sprintf_s(err, 256, "This: %08X Last: %08X Check: %08X\n", thisTime, LastClick, GetDoubleClickTime());
	OutputDebugStringA(err);

	if ((thisTime - LastClick) < GetDoubleClickTime()) {
		LastClick = 0; // Prevents dclick + click being interpreted as triple-click

		return false;
	}

	LastClick	= thisTime;

	return true;
}

void CFileViewer::CheckDragType(long dragX, long dragY) {
	RECT	rc;

	GetClientRect(hWnd, &rc);

	long	ww	= rc.right - rc.left;
	long	wh	= rc.bottom - rc.top;

	long	we	= (ww / 100) * 100;

	if (dragX > we) {
		::SendMessage(ParentWnd, WM_SETDRAGTYPE, 1, 0);

		return;
	}

	long	hi	= ww/100;
	long	ip;

	for (int x = 0; x<hi; x++) {
		ip	= (x * 100) + 50;

		if ((dragX > (ip - 16)) && (dragX < (ip + 16))) {
			::SendMessage(ParentWnd, WM_SETDRAGTYPE, 2, 0);

			return;
		}
	}

	::SendMessage(ParentWnd, WM_SETDRAGTYPE, 1, 0);
}

void CFileViewer::DoScroll(WPARAM wParam, LPARAM lParam) {
	if (LOWORD(wParam) == SB_THUMBTRACK) {
		ScrollStartY = HIWORD(wParam) * 8;

		Update();
	}

	if (LOWORD(wParam) == SB_LINEUP) {
		ScrollStartY -= 8;

		if (ScrollStartY < 0)
			ScrollStartY = 0;

		SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

		Update();
	}

	if (LOWORD(wParam) == SB_PAGEUP) {
		ScrollStartY -= 64;

		if (ScrollStartY < 0)
			ScrollStartY = 0;

		SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

		Update();
	}

//	GetScrollRange(hScrollBar, SB_CTL, &min, &max);

	if (LOWORD(wParam) == SB_LINEDOWN) {
		ScrollStartY += 8;

		if (ScrollStartY > ScrollMax)
			ScrollStartY = ScrollMax;

		SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

		Update();
	}

	if (LOWORD(wParam) == SB_PAGEDOWN) {
		ScrollStartY += 64;

		if (ScrollStartY > ScrollMax)
			ScrollStartY = ScrollMax;

		SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

		Update();
	}

	if (LOWORD(wParam) == SB_ENDSCROLL) {
		SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

		Update();
	}
}

void CFileViewer::GetCliRect( RECT &client )
{
	GetClientRect( hWnd, &client );

	client.right -= 16;
	client.top   += 24;
}

LRESULT	CFileViewer::WndProc(HWND hSourceWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	HMENU  hPopup,hSubMenu;
	RECT   DimRect;	
	static WCHAR	*blank	= L"";
	static WCHAR	multi[64];

	switch (message) {
		case WM_COMMAND:
			if ( (HWND) lParam == ControlButtons[ 0 ] )
			{
				if ( FS->pDirectory->Files.size() > 0 )
				{
					FSPlugins.NextFont( FS->pDirectory->Files[ 0 ].EncodingID, PaneIndex );

					Redraw();
				}
			}

			if ( (HWND) lParam == ControlButtons[ 1 ] )
			{
				SendMessage( ParentWnd, WM_ROOTFS, (WPARAM) this, (LPARAM) NULL );
			}

			if ( (HWND) lParam == ControlButtons[ 3 ] )
			{
				DoSwapFiles( 0xFF );
			}

			if ( (HWND) lParam == ControlButtons[ 4 ] )
			{
				DoSwapFiles( 0x00 );
			}

			if ( ( LOWORD(wParam) >= FILESYS_MENU_BASE ) && ( LOWORD(wParam) <= FILESYS_MENU_END ) )
			{
				DWORD items = GetSelectionCount();

				if ( items == 1 )
				{
					::SendMessage( ParentWnd, WM_DOENTERAS, (WPARAM) hWnd, MenuFSMap[ LOWORD( wParam ) ] );
				}

				if ( items > 1 )
				{
					::MessageBox( hWnd, L"Can't enter multiple files as file systems. Select one item only.", L"Too many items", MB_ICONEXCLAMATION | MB_OK );
				}
			}

			if ( ( LOWORD(wParam) >= GFX_MENU_BASE ) && ( LOWORD(wParam) <= GFX_MENU_END ) )
			{
				DoSCREENContentViewer( MenuXlatorMap[ LOWORD( wParam ) ] );
			}

			if ( ( LOWORD(wParam) >= TXT_MENU_BASE ) && ( LOWORD(wParam) <= TXT_MENU_END ) )
			{
				DoTEXTContentViewer( MenuXlatorMap[ LOWORD( wParam ) ] );
			}

			switch ( LOWORD(wParam) ) {
				case IDM_REFRESH:
					{
						FS->Refresh();
					
						Updated = true;
						
						Redraw();
					}
					break;

				case IDM_NEWIMAGE:
					{
						static DWORD Encoding = ENCODING_ASCII;
						
						Encoding = FS->GetEncoding();

						AppAction Action;
						
						Action.Action = AA_NEWIMAGE;
						Action.FS     = FS;
						Action.hWnd   = hWnd;
						Action.Pane   = this;
						Action.pData  = &Encoding;

						Action.Selection.clear();

						QueueAction( Action );
					}
					break;

				case IDM_COPY:
					{
						::PostMessage(ParentWnd, WM_COPYOBJECT, (WPARAM) hWnd, 0 );
					}

					break;

				case IDM_DELETE:
					{
						AppAction Action;

						Action.Action = AA_DELETE;
						Action.FS     = FS;
						Action.hWnd   = hWnd;
						Action.Pane   = this;
						Action.pData  = nullptr;

						Action.Selection = GetSelection();

						QueueAction( Action );
					}
					break;

				case IDM_FORMAT:
					{
						AppAction Action;

						Action.Action    = AA_FORMAT;
						Action.FS        = FS;
						Action.hWnd      = hWnd;
						Action.Selection = GetSelection();
						Action.Pane	     = this;
						Action.pData     = NULL;

						QueueAction( Action );
					}

					break;

				case IDM_PROPERTIES:
					{
						AppAction Action;

						Action.Action    = AA_PROPS;
						Action.FS        = FS;
						Action.hWnd      = hWnd;
						Action.Selection = GetSelection();
						Action.Pane	     = this;
						Action.pData     = NULL;

						QueueAction( Action );
					}
					break;

				case IDM_ENTER:
					ActivateItem(mouseX, mouseY);

					break;

				case IDM_RENAME:
					{
						RenameFile();
					}
					break;

				case IDM_LARGEICONS:
					Displaying = DisplayLargeIcons;

					Updated = true;
					GetCliRect( DimRect );
					RecalculateDimensions( DimRect );
					Redraw();
					break;

				case IDM_DETAILS:
					Displaying = DisplayDetails;

					Updated = true;
					GetCliRect( DimRect );
					RecalculateDimensions( DimRect );
					Redraw();
					break;
				case IDM_FILELIST:
					Displaying = DisplayList;

					Updated = true;
					GetCliRect( DimRect );
					RecalculateDimensions( DimRect );
					Redraw();
					break;
			}

			for ( std::map<UINT, DWORD>::iterator m=MenuFSMap.begin(); m!=MenuFSMap.end(); m++ )
			{
				if ( wParam == m->first )
				{
					// TODO: Fire a function here
				}
			}

			return DefWindowProc(hSourceWnd, message, wParam, lParam);

		case WM_SCCLOSED:
			{
				CSCREENContentViewer *pClass = (CSCREENContentViewer *) lParam;

				delete pClass;
			}

			return 0;

		case WM_VSCROLL:
			DoScroll(wParam, lParam);

			return 0;

		case WM_MOUSEACTIVATE:
				char a[256];
				sprintf(a, "Window %08X WM_ACTIVATE %08x %08X", hWnd, wParam, lParam );
				OutputDebugStringA( a );

			::SetFocus( hWnd );

			Redraw();

			return MA_ACTIVATE;

		case WM_SETFOCUS:
			HasFocus = true;

				sprintf(a, "Window %08X has focus\n", hWnd );
				OutputDebugStringA( a );

			::PostMessage( ParentWnd, WM_FV_FOCUS, (WPARAM) hWnd, 0 );

			Redraw();

			return 0;

		case WM_KILLFOCUS:
			HasFocus = false;

				sprintf(a, "Window %08X lost focus\n", hWnd );
				OutputDebugStringA( a );

			Redraw();

			return 0;

		case WM_GETDLGCODE:
			switch ( wParam )
				{
					case VK_TAB:
					case VK_RETURN:
						return DLGC_STATIC;
			
					case VK_LEFT:
					case VK_RIGHT:
					case VK_HOME:
					case VK_END:
						return DLGC_WANTARROWS;

					case VK_SHIFT:
					case VK_CONTROL:
						return DLGC_WANTALLKEYS;

					default:
						return DLGC_WANTCHARS;
				}
		
			return DLGC_STATIC;

		case WM_KEYUP:
			DoKeyControls( message, wParam, lParam );
			break;

		case WM_PAINT:
			Redraw();

			return DefWindowProc(hSourceWnd, message, wParam, lParam);

		case WM_LBUTTONDOWN:
		case WM_MOUSEMOVE:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
			DoSelections( message, wParam, lParam );

			return DefWindowProc(hSourceWnd, message, wParam, lParam);

		case WM_TIMER:
			if ( wParam == 0xB011D )
			{
				Redraw();
			}

			return DefWindowProc( hSourceWnd, message, wParam, lParam );

		case WM_MOUSELEAVE:
			if ( Bounding )
			{
				Bounding  = false;
				MouseDown = false;
				dragX     = -1;
				dragY     = -1;

				OutputDebugStringA( "Bounding left window!\n" );

				KillTimer( hWnd, 0xB011D );

				Redraw();
			}

			return DefWindowProc(hSourceWnd, message, wParam, lParam);

		case WM_RBUTTONUP:
		{
			DoContextMenu();

			return DefWindowProc(hSourceWnd, message, wParam, lParam);
		}

		case WM_ERASEBKGND:
			return FALSE;

		case WM_CTLCOLORSTATIC:
			{
				return (LRESULT) GetStockObject( WHITE_BRUSH );
			}
			break;

		case WM_MOUSEWHEEL:
			ScrollStartY		-=	(GET_WHEEL_DELTA_WPARAM(wParam) / 2);

			if (ScrollStartY < 0)
				ScrollStartY = 0;

			if (ScrollStartY > ScrollMax)
				ScrollStartY	= ScrollMax;

			SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

			Update();

			break;

		default:
			break;
	}

	return DefWindowProc( hSourceWnd, message, wParam, lParam);
}

void CFileViewer::Resize(int w, int h)
{
	SetWindowPos( hScrollBar, NULL, w - 16, 24, 16, h - 24, NULL );

	std::vector<HWND>::iterator i;

	RECT ThisRect;

	GetCliRect( ThisRect );

	DWORD s = ( ThisRect.right + 16 ) - ThisRect.left;

	s -= ControlButtons.size() * 24;
	s+=1;

	for ( i=ControlButtons.begin(); i != ControlButtons.end(); i++ )
	{
		HWND h = *i;

		SetWindowPos( h, NULL, s, 0, 24, 24, NULL );

		s+= 24;
	}

	ThisRect.right -= 16;
	ThisRect.top   += 24;

	RecalculateDimensions( ThisRect );

	SetWindowPos( hSearchAVI, NULL, ((ThisRect.right - ThisRect.left) / 2) - 24, ((ThisRect.bottom - ThisRect.top) / 2), 48, 50, SWP_NOZORDER | SWP_NOREPOSITION );
}

void CFileViewer::DrawBasicLayout() {
	RECT	rect;

	GetClientRect(hWnd, &rect);

	rect.top	+= 22;

	rect.right -= 15;

	DrawEdge(viewDC, &rect, EDGE_SUNKEN, BF_RECT);

	rect.right += 15;
	rect.right -= 24 * ControlButtons.size();

	rect.top	-= 22;

	DWORD BColour = GetSysColor(COLOR_BTNFACE);

	if ( hViewerBrush == NULL )
	{
		hViewerBrush = CreateSolidBrush( BColour );
	}

	rect.bottom	= rect.top + 24;

	FillRect(viewDC, &rect, hViewerBrush);

	DrawEdge(viewDC, &rect, EDGE_RAISED, BF_RECT);

	if ( IsSearching )
	{
		FontBitmap TitleString( FONTID_PC437, (BYTE *) "Loading...", (BYTE) ( ( rect.right - rect.left ) / 8 ), false, false );

		TitleString.SetGrayed( !HasFocus );

		TitleString.DrawText( viewDC, 6, 4, DT_TOP | DT_LEFT );

		return;
	}

	/* Collect the title string stack */
	std::vector<TitleComponent>::iterator iStack;

	DWORD FullWidth = 0;
	DWORD MaxWidth  = ( ( rect.right - rect.left ) / 8 ) - 1;

	for ( iStack = TitleStack.begin(); iStack != TitleStack.end(); iStack++ )
	{
		FullWidth += strlen( (char *) iStack->String );
	}

	int offset = 0;

	if ( MaxWidth < FullWidth )
	{
		offset = ( MaxWidth - FullWidth ) * 8;
	}

	int coffset = offset;

	for ( iStack = TitleStack.begin(); iStack != TitleStack.end(); iStack++ )
	{
		FontBitmap TitleString( FSPlugins.FindFont( iStack->Encoding, PaneIndex ), iStack->String, (BYTE) ( ( rect.right - rect.left ) / 8 ), false, false );

		TitleString.SetButtonColor( GetRValue( BColour ), GetGValue( BColour ), GetBValue( BColour ) );

		TitleString.SetGrayed( !HasFocus );

		TitleString.DrawText( viewDC, 6 + coffset, 4, DT_TOP | DT_LEFT );

		coffset += ( strlen( (char *) iStack->String ) * 8 );
	}
}

void CFileViewer::DrawFile(int i, NativeFile *pFile, DWORD Icon, bool Selected) {
	HDC	hSourceDC = CreateCompatibleDC(viewDC);

	HBITMAP hIcon        = NULL;
	HBITMAP hCreatedIcon = NULL;

	DWORD      iw = 34;
	DWORD      ih = 34;
	DWORD      rw = 34;
	DWORD      rh = 34;

	if ( pFile->HasResolvedIcon )
	{
		if ( FS->pDirectory->ResolvedIcons.find( pFile->fileID ) != FS->pDirectory->ResolvedIcons.end() )
		{
			IconDef icon = FS->pDirectory->ResolvedIcons[ pFile->fileID ];

			iw = icon.bmi.biWidth;
			ih = icon.bmi.biHeight;

			AspectRatio CAR = ScreenCompensatedIconRatio( AspectRatio( (WORD) iw, (WORD) ih), icon.Aspect, 64, 34 );

			rw = CAR.first;
			rh = CAR.second;

			hCreatedIcon = CreateBitmap( iw, ih, icon.bmi.biPlanes, icon.bmi.biBitCount, icon.pImage );

			hIcon = hCreatedIcon;
		}
	}
	
	if ( hIcon == NULL )
	{
		hIcon = BitmapCache.GetBitmap(Icon);
	}

	HGDIOBJ		hO	= SelectObject(hSourceDC, hIcon);

	DWORD FontID = FSPlugins.FindFont( pFile->EncodingID, PaneIndex );

	BYTE FullName[ 64 ];
	int  Truncs = 8;

	if ( Displaying == DisplayDetails )
	{
		Truncs = 28;
	}

	if ( Displaying == DisplayList )
	{
		Truncs = 14;
	}


	if ( rstrnlen( pFile->Filename, 64 ) > ( Truncs + 2 ) )
	{
		rstrncpy( FullName, pFile->Filename, Truncs );

		rstrncat( FullName, (BYTE *) "..", 64 );
	}
	else
	{
		rstrncpy( FullName, pFile->Filename, Truncs + 2 );
	}

	if ( pFile->Flags & FF_Extension )
	{
		rstrncat( FullName, (BYTE *) ".", 64 );
		rstrncat( FullName, pFile->Extension, 64 );
	}

	FontBitmap FileLabel( FontID, (BYTE *) FullName, rstrnlen( FullName, 64 ), false, Selected );

	int y = ((i / IconsPerLine) * ItemHeight ) - ScrollStartY;
	int x =  (i % IconsPerLine) * ItemWidth;

	DWORD rop = SRCCOPY;

	if ( Selected ) { rop = NOTSRCCOPY; }

	DWORD ix = 0;
	DWORD iy = 0;
	DWORD tiw = 34;
	DWORD tih = 34;

	if ( Displaying == DisplayLargeIcons )
	{
		int	xoffs = 0;
		int	xw    = 0;
		int	yoffs = 0;

		ix = x + 33 + xoffs;
		iy = y + 30 + yoffs;
	}

	if ( Displaying == DisplayDetails )
	{
		ix = x + 8U;
		iy = y + 30;
	}

	if ( Displaying == DisplayList )
	{
		ix  = x + 8U;
		iy  = y + 28U;
		tiw = 17;
		tih = 17;
		rw  = rw / 2;
		rh  = rh / 2;
	}

	if ( rw > tiw )
	{
		ix -= ( rw - tiw ) / 2;
	}
	else if ( rw < tiw )
	{
		ix += ( tiw - rw ) / 2;
	}

	if ( rh < tih )
	{
		iy += ( tih - rh ) / 2;
	}

	StretchBlt( viewDC, ix, iy, rw, rh, hSourceDC, 0, 0, iw, ih, rop );

	if ( Displaying == DisplayLargeIcons )
	{	
		FileLabel.DrawText( viewDC, x + 33U + 17U, y + 34U + 40U + 4U, DT_CENTER | DT_VCENTER );
	}
	else if ( Displaying == DisplayList )
	{
		FileLabel.DrawText( viewDC, x + 48U, y + 28U, DT_LEFT | DT_TOP );
	}

	if ( Displaying == DisplayDetails )
	{
		FileLabel.DrawText( viewDC, x + 48U, y + 30U, DT_LEFT | DT_TOP );

		if ( !(pFile->Flags & FF_Special) )
		{
			FontBitmap DetailString( FontID, FS->DescribeFile( pFile->fileID  ), 40, false, Selected );

			DetailString.DrawText( viewDC, x + 48U, y + 48U, DT_LEFT | DT_TOP );
		}
	}

	SelectObject(hSourceDC, hO);

	DeleteDC(hSourceDC);

	if ( ( Updated ) && ( SelectionStack.back() != -1 ) && ( i == SelectionStack.back() ) )
	{
		CalculatedY = y;
	}

	if ( hCreatedIcon != NULL )
	{
		DeleteObject( (HGDIOBJ) hCreatedIcon );
	}
}

void CFileViewer::Redraw() {
	HDC  hDC = GetDC(hWnd);
	RECT wndRect;

	GetCliRect( wndRect);

	if ((KnownX != (wndRect.right - wndRect.left)) || (KnownY != (wndRect.bottom - wndRect.top))) {
		if (viewBuffer) {
			SelectObject(viewDC, viewObj);

			DeleteObject(viewBuffer);
			DeleteDC(viewDC);
		}

		viewDC		= CreateCompatibleDC(hDC);
		viewBuffer	= CreateCompatibleBitmap(hDC, ( wndRect.right + 16 ) - wndRect.left, wndRect.bottom - ( wndRect.top - 24 ) );

		viewObj		= SelectObject(viewDC, viewBuffer);

		KnownX =   wndRect.right   -   wndRect.left;
		KnownY =   wndRect.bottom  - ( wndRect.top - 24 );
	}

	FillRect(viewDC, &wndRect, (HBRUSH) GetStockObject(WHITE_BRUSH));

	if ( (FS) && ( !IsSearching ) ) {
		std::vector<NativeFile>::iterator	iFile;

		DWORD exi = FS->pDirectory->Files.size(); // Extra increment

		if ( FS->FSID != FS_Root )
		{
			exi++;
		}

		if (Updated) {
			FileEntries		= FS->pDirectory->Files.size();
			FileSelections  = std::vector<bool>( FS->pDirectory->Files.size(), false );

			ScrollStartY	= 0;

			if ( SelectionStack.size() > 0 )
			{
				int sIndex = SelectionStack.back();

				if ( ( sIndex < (int) FileSelections.size() ) && ( sIndex >= 0 ) )
				{
					FileSelections[ sIndex ] = true;
				}
			}

			if ( exi > IconsPerWindow ) {
				EnableWindow(hScrollBar, TRUE);

				DWORD exl = exi / IconsPerLine;

				if ( ( exl * IconsPerLine ) < exi )
				{
					exl++;
				}

				int nwh = exl * ItemHeight;

				nwh += IconYOffset;

				int ps  = nwh / WindowHeight;

				nwh -= WindowHeight;

				SCROLLINFO si	= { sizeof(SCROLLINFO), SIF_ALL, 0, (nwh / 8), ps, 0, 0 };

				SetScrollInfo(hScrollBar, SB_CTL, &si, TRUE);

				ScrollMax = nwh;
			} else
				EnableWindow(hScrollBar, FALSE);
		}

		int	IconIndex = 1;
		int FileIndex = 0;
		
		long NewScrollStartY = ScrollStartY;

		if ( CurrentFSID == FS_Root )
		{
			IconIndex = 0;
		}
		else
		{
			BYTE ParentString[ 9 ] = { "X Parent" };

			ParentString[ 0 ] = 174;

			NativeFile Dummy;
			strncpy_s( (char *) Dummy.Filename, 16, (char *) ParentString, 8 );
			Dummy.Flags = FF_Special;
			Dummy.EncodingID = ENCODING_ASCII;

			DrawFile( 0, &Dummy, FT_Directory, ParentSelected);
		}

		for (iFile=FS->pDirectory->Files.begin(); iFile != FS->pDirectory->Files.end(); iFile++) {
			if ( iFile->fileID == 0U )
			{
				SendMessage( ParentWnd, WM_IDENTIFYFONT, (WPARAM) this, (LPARAM) FSPlugins.FindFont( iFile->EncodingID, PaneIndex ) );
			}


			// See that weird way of getting the NativeFile object? Yeah.. MSVC says iFile (iterator of NativeFile, and
			// thus yields a point to a NativeFile, as demonstrated above, can't be cast to NativeFile*......

			NativeFile file = *iFile;
			
			bool Selected =  false;

			if ( FileSelections.size() >= (DWORD) ( FileIndex + 1 ) )
			{
				Selected = FileSelections[ FileIndex ];
			}

			DrawFile( IconIndex, &file, file.Icon, Selected );

			IconIndex++;
			FileIndex++;
		}

		if ( ( Updated ) && ( SelectionStack.back() != -1 ) )
		{
			LastItemIndex = 0x7FFFFFFF;

			long ExHeight = 56;

			if ( Displaying == DisplayDetails ) { ExHeight = 64; }
			if ( Displaying == DisplayList ) { ExHeight = 30; }

			while ( ( ScrollStartY + WindowHeight ) < (CalculatedY + ExHeight ) )
			{
				ScrollStartY += 10;

				if ( ScrollStartY > ScrollMax )
				{
					ScrollStartY = ScrollMax;
				}
			}

			SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);
		}
	}

	Updated	= false;

	DrawBasicLayout();

	if ( ( Bounding ) && ( !IsSearching ) )
	{
		HGDIOBJ hOld = SelectObject( viewDC, Pens[ CurrentPen ] );

		MoveToEx( viewDC, dragX,  dragY, NULL );
		LineTo(   viewDC, dragX,  mouseY );
		LineTo(   viewDC, mouseX, mouseY );
		LineTo(   viewDC, mouseX, dragY );
		LineTo(   viewDC, dragX,  dragY );

		SelectObject( viewDC, hOld );

		CurrentPen = ( CurrentPen + 1 ) % 3;
	}

	BitBlt(hDC, 0,24,KnownX,KnownY, viewDC,0,24,SRCCOPY);
	BitBlt(hDC, 0,0,(KnownX - (ControlButtons.size() * 21)) + 1,24, viewDC, 0,0, SRCCOPY);

	ReleaseDC(hWnd, hDC);
}

void CFileViewer::RecalculateDimensions( RECT &wndRect )
{
	WindowWidth  = wndRect.right - wndRect.left;
	WindowHeight = wndRect.bottom - wndRect.top;

	switch (Displaying)
	{
	case DisplayLargeIcons:
		ItemWidth      = 100U;
		ItemHeight     = 64U;
		IconYOffset    = 6U;
		IconsPerLine   = WindowWidth / 100U;
		LinesPerWindow = ( WindowHeight - 6U ) / 64U;
		break;

	case DisplayDetails:
		ItemWidth      = 400U;
		ItemHeight     = 40U;
		IconYOffset    = 6U;
		IconsPerLine   = WindowWidth / 400U;
		LinesPerWindow = (WindowHeight - 6U) / 40U;
		break;

	case DisplayList:
		ItemWidth      = 200U;
		ItemHeight     = 20U;
		IconYOffset    = 4U;
		IconsPerLine   = WindowWidth / 200U;
		LinesPerWindow = (WindowHeight - 4U) / 20U;
		break;

	default:
		break;
	}

	IconsPerWindow  = IconsPerLine * LinesPerWindow;
}

DWORD CFileViewer::GetItem(DWORD x, DWORD y) {
	// Adjusted y position (accounting for scroll)
	DWORD ay = ( y + ScrollStartY - 30U);

	if ( x > ( ItemWidth * IconsPerLine ) )
	{
		// Outside of horizontally valid area
		return -2;
	}

	DWORD Index = ( (ay / ItemHeight) * IconsPerLine ) + ( x / ItemWidth );
	DWORD Index2 = Index;

	if ( FS->FSID != FS_Root )
	{
		Index2--;
	}


	if ( Index == 0U )
	{
		return 0U;
	}
	else
	{
		if ( Index2 < FS->pDirectory->Files.size() )
		{
			return Index;
		}
	}

	return -2;
}

void CFileViewer::ActivateItem(int x, int y) {
	int	ix	= GetItem(x,y);

	if ( ix != LastItemIndex )
	{
		LastClick = 0;
	}

	LastItemIndex = ix;

	DWORD Items = GetSelectionCount();

	if ( FS->FSID != FS_Root )
	{
		ix--;
	}

	if ( (ix == -1) && ( Items == 0 ) )
	{
		::SendMessage(ParentWnd, WM_GOTOPARENT, (WPARAM) hWnd, 0);
	}
	else if (ix >= 0)
	{
		DIndex = (WORD) ix;

		NativeFileIterator i;

		bool Translateable = false;

		for ( i = FS->pDirectory->Files.begin(); i != FS->pDirectory->Files.end(); i++ )
		{
			if ( ( FileSelections[ i->fileID ] ) || ( i->fileID == ix ) )
			{
				if ( i->XlatorID != NULL )
				{
					Translateable = true;
				}
			}
		}

		if ( Translateable )
		{
			DoContentViewer();

			DIndex = 0xFFFF;

			return;
		}

		SelectionStack.back() = ix;

		::SendMessage(ParentWnd, WM_ENTERICON, (WPARAM) hWnd, ix);
	}

	DIndex = 0xFFFF;
}

void CFileViewer::ClearItems( bool DoUpdate = true) {
	FileSelections.clear();

	if ( FS )
	{
		FileSelections = std::vector<bool>( FS->pDirectory->Files.size(), false );
	}

	ParentSelected	= false;

	if ( DoUpdate )
	{
		Update();
	}
}

void CFileViewer::ToggleItem(int x, int y) {
	static WCHAR blank[1]	= {NULL};

	if (!FS)
		return;

	DWORD ix = GetItem(x,y);

	if ( ( ix == 0 ) && ( FS->FSID != FS_Root ) )
	{
		ParentSelected	= !ParentSelected;
	}
	else
	{
		if ( FS->FSID != FS_Root )
		{
			ix--;
		}

		if ( ( ix > 0x80000000 ) || ( ix >= FS->pDirectory->Files.size() ) ) {
			ClearItems();

			return;
		}

		FileSelections[ix]	= !FileSelections[ix];
	}

	Update();
}

void CFileViewer::Update() {
	RECT rect;

	GetClientRect(hWnd, &rect);

	InvalidateRect(hWnd, &rect, FALSE);
}

void CFileViewer::StartDragging() {
	Dragging	= true;
}

void CFileViewer::EndDragging() {
	Dragging	= false;
	dragX		= -1;
	dragY		= -1;
}

void CFileViewer::PopulateFSMenus( HMENU hPopup )
{
	HMENU hSubMenu = GetSubMenu(hPopup, 0);	

	HMENU hProviderMenu = CreatePopupMenu();

	MENUITEMINFO mii = { 0 };
	mii.cbSize = sizeof( MENUITEMINFO );
	mii.fMask = MIIM_ID | MIIM_STRING | MIIM_SUBMENU ;
	mii.wID = 42999;
	mii.hSubMenu = hProviderMenu;
	mii.dwTypeData = _T( "Enter As" );

	InsertMenuItem( hSubMenu, (UINT) 1, TRUE, &mii );

	std::vector<FSMenu> menus = FSPlugins.GetFSMenu();
	std::vector<FSMenu>::iterator iter;

	UINT index = 43000;

	for (iter = menus.begin(); iter != menus.end(); iter++ )
	{
		HMENU hFormatMenu = CreatePopupMenu();

		mii.cbSize = sizeof( MENUITEMINFO );
		mii.fMask = MIIM_ID | MIIM_STRING | MIIM_SUBMENU ;
		mii.wID = index;
		mii.hSubMenu = hFormatMenu;
		mii.dwTypeData = (WCHAR *) iter->Provider.c_str();

		InsertMenuItem( hProviderMenu, (UINT) 1, TRUE, &mii );

		std::vector<FormatMenu>::iterator s;

		for ( s=iter->FS.begin(); s!=iter->FS.end(); s++ )
		{
			AppendMenu( hFormatMenu, MF_STRING, (UINT) index, s->FS.c_str() );

			MenuFSMap[ index ] = s->ID;

			index++;
		}
	}
	
}

void CFileViewer::PopulateXlatorMenus( HMENU hPopup )
{
	HMENU hSubMenu = GetSubMenu(hPopup, 0);	

	HMENU hProviderMenu = CreatePopupMenu();

	MENUITEMINFO mii = { 0 };
	mii.cbSize = sizeof( MENUITEMINFO );
	mii.fMask = MIIM_ID | MIIM_STRING | MIIM_SUBMENU ;
	mii.wID = 42998;
	mii.hSubMenu = hProviderMenu;
	mii.dwTypeData = _T( "View As" );

	InsertMenuItem( hSubMenu, (UINT) 9, TRUE, &mii );

	mii.cbSize = sizeof( MENUITEMINFO );
	mii.fMask = MIIM_ID | MIIM_FTYPE ;
	mii.wID = 42997;
	mii.hSubMenu = hProviderMenu;
	mii.fType = MFT_SEPARATOR;
	mii.dwTypeData = _T( "-" );

	InsertMenuItem( hSubMenu, (UINT) 10, TRUE, &mii );

	HMENU hGraphixMenu = CreatePopupMenu();

	mii.cbSize = sizeof( MENUITEMINFO );
	mii.fMask = MIIM_ID | MIIM_STRING | MIIM_SUBMENU ;
	mii.wID = 43500;
	mii.hSubMenu = hGraphixMenu;
	mii.dwTypeData = (WCHAR *) L"Graphic";

	InsertMenuItem( hProviderMenu, (UINT) 1, TRUE, &mii );


	HMENU hTextMenu = CreatePopupMenu();

	mii.cbSize = sizeof( MENUITEMINFO );
	mii.fMask = MIIM_ID | MIIM_STRING | MIIM_SUBMENU ;
	mii.wID = 43501;
	mii.hSubMenu = hTextMenu;
	mii.dwTypeData = (WCHAR *) L"Text";

	InsertMenuItem( hProviderMenu, (UINT) 2, TRUE, &mii );



	ProviderList Providers = FSPlugins.GetProviders();

	UINT GFXindex = GFX_MENU_BASE;
	UINT TXTindex = TXT_MENU_BASE;

	AppendMenu( hTextMenu, MF_STRING, (UINT) TXTindex, L"Text File" );

	MenuXlatorMap[ TXTindex ] = TUID_TEXT;

	TXTindex++;

	ProviderDesc_iter iter;

	for (iter = Providers.begin(); iter != Providers.end(); iter++ )
	{
		GraphicTranslatorList GFX = FSPlugins.GetGraphicTranslators( iter->PUID );

		GraphicTranslatorIterator gfx;

		for ( gfx = GFX.begin(); gfx != GFX.end(); gfx++ )
		{
			AppendMenu( hGraphixMenu, MF_STRING, (UINT) GFXindex, gfx->FriendlyName.c_str() );

			MenuXlatorMap[ GFXindex ] = gfx->TUID;

			GFXindex++;
		}

		TextTranslatorList TXT = FSPlugins.GetTextTranslators( iter->PUID );

		TextTranslatorIterator txt;

		for ( txt = TXT.begin(); txt != TXT.end(); txt++ )
		{
			AppendMenu( hTextMenu, MF_STRING, (UINT) TXTindex, txt->FriendlyName.c_str() );

			MenuXlatorMap[ TXTindex ] = txt->TUID;

			TXTindex++;
		}
	}
	
}
DWORD CFileViewer::GetSelectionCount( void )
{
	std::vector<bool>::iterator i;

	DWORD Result = 0;

	for ( i = FileSelections.begin(); i != FileSelections.end(); i++ )
	{
		if ( *i )
		{
			Result++;
		}
	}

	return Result;
}

std::vector<NativeFile> CFileViewer::GetSelection( void )
{
	std::vector<NativeFile> Selection;

	std::vector<bool>::iterator i;

	DWORD Result = 0xFFFFFFFF;
	DWORD Index  = 0;

	for ( i = FileSelections.begin(); i != FileSelections.end(); i++ )
	{
		if ( *i )
		{
			Selection.push_back( FS->pDirectory->Files[ Index ] );
		}

		Index++;
	}

	return Selection;
}

/* This returns the last selected item in the index.
   Obviously don't use this if multiple items are selected */
DWORD CFileViewer::GetSelectedIndex( void )
{
	std::vector<bool>::iterator i;

	DWORD Result = 0xFFFFFFFF;
	DWORD Index  = 0;

	for ( i = FileSelections.begin(); i != FileSelections.end(); i++ )
	{
		if ( *i )
		{
			Result = Index;
		}

		Index++;
	}

	return Result;
}

void CFileViewer::DoSelections( UINT Msg, WPARAM wParam, LPARAM lParam )
{
	static BYTE multi[ 64 ];

	if ( Msg == WM_LBUTTONUP )
	{
		OutputDebugStringA( "LBUTTONUP\n" );
		KillTimer( hWnd, 0xB011D );

		SetFocus(hWnd);

		if (Updated)
			return;

		mouseX	= GET_X_LPARAM( lParam );
		mouseY	= GET_Y_LPARAM( lParam );

		if ( ( !Dragging ) && ( !Bounding ) ) {
			OutputDebugStringA( "Not dragging it " );
			int	ix	= GetItem(mouseX, mouseY);

			if (!ShiftPressed())
			{
				ClearItems();
			}

			char e[256];
			sprintf(e, "ix = %d, pix = %d\n", ix, LastItemIndex );
			OutputDebugStringA( e );

			if ( ( CheckClick() ) || ( ix != LastItemIndex ) )
			{
				OutputDebugStringA( "Toggling it\n" );
				ToggleItem(mouseX, mouseY);

				LastItemIndex = ix;
			}
			else
			{
				OutputDebugStringA( "Activating it\n" );
				ActivateItem(mouseX, mouseY);
			}
		}

		if (Dragging) {
			OutputDebugStringA( "Dragging it " );
			SetCursor(LoadCursor(NULL, IDC_ARROW));

			SendMessage(ParentWnd, WM_FVENDDRAG, 0, 0);
		}

		if ( Bounding )
		{
			Bounding = false;

			ClearItems();

			DWORD xs = dragX;
			DWORD xe = mouseX;

			if ( xe < xs ) { xe = dragX; xs = mouseX; }

			DWORD ys = dragY;
			DWORD ye = mouseY;

			if ( ye < ys ) { ye = dragY; ys = mouseY; }


			for ( DWORD xi = xs; xi < xe; xi += ItemWidth )
			{
				for ( DWORD yi = ys; yi < ye; yi += ItemHeight )
				{
					DWORD ii = GetItem( xi, yi );

					if ( ( FS->FSID != FS_Root ) && ( ii > 0 ) ) { ii--; }

					if ( ( ii < FileSelections.size() ) && ( ii >= 0 ) )
					{
						FileSelections[ ii ] = true;
					}
				}
			}

			Redraw();
		}

		DWORD Selected = GetSelectionCount();

		switch ( Selected ) {
		case 0:
			{
				rsprintf( multi, "%d File System Objects", FS->pDirectory->Files.size() );

				::SendMessage( ParentWnd, WM_UPDATESTATUS, (WPARAM) this, (LPARAM) multi);
			}
			break;

		case 1:
			::SendMessage( ParentWnd, WM_UPDATESTATUS, (WPARAM) this, (LPARAM) FS->GetStatusString( GetSelectedIndex() ) );
			break;

		default:
			{
				rsprintf( multi, "%d Files selected", Selected );
					
				::SendMessage( ParentWnd, WM_UPDATESTATUS, (WPARAM) this, (LPARAM) multi );
			}

			break;
		}

		dragX	= -1;
		dragY	= -1;

		MouseDown = false;
		Dragging  = false;
	}

	if ( Msg == WM_MOUSEMOVE )
	{
		mouseX	= GET_X_LPARAM(lParam);
		mouseY	= GET_Y_LPARAM(lParam);

		if ((dragX != -1) && (dragY != -1))
		{
			if (( dwDiff(mouseX, dragX) > 16) || ( dwDiff(mouseY, dragY) > 16))
			{
				if ( SelectionAtMouseDown )
				{
					Dragging = true;
				}
				else
				{
					if ( !Bounding )
					{
						TRACKMOUSEEVENT tme;

						tme.cbSize      = sizeof( TRACKMOUSEEVENT );
						tme.dwFlags     = TME_LEAVE;
						tme.dwHoverTime = HOVER_DEFAULT;
						tme.hwndTrack   = hWnd;

						TrackMouseEvent( &tme );

						OutputDebugStringA( "Bounding leave detect start!\n" );

						SetTimer( hWnd, 0xB011D, 80, NULL );
					}

					Bounding = true;
				}
			}
		}

		if ( ( mouseY < 24 ) || ( mouseX > ( WindowWidth + 8 ) ) )
		{
			// Bounding in the title bar or scroll bar? tut tut.
			PostMessage( hWnd, WM_MOUSELEAVE, 0, 0 );
		}
	
		if (Dragging)
		{
			SendMessage(ParentWnd, WM_FVSTARTDRAG, 0, 0);
			SetCursor(LoadCursor(hInst, MAKEINTRESOURCE(IDI_SINGLEFILE)));
		}
		else
		{
			SetCursor(LoadCursor(NULL, IDC_ARROW));
		}

		if ( Bounding )
		{
			Redraw();
		}
	}

	if ( Msg == WM_LBUTTONDOWN )
	{
		dragX	= GET_X_LPARAM(lParam);
		dragY	= GET_Y_LPARAM(lParam);

		DWORD n = GetSelectedIndex();
		DWORD i = GetItem( dragX, dragY );

		if ( FS->FSID != FS_Root )
		{
			i--;
		}

		DWORD sc = GetSelectionCount();

		if ( sc == 1 )
		{
			if ( n == i )
			{
				SelectionAtMouseDown = true;
			}
			else
			{
				SelectionAtMouseDown = false;
			}
		}
		else if ( sc > 1 )
		{
			SelectionAtMouseDown = true;
		}
		else
		{
			SelectionAtMouseDown = false;
		}

		MouseDown = true;
	}

	/* This message may not exist */
	if ( Msg == WM_LBUTTONDBLCLK )
	{
		/* Double-clicking a multi-section makes no sense */
		if ( !ShiftPressed() )
		{
			for (int i=0; i<FileEntries; i++)
				FileSelections[i]	= false;

			Update();

			ActivateItem(mouseX, mouseY);
		}
	}
}

void CFileViewer::SetTitleStack( std::vector<TitleComponent> NewTitleStack )
{
	TitleStack = NewTitleStack;

	Redraw();
}

std::vector<TitleComponent> CFileViewer::GetTitleStack( void )
{
	return TitleStack;
}

void CFileViewer::DoSCREENContentViewer( DWORD PrefTUID )
{
	RECT rect;
	NativeFileIterator iter;

	std::vector<NativeFile> Selection = GetSelection();

	if ( DIndex != 0xFFFF )
	{
		Selection.push_back( FS->pDirectory->Files[ DIndex ] );
	}

	int wInd = 1;

	for ( iter = Selection.begin(); iter != Selection.end(); iter++ )
	{
		CTempFile FileObj;

		FS->ReadFile( iter->fileID, FileObj );

		CSCREENContentViewer *pSCViewer;

		if ( PrefTUID == NULL )
		{
			pSCViewer = new CSCREENContentViewer( FileObj, iter->XlatorID );
		}
		else
		{
			pSCViewer = new CSCREENContentViewer( FileObj, PrefTUID );
		}

		GetWindowRect(ParentWnd, &rect);

		pSCViewer->Create( hWnd, hInst, rect.left + (32 * wInd), rect.top + ( 32 * wInd ), 500 );

		wInd++;
	}
}

void CFileViewer::DoTEXTContentViewer( DWORD PrefTUID )
{
	RECT rect;
	NativeFileIterator iter;

	std::vector<NativeFile> Selection = GetSelection();

	if ( DIndex != 0xFFFF )
	{
		Selection.push_back( FS->pDirectory->Files[ DIndex ] );
	}

	int wInd = 1;

	for ( iter = Selection.begin(); iter != Selection.end(); iter++ )
	{
		CTempFile FileObj;

		FS->ReadFile( iter->fileID, FileObj );

		CTEXTContentViewer *pTXViewer;

		if ( PrefTUID == NULL )
		{
			pTXViewer = new CTEXTContentViewer( FileObj, iter->XlatorID );
		}
		else
		{
			pTXViewer = new CTEXTContentViewer( FileObj, PrefTUID );
		}

		pTXViewer->FSEncodingID = FS->GetEncoding();

		GetWindowRect(ParentWnd, &rect);

		pTXViewer->Create( hWnd, hInst, rect.left + (32 * wInd), rect.top + ( 32 * wInd ), 500 );

		wInd++;
	}
}

void CFileViewer::DoContentViewer( void )
{
	RECT rect;
	NativeFileIterator iter;

	std::vector<NativeFile> Selection = GetSelection();

	if ( DIndex != 0xFFFF )
	{
		Selection.push_back( FS->pDirectory->Files[ DIndex ] );
	}

	TextTranslatorList    TXTList = FSPlugins.GetTextTranslators( NULL );
	GraphicTranslatorList GFXList = FSPlugins.GetGraphicTranslators( NULL );

	GetWindowRect(ParentWnd, &rect);

	int wInd = 1;

	for ( iter = Selection.begin(); iter != Selection.end(); iter++ )
	{
		CTempFile FileObj;

		FS->ReadFile( iter->fileID, FileObj );

		TextTranslatorIterator iText;

		if ( iter->XlatorID == TUID_TEXT )
		{
			CTEXTContentViewer *pTXViewer;

			pTXViewer = new CTEXTContentViewer( FileObj, iter->XlatorID );

			pTXViewer->FSEncodingID = FS->GetEncoding();

			pTXViewer->Create( hWnd, hInst, rect.left + (32 * wInd), rect.top + ( 32 * wInd ), 500 );

			wInd++;
		}

		for ( iText = TXTList.begin(); iText != TXTList.end(); iText++ )
		{
			if ( iter->XlatorID == iText->TUID )
			{
				CTEXTContentViewer *pTXViewer;

				pTXViewer = new CTEXTContentViewer( FileObj, iter->XlatorID );

				pTXViewer->FSEncodingID = FS->GetEncoding();

				pTXViewer->Create( hWnd, hInst, rect.left + (32 * wInd), rect.top + ( 32 * wInd ), 500 );

				wInd++;
			}
		}

		GraphicTranslatorIterator iGFX;

		for ( iGFX = GFXList.begin(); iGFX != GFXList.end(); iGFX++ )
		{
			if ( iter->XlatorID == iGFX->TUID )
			{
				CSCREENContentViewer *pSCViewer;

				pSCViewer = new CSCREENContentViewer( FileObj, iter->XlatorID );

				pSCViewer->Create( hWnd, hInst, rect.left + (32 * wInd), rect.top + ( 32 * wInd ), 500 );

				wInd++;
			}
		}
	}
}

void CFileViewer::RenameFile( void )
{
	NativeFileIterator iter;

	std::vector<NativeFile> Selection = GetSelection();

	for ( iter = Selection.begin(); iter != Selection.end(); iter++ )
	{
		BYTE OldName[ 256 ];

		rstrncpy( OldName, iter->Filename, 256 );

		CFileViewer::pRenameFile = (BYTE *) FS->GetEncoding();

		if ( DialogBoxParam( hInst, MAKEINTRESOURCE(IDD_RENAME), hWnd, RenameDialogProc, (LPARAM) OldName ) == IDOK )
		{
			FS->Rename( iter->fileID, OldName );
		}
	}

	Updated = true;

	Redraw();
}

BYTE *CFileViewer::pRenameFile         = nullptr;
EncodingEdit *CFileViewer::pRenameEdit = nullptr;

INT_PTR CALLBACK CFileViewer::RenameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch ( message )
	{
	case WM_INITDIALOG:
		{
			pRenameEdit = new EncodingEdit( hDlg, 12, 36, 455, true );

			pRenameEdit->Encoding = (DWORD) CFileViewer::pRenameFile;

			CFileViewer::pRenameFile = (BYTE *) lParam;

			pRenameEdit->SetText( (BYTE *) lParam );
			pRenameEdit->SelectAll();
			pRenameEdit->SetFocus();
		}
		break;

	case WM_COMMAND:
		{
			if ( LOWORD( wParam ) == IDOK )
			{
				rstrncpy(  CFileViewer::pRenameFile, pRenameEdit->GetText(), 256 );

				EndDialog( hDlg, IDOK );
			}

			if ( LOWORD( wParam ) == IDCANCEL )
			{
				EndDialog( hDlg, IDCANCEL );
			}
		}
		break;
	}

	return FALSE;
}

void CFileViewer::DoSwapFiles( BYTE UpDown )
{
	if ( GetSelectionCount() != 1 )
	{
		return;
	}

	DWORD SwapFile = GetSelectedIndex();
	DWORD MaxFile  = FS->pDirectory->Files.size();

	// The condition above should nix this, but still..
	if ( MaxFile == 0 )
	{
		return;
	}

	MaxFile--;

	if ( UpDown == 0xFF )
	{
		if ( SwapFile > 0 )
		{
			FS->SwapFile( SwapFile - 1, SwapFile );
		}
	}
	else
	{
		if ( SwapFile < MaxFile )
		{
			FS->SwapFile( SwapFile + 1, SwapFile );
		}
	}
}

void CFileViewer::SetSearching( bool s )
{
	IsSearching = s;

	if ( s )
	{
		ShowWindow( hSearchAVI, SW_SHOW );
	}
	else
	{
		ShowWindow( hSearchAVI, SW_HIDE );
	}
}


void CFileViewer::DoKeyControls( UINT message, WPARAM wParam, LPARAM lParam )
{
	if ( ( IsSearching ) || ( !HasFocus ) )
	{
		return;
	}

	if ( FileSelections.size() == 0 )
	{
		ParentSelected = true;

		return;
	}

	int keyCode = (int) wParam;

	DWORD sc = GetSelectionCount();

	switch ( keyCode )
	{
	case VK_LEFT:
		{
			if ( sc == 0 )
			{
				ParentSelected = true;
			}
			else
			{
				if ( ( sc == 1 ) && ( GetSelectedIndex() == 0 ) && ( FS->FSID != FS_Root ) )
				{
					ClearItems( false );
				}
				else
				{
					DWORD si = 0xFFFFFFFF;

					std::vector<bool>::iterator iS;
					DWORD fi = 0;

					for ( iS = FileSelections.begin(); iS != FileSelections.end(); iS++ )
					{
						if ( ( *iS ) && ( fi < si ) )
						{
							si = fi;
						}
						fi++;
					}

					if ( !ShiftPressed() )
					{
						for ( iS = FileSelections.begin(); iS != FileSelections.end(); iS++ )
						{
							*iS = false;
						}
					}

					if ( si > 0 )
					{
						si--;
					}

					FileSelections[ si ] = true;
				}
			}
		}

		break;

	case VK_RIGHT:
		{
			DWORD si = 0x0000;

			std::vector<bool>::iterator iS;
			DWORD fi = 0;

			for ( iS = FileSelections.begin(); iS != FileSelections.end(); iS++ )
			{
				if ( ( *iS ) && ( fi > si ) )
				{
					si = fi;
				}
				fi++;
			}

			if ( !ShiftPressed() )
			{
				ClearItems( false );
			}

			if ( si < FileSelections.size() - 1 )
			{
				si++;
			}

			if ( sc == 0 )
			{
				si = 0;
			}

			FileSelections[ si ] = true;

			ParentSelected = false;
		}

		break;

	case VK_UP:
		{
			if ( sc == 0 )
			{
				ParentSelected = true;
			}
			else
			{
				int   si = 0x7FFFFFFF;
				DWORD ei = 0x00000000;

				std::vector<bool>::iterator iS;
				DWORD fi = 0;

				for ( iS = FileSelections.begin(); iS != FileSelections.end(); iS++ )
				{
					if ( ( *iS ) && ( fi < si ) )
					{
						si = fi;
					}
					if ( ( *iS ) && ( fi > ei ) )
					{
						ei = fi;
					}
					fi++;
				}

				if ( !ShiftPressed() )
				{
					ClearItems( false );
				}

				if ( (si - (int) IconsPerLine) > 0 )
				{
					si -= IconsPerLine;

					if ( !ShiftPressed() )
					{
						ei -= IconsPerLine;
					}
				}
				else
				{
					si = 0;

					if ( !ShiftPressed() )
					{
						ei = 0;
					}
				}

				fi = 0;
				for ( iS = FileSelections.begin(); iS != FileSelections.end(); iS++ )
				{
					if ( ( fi >= si ) && ( fi <= ei ) )
					{
						*iS = true;
					}
					fi++;
				}
			}
		}

		break;

	case VK_DOWN:
		{
			if ( sc == 0 )
			{
				ParentSelected = true;
			}
			else
			{
				int   si = 0x7FFFFFFF;
				DWORD ei = 0x00000000;

				std::vector<bool>::iterator iS;
				DWORD fi = 0;

				for ( iS = FileSelections.begin(); iS != FileSelections.end(); iS++ )
				{
					if ( ( *iS ) && ( fi < si ) )
					{
						si = fi;
					}
					if ( ( *iS ) && ( fi > ei ) )
					{
						ei = fi;
					}
					fi++;
				}

				if ( !ShiftPressed() )
				{
					ClearItems( false );
				}

				if ( (si + (int) IconsPerLine) < FileSelections.size() - 1 )
				{
					ei += IconsPerLine;

					if ( !ShiftPressed() )
					{
						si += IconsPerLine;
					}
				}
				else
				{
					ei = FileSelections.size() - 1;

					if ( !ShiftPressed() )
					{
						si = FileSelections.size() - 1;
					}
				}

				fi = 0;
				for ( iS = FileSelections.begin(); iS != FileSelections.end(); iS++ )
				{
					if ( ( fi >= si ) && ( fi <= ei ) )
					{
						*iS = true;
					}
					fi++;
				}
			}
		}

		break;

	case VK_RETURN:
		{
			DWORD si = GetSelectedIndex();

			ActivateItem( ( ( si % IconsPerLine ) * ItemWidth ) + ( ItemWidth / 2 ), ( (si / IconsPerLine ) * ItemHeight ) + (ItemHeight / 2 ) );
		}
		break;

	case VK_APPS:
		DoContextMenu();
		break;
	}

	if ( !ParentSelected )
	{
		::SendMessage( ParentWnd, WM_UPDATESTATUS, (WPARAM) this, (LPARAM) FS->GetStatusString( GetSelectedIndex() ) );
	}

	Update();
}

void CFileViewer::DoContextMenu( void )
{
	RECT rect;
	DWORD Selected = GetSelectionCount();

	HMENU hPopup;
	HMENU hSubMenu;

	if ( CurrentFSID == FS_Root )
	{
		hPopup	= LoadMenu(hInst, MAKEINTRESOURCE(IDR_ROOT_POPUP));
	}
	else
	{
		hPopup	= LoadMenu(hInst, MAKEINTRESOURCE(IDR_POPUP));
	}

	GetWindowRect(hWnd, &rect);

	if ( Selected == 1 )
	{
		PopulateFSMenus(hPopup);
	}

	hSubMenu	= GetSubMenu(hPopup, 0);

	if ( Selected == 0 )
	{
		DeleteMenu(hSubMenu, IDM_FORMAT, MF_BYCOMMAND);
		DeleteMenu(hSubMenu, IDM_ENTER, MF_BYCOMMAND);
	}

	if ( Displaying == DisplayLargeIcons ) { CheckMenuItem( hSubMenu, IDM_LARGEICONS, MF_BYCOMMAND | MF_CHECKED ); } else { CheckMenuItem( hSubMenu, IDM_LARGEICONS, MF_BYCOMMAND | MF_UNCHECKED ); }
	if ( Displaying == DisplayDetails )    { CheckMenuItem( hSubMenu, IDM_DETAILS, MF_BYCOMMAND | MF_CHECKED );    } else { CheckMenuItem( hSubMenu, IDM_DETAILS, MF_BYCOMMAND | MF_UNCHECKED ); }
	if ( Displaying == DisplayList )       { CheckMenuItem( hSubMenu, IDM_FILELIST, MF_BYCOMMAND | MF_CHECKED );   } else { CheckMenuItem( hSubMenu, IDM_FILELIST, MF_BYCOMMAND | MF_UNCHECKED ); }

	if ( CurrentFSID != FS_Root)
	{
		if ( Selected == 1 )
		{
			PopulateXlatorMenus(hPopup);
		}
		else if ( Selected == 0 )
		{
			EnableMenuItem( hPopup, IDM_ENTER,     MF_BYCOMMAND | MF_DISABLED );
			EnableMenuItem( hPopup, IDM_COPY,      MF_BYCOMMAND | MF_DISABLED );
			EnableMenuItem( hPopup, IDM_TRANSLATE, MF_BYCOMMAND | MF_DISABLED );
		}
	}

	TrackPopupMenu(hSubMenu, 0, rect.left + mouseX, rect.top + mouseY, 0, hWnd, NULL);

	DestroyMenu(hPopup);
}