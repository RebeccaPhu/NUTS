#include "StdAfx.h"
#include "FileViewer.h"
#include "FormatWizard.h"
#include "FontBitmap.h"
#include "Plugins.h"
#include "libfuncs.h"
#include "IconRatio.h"
#include "AudioPlayer.h"

#include <CommCtrl.h>

#include <WindowsX.h>

std::map<HWND, CFileViewer *> CFileViewer::viewers;

bool CFileViewer::WndClassReg = false;

#define ProgW 128
#define ProgH 18

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

	InitializeCriticalSection( &CacheLock );

	KnownX	= 0;
	KnownY	= 0;

	viewBuffer	= NULL;
	viewDC		= NULL;
	hSourceDC   = NULL;

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
	FileLabels.clear();
	FileDescs.clear();
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

	pDropSite = nullptr;
}

CFileViewer::~CFileViewer(void) {
	NixWindow( hProgress );
	NixWindow( hScrollBar );
	NixWindow( hWnd );

	if ( hViewerBrush != NULL )
	{
		DeleteObject( (HGDIOBJ) hViewerBrush );
	}

	if (viewBuffer) {
		SelectObject(viewDC, viewObj);

		DeleteObject(viewBuffer);
		DeleteDC(viewDC);
	}

	if ( pDropSite != nullptr )
	{
		pDropSite->Revoke();

		/* This deletes itself when the ref count goes to zero */
		pDropSite->Release();
	}

	FreeLabels();

	DeleteCriticalSection( &CacheLock );
}

int CFileViewer::Create(HWND Parent, HINSTANCE hInstance, int x, int w, int h) {
	titleBarFont	= CreateFont(16,6,0,0,FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"MS Shell Dlg");
	filenameFont	= CreateFont(12,5,0,0,FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"MS Shell Dlg");

	hWnd = CreateWindowEx(WS_EX_CONTROLPARENT, L"NUTS File Browser Pane", L"", SS_BLACKFRAME|SS_NOTIFY|SS_OWNERDRAW|WS_CHILD|WS_VISIBLE|WS_TABSTOP, x, 0, w, h, Parent, NULL, hInstance, NULL);
	//SS_WHITERECT|

	viewers[ hWnd ] = this;

	ParentWnd	= Parent;

	pDropSite = new DropSite( hWnd );

	int tw = w - 140;

	hScrollBar	= CreateWindowEx(NULL, L"SCROLLBAR", L"", WS_CHILD|WS_VISIBLE|SBS_VERT|WS_DISABLED, w - 24, 24, 16, h, hWnd, NULL, hInst, NULL);

	hProgress = CreateWindowEx(
		0, PROGRESS_CLASS, NULL,
		WS_CHILD | PBS_MARQUEE,
		0, 0, ProgW, ProgH,
		hWnd, NULL, hInst, NULL
	);
	
	::PostMessage( hProgress, PBM_SETMARQUEE, (WPARAM) TRUE, 0 );

	DoStatusBar();

	SideBar.Create( hWnd, hInst );

	Refresh();

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

		Refresh();
	}

	if (LOWORD(wParam) == SB_LINEUP) {
		ScrollStartY -= 8;

		if (ScrollStartY < 0)
			ScrollStartY = 0;

		SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

		Refresh();
	}

	if (LOWORD(wParam) == SB_PAGEUP) {
		ScrollStartY -= 64;

		if (ScrollStartY < 0)
			ScrollStartY = 0;

		SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

		Refresh();
	}

//	GetScrollRange(hScrollBar, SB_CTL, &min, &max);

	if (LOWORD(wParam) == SB_LINEDOWN) {
		ScrollStartY += 8;

		if (ScrollStartY > ScrollMax)
			ScrollStartY = ScrollMax;

		SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

		Refresh();
	}

	if (LOWORD(wParam) == SB_PAGEDOWN) {
		ScrollStartY += 64;

		if (ScrollStartY > ScrollMax)
			ScrollStartY = ScrollMax;

		SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

		Refresh();
	}

	if (LOWORD(wParam) == SB_ENDSCROLL) {
		SetScrollPos(hScrollBar, SB_CTL, ScrollStartY / 8, TRUE);

		Refresh();
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

			if ( LOWORD(wParam) == IDM_PLAY_AUDIO )
			{
				DoPlayAudio();
			}

			if ( ( LOWORD(wParam) > LC_MENU_BASE ) && ( LOWORD(wParam) <= LC_MENU_END ) )
			{
				std::vector<NativeFile> Selection = GetSelection();

				int r = FS->ExecLocalCommand( LOWORD(wParam) - LC_MENU_BASE - 1, Selection, hWnd );

				if ( r == CMDOP_REFRESH )
				{
					::PostMessage( hWnd, WM_COMMAND, IDM_REFRESH, 0 );
				}
				else if ( r < 0 )
				{
					NUTSError::Report( L"Local command", hWnd );
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
				case IDM_MOVEUP:
					{
						DoSwapFiles( 0x00 );

						FreeLabels();

						Redraw();
					}
					break;

				case IDM_MOVEDOWN:
					{
						DoSwapFiles( 0xFF );

						FreeLabels();

						Redraw();
					}
					break;

				case IDM_ROOTFS:
					{
						SendMessage( ParentWnd, WM_ROOTFS, (WPARAM) hWnd, (LPARAM) NULL );
					}
					break;

				case IDM_FONTSW:
					{
						FreeLabels();

						FSPlugins.NextFont( FS->GetEncoding(), PaneIndex );

						Redraw();
					}
					break;

				case IDM_REFRESH:
					{
						SendMessage( ParentWnd, WM_REFRESH_PANE, (WPARAM) hWnd, (LPARAM) NULL );						
					}
					break;

				case IDM_PARENT:
					{
						::SendMessage(ParentWnd, WM_GOTOPARENT, (WPARAM) hWnd, 0);
					}
					break;

				case IDM_SELECTALL:
					{
						std::vector<bool>::iterator iSelect;

						for ( iSelect = FileSelections.begin(); iSelect != FileSelections.end(); iSelect ++ )
						{
							*iSelect = true;
						}

						ParentSelected = false;

						Refresh();
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
						if ( GetSelectionCount() == 0 ) { break; }

						::PostMessage(ParentWnd, WM_COPYOBJECT, (WPARAM) hWnd, 0 );
					}

					break;

				case IDM_INSTALL:
					{
						if ( GetSelectionCount() == 0 ) { break; }

						::PostMessage(ParentWnd, WM_INSTALLOBJECT, (WPARAM) hWnd, 0 );
					}

					break;

				case IDM_DELETE:
					{
						if ( GetSelectionCount() == 0 ) { break; }

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
						if ( GetSelectionCount() == 0 ) { break; }

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
					if ( GetSelectionCount() == 0 ) { break; }

					ActivateItem(mouseX, mouseY);

					break;

				case IDM_RENAME:
					{
						if ( GetSelectionCount() == 0 ) { break; }

						::SendMessage( ParentWnd, WM_RENAME_FILE, (WPARAM) hWnd, 0 );
					}
					break;

				case IDM_NEWFOLDER:
					{
						::SendMessage( ParentWnd, WM_NEW_DIR, (WPARAM) hWnd, 0 );
					}
					break;

				case IDM_LARGEICONS:
					Displaying = DisplayLargeIcons;

					Update();
					break;

				case IDM_DETAILS:
					Displaying = DisplayDetails;

					Update();
					break;
				case IDM_FILELIST:
					Displaying = DisplayList;

					Update();
					break;
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
						return DefWindowProc(hSourceWnd, message, wParam, lParam);

					case VK_RETURN:
						return DLGC_STATIC;
			
					case VK_LEFT:
					case VK_RIGHT:
					case VK_HOME:
					case VK_END:
					case VK_UP:
					case VK_DOWN:
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
			if ( !FSChangeLock )
			{
				Redraw();
			}

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

			Refresh();

			break;

		case WM_EXTERNALDROP:
			/* Pass directly to MainWnd */
			::PostMessage( ParentWnd, WM_EXTERNALDROP, wParam, lParam );
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

	SetWindowPos( hProgress, NULL, ((ThisRect.right - ThisRect.left) / 2) - ( ProgW / 2 ), ((ThisRect.bottom - ThisRect.top) / 2) - ( ProgH / 2), ProgW, ProgH, SWP_NOZORDER | SWP_NOREPOSITION );

	SideBar.Resize();
}

void CFileViewer::DrawBasicLayout() {
	RECT	rect;

	GetClientRect(hWnd, &rect);

	rect.top	+= 22;

	rect.right -= 15;

	rect.left += 128;

	DrawEdge(viewDC, &rect, EDGE_SUNKEN, BF_RECT);

	/* You put your offsets in, your offsets out... */
	rect.right += 15;
	rect.right -= 24;

	rect.top	-= 22;

	DWORD BColour = GetSysColor(COLOR_BTNFACE);

	if ( hViewerBrush == NULL )
	{
		hViewerBrush = CreateSolidBrush( BColour );
	}

	rect.left -= 128;

	rect.bottom	= rect.top + 24;

	FillRect(viewDC, &rect, hViewerBrush);

	DrawEdge(viewDC, &rect, EDGE_RAISED, BF_RECT);

	if ( IsSearching )
	{
		FontBitmap TitleString( FONTID_PC437, (BYTE *) "Loading...", (BYTE) ( ( rect.right - rect.left ) / 8 ), false, false );

		TitleString.SetButtonColor( GetRValue( BColour ), GetGValue( BColour ), GetBValue( BColour ) );

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
		WORD l  = rstrnlen( iStack->String, 255 );
		WORD ml = (BYTE) ( ( rect.right - rect.left ) / 8 );

		FontBitmap TitleString( FSPlugins.FindFont( iStack->Encoding, PaneIndex ), iStack->String, (BYTE) l, false, false );

		TitleString.SetButtonColor( GetRValue( BColour ), GetGValue( BColour ), GetBValue( BColour ) );

		TitleString.SetGrayed( !HasFocus );

		TitleString.DrawText( viewDC, 6 + coffset, 4, DT_TOP | DT_LEFT );

		coffset += ( strlen( (char *) iStack->String ) * 8 );
	}
}

void CFileViewer::DrawFile(int i, NativeFile *pFile, DWORD Icon, bool Selected) {
	if ( hSourceDC == NULL )
	{
		hSourceDC = CreateCompatibleDC(viewDC);
	}

	if ( IconsPerLine == 0 ) { return; }
	if ( LinesPerWindow == 0 ) { return; }

	HBITMAP hIcon        = NULL;
	HBITMAP hCreatedIcon = NULL;

	DWORD      iw = 34;
	DWORD      ih = 34;
	DWORD      rw = 34;
	DWORD      rh = 34;

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

	int y = ((i / IconsPerLine) * ItemHeight ) - ScrollStartY;
	int x =  (i % IconsPerLine) * ItemWidth;

	x += 128;

	DWORD rop = SRCCOPY;

	if ( Selected ) { rop = NOTSRCCOPY; }

	int   ix  = 0;
	int   iy  = 0;
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

	int TopLimit = 0 - rh;
	int BottomLimit = ( LinesPerWindow + 1 ) * ItemHeight;

	if ( ( iy >= TopLimit ) && ( iy <= BottomLimit ) )
	{
		if ( pFile->HasResolvedIcon )
		{
			if ( TheseIcons.find( pFile->fileID ) != TheseIcons.end() )
			{
				IconDef icon = TheseIcons[ pFile->fileID ];

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

		StretchBlt( viewDC, ix, iy, rw, rh, hSourceDC, 0, 0, iw, ih, rop );

		FontBitmap *pFileLabel;
	
		pFileLabel = FileLabels[ pFile->fileID ];

		if ( pFileLabel == nullptr )
		{
			pFileLabel = new FontBitmap( FontID, (BYTE *) FullName, rstrnlen( FullName, 64 ), false, Selected );
		}

		FileLabels[ pFile->fileID ] = pFileLabel;

		if ( Displaying == DisplayLargeIcons )
		{	
			pFileLabel->DrawText( viewDC, x + 33U + 17U, y + 34U + 40U + 4U, DT_CENTER | DT_VCENTER );
		}
		else if ( Displaying == DisplayList )
		{
			pFileLabel->DrawText( viewDC, x + 48U, y + 28U, DT_LEFT | DT_TOP );
		}

		if ( Displaying == DisplayDetails )
		{
			pFileLabel->DrawText( viewDC, x + 48U, y + 30U, DT_LEFT | DT_TOP );

			FontBitmap *pDetailString = FileDescs[ pFile->fileID ];

			if ( pDetailString == nullptr )
			{
				pDetailString = new FontBitmap( FontID, FS->DescribeFile( pFile->fileID  ), 40, false, Selected );

				FileDescs[ pFile->fileID ] = pDetailString;
			}

			pDetailString->DrawText( viewDC, x + 48U, y + 48U, DT_LEFT | DT_TOP );
		}

		SelectObject(hSourceDC, hO);
	}

	if ( SelectionStack.size() > 0 )
	{
		if ( ( Updated ) && ( SelectionStack.back() != -1 ) && ( i == SelectionStack.back() ) )
		{
			CalculatedY = y;
		}
	}

	if ( hCreatedIcon != NULL )
	{
		DeleteObject( (HGDIOBJ) hCreatedIcon );
	}
}

void CFileViewer::Refresh()
{
	InvalidateRect( hWnd, NULL, FALSE );

	::SendMessageA( hWnd, WM_PAINT, NULL, NULL );
}

void CFileViewer::Redraw() {
	HDC  hDC = GetDC(hWnd);
	RECT wndRect;

	EnterCriticalSection( &CacheLock );

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

		DWORD exi = TheseFiles.size(); // Extra increment

		if ( FS->FSID != FS_Root )
		{
			exi++;
		}

		if (Updated) {
			FreeLabels();


			RECT DimRect;

			GetCliRect( DimRect );
			RecalculateDimensions( DimRect );

			ScrollStartY   = 0;

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
			}
			else
			{
				ScrollMax = LinesPerWindow * ItemHeight;

				EnableWindow(hScrollBar, FALSE);
			}

			DoStatusBar();
		}

		long NewScrollStartY = ScrollStartY;

		SendMessage( ParentWnd, WM_IDENTIFYFONT, (WPARAM) this, (LPARAM) FSPlugins.FindFont( FS->GetEncoding(), PaneIndex ) );

		for (iFile=TheseFiles.begin(); iFile != TheseFiles.end(); iFile++) {
			NativeFile file = *iFile;
			
			bool Selected =  false;

			if ( FileSelections.size() >= (DWORD) ( file.fileID + 1 ) )
			{
				Selected = FileSelections[ file.fileID ];
			}

			if ( file.fileID < TheseFiles.size() )
			{
				DrawFile( file.fileID, &file, file.Icon, Selected );
			}
		}

		if ( ( Updated ) && ( ( SelectionStack.size() > 0 ) && ( SelectionStack.back() != -1 ) ) )
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

	BitBlt(hDC, 128,24,KnownX-128,KnownY, viewDC,128,24,SRCCOPY);
	BitBlt(hDC, 0,0,(KnownX - (ControlButtons.size() * 21)) + 15,24, viewDC, 0, 0, SRCCOPY);

	ReleaseDC(hWnd, hDC);

	LeaveCriticalSection( &CacheLock );
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
		IconsPerLine   = ( WindowWidth - 128 ) / 100U;
		LinesPerWindow = ( WindowHeight - 6U ) / 64U;
		break;

	case DisplayDetails:
		ItemWidth      = 400U;
		ItemHeight     = 40U;
		IconYOffset    = 6U;
		IconsPerLine   = ( WindowWidth - 128 ) / 400U;
		LinesPerWindow = (WindowHeight - 6U) / 40U;
		break;

	case DisplayList:
		ItemWidth      = 200U;
		ItemHeight     = 20U;
		IconYOffset    = 4U;
		IconsPerLine   = ( WindowWidth - 128 ) / 200U;
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

	DWORD rx = x - 128;

	if ( rx > ( ItemWidth * IconsPerLine ) )
	{
		// Outside of horizontally valid area
		return -2;
	}

	DWORD Index = ( (ay / ItemHeight) * IconsPerLine ) + ( rx / ItemWidth );

	if ( Index < TheseFiles.size() )
	{
		return Index;
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

	if (ix >= 0)
	{
		DIndex = (WORD) ix;

		NativeFileIterator i;

		bool Translateable = false;

		for ( i = TheseFiles.begin(); i != TheseFiles.end(); i++ )
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

		if ( SelectionStack.size() > 0 )
		{
			SelectionStack.back() = ix;
		}

		::SendMessage(ParentWnd, WM_ENTERICON, (WPARAM) hWnd, ix);
	}

	DIndex = 0xFFFF;
}

void CFileViewer::ClearItems( bool DoUpdate = true) {
	FileSelections.clear();

	if ( FS )
	{
		FreeLabels();

		EnterCriticalSection( &CacheLock );

		FileSelections = std::vector<bool>( TheseFiles.size(), false );
		FileLabels     = std::vector<FontBitmap *>( TheseFiles.size(), nullptr );
		FileDescs      = std::vector<FontBitmap *>( TheseFiles.size(), nullptr );

		LeaveCriticalSection( &CacheLock );
	}

	ParentSelected	= false;

	if ( DoUpdate )
	{
		Refresh();
	}
}

void CFileViewer::ToggleItem(int x, int y) {
	static WCHAR blank[1]	= {NULL};

	if (!FS)
		return;

	DWORD ix = GetItem(x,y);

	if ( ( ix > 0x80000000 ) || ( ix >= TheseFiles.size() ) ) {
		ClearItems();

		return;
	}

	EnterCriticalSection( &CacheLock );

	delete FileLabels[ ix ];

	FileLabels[ ix ] = nullptr;

	FileSelections[ix]	= !FileSelections[ix];

	LeaveCriticalSection( &CacheLock );

	UpdateSidePanelFlags();

	Refresh();
}

void CFileViewer::Update() {
	/* Lock the CS */
	EnterCriticalSection( &CacheLock );

	/* Take copies */
	if ( FS != nullptr )
	{
		TheseFiles = FS->pDirectory->Files;
		TheseIcons = FS->pDirectory->ResolvedIcons;
	}
	else
	{
		TheseFiles.clear();
		TheseIcons.clear();
	}

	FileEntries	   = TheseFiles.size();
	FileSelections = std::vector<bool>( FileEntries, false );
	FileLabels     = std::vector<FontBitmap *>( FileEntries, nullptr );
	FileDescs      = std::vector<FontBitmap *>( FileEntries, nullptr );

	Updated = true;

	LeaveCriticalSection( &CacheLock );

	SideBar.SetTopic( FS->TopicIcon, FSPlugins.FSName( FS->FSID ) );

	Refresh();
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
	/* If the FS doesn't support containing images, don't populate this */
	if ( FS->Flags & FSF_Prohibit_Nesting )
	{
		return;
	}

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



	NUTSProviderList Providers = FSPlugins.GetProviders();

	UINT GFXindex = GFX_MENU_BASE;
	UINT TXTindex = TXT_MENU_BASE;

	AppendMenu( hTextMenu, MF_STRING, (UINT) TXTindex, L"Text File" );

	MenuXlatorMap[ TXTindex ] = TUID_TEXT;

	TXTindex++;

	NUTSProvider_iter iter;

	for (iter = Providers.begin(); iter != Providers.end(); iter++ )
	{
		TranslatorList GFX = FSPlugins.GetTranslators( iter->ProviderID, TXGFXTranslator );

		TranslatorIterator gfx;

		for ( gfx = GFX.begin(); gfx != GFX.end(); gfx++ )
		{
			AppendMenu( hGraphixMenu, MF_STRING, (UINT) GFXindex, gfx->FriendlyName.c_str() );

			MenuXlatorMap[ GFXindex ] = gfx->ProviderID;

			GFXindex++;
		}

		TranslatorList TXT = FSPlugins.GetTranslators( iter->ProviderID, TXTextTranslator );

		TranslatorIterator txt;

		for ( txt = TXT.begin(); txt != TXT.end(); txt++ )
		{
			AppendMenu( hTextMenu, MF_STRING, (UINT) TXTindex, txt->FriendlyName.c_str() );

			MenuXlatorMap[ TXTindex ] = txt->ProviderID;

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
			Selection.push_back( TheseFiles[ Index ] );
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

				DoStatusBar();
			}
			else
			{
				OutputDebugStringA( "Activating it\n" );
				ActivateItem(mouseX, mouseY);

				DoStatusBar();
			}
		}

		if (Dragging) {
			OutputDebugStringA( "Dragging it " );
			SetCursor(LoadCursor(NULL, IDC_ARROW));

			SendMessage(ParentWnd, WM_FVENDDRAG, (WPARAM) hWnd, 0);
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

			EnterCriticalSection( &CacheLock );

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

			LeaveCriticalSection( &CacheLock );

			FreeLabels();

			Redraw();
		}

		DWORD Selected = GetSelectionCount();

		DoStatusBar();

		dragX	= -1;
		dragY	= -1;

		MouseDown = false;
		Dragging  = false;

		UpdateSidePanelFlags();
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
			SendMessage(ParentWnd, WM_FVSTARTDRAG, (WPARAM) hWnd, 0);
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

			Refresh();

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
		Selection.push_back( TheseFiles[ DIndex ] );
	}

	int wInd = 1;

	for ( iter = Selection.begin(); iter != Selection.end(); iter++ )
	{
		CTempFile FileObj;

		if ( FS->ReadFile( iter->fileID, FileObj ) != NUTS_SUCCESS )
		{
			NUTSError::Report( L"Read File", ParentWnd );

			return;
		}

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
		Selection.push_back( TheseFiles[ DIndex ] );
	}

	int wInd = 1;

	for ( iter = Selection.begin(); iter != Selection.end(); iter++ )
	{
		CTempFile FileObj;

		if ( FS->ReadFile( iter->fileID, FileObj ) != NUTS_SUCCESS )
		{
			NUTSError::Report( L"Read File", ParentWnd );

			return;
		}

		CTEXTContentViewer *pTXViewer;

		if ( PrefTUID == NULL )
		{
			pTXViewer = new CTEXTContentViewer( FileObj, iter->XlatorID );
		}
		else
		{
			pTXViewer = new CTEXTContentViewer( FileObj, PrefTUID );
		}

		pTXViewer->FSEncodingID = iter->EncodingID;

		/* Ah, but some things (e.g. TZX description files) should be PC437! */
		if ( iter->Flags & FF_EncodingOverride )
		{
			pTXViewer->FSEncodingID = ENCODING_ASCII;
		}

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
		Selection.push_back( TheseFiles[ DIndex ] );
	}

	TranslatorList TXList = FSPlugins.GetTranslators( NULL, NULL );

	GetWindowRect(ParentWnd, &rect);

	int wInd = 1;

	for ( iter = Selection.begin(); iter != Selection.end(); iter++ )
	{
		CTempFile FileObj;

		if ( FS->ReadFile( iter->fileID, FileObj ) != NUTS_SUCCESS )
		{
			NUTSError::Report( L"Read File", ParentWnd );

			return;
		}

		TranslatorIterator iTx;

		if ( iter->XlatorID == TUID_TEXT )
		{
			CTEXTContentViewer *pTXViewer;

			pTXViewer = new CTEXTContentViewer( FileObj, iter->XlatorID );

			pTXViewer->FSEncodingID = iter->EncodingID;

			/* Ah, but some things (e.g. TZX description files) should be PC437! */
			if ( iter->Flags & FF_EncodingOverride )
			{
				pTXViewer->FSEncodingID = ENCODING_ASCII;
			}

			pTXViewer->Create( hWnd, hInst, rect.left + (32 * wInd), rect.top + ( 32 * wInd ), 500 );

			wInd++;
		}

		for ( iTx = TXList.begin(); iTx != TXList.end(); iTx++ )
		{
			if ( ( iter->XlatorID == iTx->ProviderID ) && ( iTx->Flags & TXTextTranslator ) )
			{
				CTEXTContentViewer *pTXViewer;

				pTXViewer = new CTEXTContentViewer( FileObj, iter->XlatorID );

				pTXViewer->FSEncodingID = iter->EncodingID;

				pTXViewer->Create( hWnd, hInst, rect.left + (32 * wInd), rect.top + ( 32 * wInd ), 500 );

				wInd++;
			}

			if ( ( iter->XlatorID == iTx->ProviderID ) && ( iTx->Flags & TXGFXTranslator ) )
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
	IgnoreKeys = 1;

	NativeFileIterator iter;

	std::vector<NativeFile> Selection = GetSelection();

	for ( iter = Selection.begin(); iter != Selection.end(); iter++ )
	{
		if ( iter->Flags & FF_NotRenameable )
		{
			/* Call Rename anyway, so the FS can explain /why/ this is disallowed */
			FS->Rename( iter->fileID, nullptr, nullptr );

			continue;
		}

		BYTE OldName[ 256 ];
		BYTE OldExt[ 256 ];

		rstrncpy( OldName, iter->Filename, 256 );
		rstrncpy( OldExt, iter->Extension, 256 );

		CFileViewer::StaticEncoding = FS->GetEncoding();
		CFileViewer::StaticFlags    = FS->Flags;
		CFileViewer::pRenameFile    = OldName;
		CFileViewer::pRenameExt     = OldExt;

		if ( DialogBoxParam( hInst, MAKEINTRESOURCE(IDD_RENAME), hWnd, RenameDialogProc, (LPARAM) OldName ) == IDOK )
		{
			if ( FS->Rename( iter->fileID, OldName, OldExt ) != NUTS_SUCCESS )
			{
				NUTSError::Report( L"Rename", ParentWnd );
			}
		}

		if ( CFileViewer::pRenameEdit != nullptr )
		{
			delete CFileViewer::pRenameEdit;
		}

		CFileViewer::pRenameEdit = nullptr;

		if ( CFileViewer::pRenameEditX != nullptr )
		{
			delete CFileViewer::pRenameEditX;
		}

		CFileViewer::pRenameEditX = nullptr;
	}

	Update();
}

void CFileViewer::NewDirectory( void )
{
	IgnoreKeys = 1;

	BYTE DirName[ 256 ];
	BYTE DirExt[ 256 ];

	CFileViewer::pNewDir  = DirName;
	CFileViewer::pNewDirX = DirExt;

	DirName[ 0 ] = 0;
	DirExt[ 0 ]  = 0;

	CFileViewer::StaticEncoding = FS->GetEncoding();
	CFileViewer::StaticFlags    = FS->Flags;

	if ( DialogBoxParam( hInst, MAKEINTRESOURCE(IDD_NEWDIR), hWnd, NewDirDialogProc, (LPARAM) 0 ) == IDOK )
	{
		if ( pNewDir != nullptr )
		{
			NativeFile Dir;

			Dir.EncodingID = StaticEncoding;
			Dir.FSFileType = FT_UNSET;
			Dir.Filename   = pNewDir;

			if ( ( StaticFlags & FSF_Uses_Extensions ) && ( ! ( StaticFlags & FSF_NoDir_Extensions ) ) )
			{
				Dir.Extension = pNewDirX;
			}

			Dir.Flags = FF_Directory;
			Dir.Type  = FT_Directory;

			if ( FS->CreateDirectory( &Dir, CDF_MANUAL_OP ) != NUTS_SUCCESS )
			{
				NUTSError::Report( L"Create Directory", ParentWnd );
			}

			pNewDir  = nullptr;
			pNewDirX = nullptr;
		}
	}

	if ( pNewDirEdit != nullptr )
	{
		delete pNewDirEdit;

		pNewDirEdit  = nullptr;
	}

	if ( pNewDirEditX != nullptr )
	{
		delete pNewDirEditX;
	
		pNewDirEditX = nullptr;
	}

	Update();
}

BYTE *CFileViewer::pRenameFile          = nullptr;
EncodingEdit *CFileViewer::pRenameEdit  = nullptr;

BYTE *CFileViewer::pRenameExt           = nullptr;
EncodingEdit *CFileViewer::pRenameEditX = nullptr;

DWORD CFileViewer::StaticFlags    = 0;
DWORD CFileViewer::StaticEncoding = 0;

INT_PTR CALLBACK CFileViewer::RenameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch ( message )
	{
	case WM_INITDIALOG:
		{
			if ( StaticFlags & FSF_Uses_Extensions )
			{
				pRenameEdit  = new EncodingEdit( hDlg, 12, 52, 348, false );
				pRenameEditX = new EncodingEdit( hDlg, 362, 52, 105, true );

				pRenameEditX->Encoding = StaticEncoding;

				pRenameEditX->SetText( pRenameExt );

				pRenameEditX->SetBuddy( pRenameEdit );

				if ( StaticFlags & FSF_Fake_Extensions )
				{
					pRenameEditX->Disabled = true;
				}

				if ( !( StaticFlags & FSF_Supports_Spaces ) )
				{
					pRenameEditX->AllowSpaces = false;
				}
			}
			else
			{
				pRenameEdit = new EncodingEdit( hDlg, 12, 52, 455, true );
			}

			pRenameEdit->Encoding = StaticEncoding;

			if ( !( StaticFlags & FSF_Supports_Spaces ) )
			{
				pRenameEdit->AllowSpaces = false;
			}

			pRenameEdit->SetText( pRenameFile );

			pRenameEdit->SelectAll();
			pRenameEdit->SetFocus();
		}
		break;

	case WM_COMMAND:
		{
			if ( LOWORD( wParam ) == IDOK )
			{
				rstrncpy(  CFileViewer::pRenameFile, pRenameEdit->GetText(),  256 );

				if ( StaticFlags & FSF_Uses_Extensions )
				{
					rstrncpy(  CFileViewer::pRenameExt,  pRenameEditX->GetText(), 256 );
				}

				EndDialog( hDlg, IDOK );
			}
			else if ( LOWORD( wParam ) == IDCANCEL )
			{
				EndDialog( hDlg, IDCANCEL );
			}
			else if ( pRenameEditX != nullptr )
			{
				PostMessage( pRenameEditX->hWnd, message, wParam, lParam );
			}
			else if ( pRenameEdit != nullptr )
			{
				PostMessage( pRenameEdit->hWnd, message, wParam, lParam );
			}
		}
		break;
	}

	return FALSE;
}

BYTE *CFileViewer::pNewDir              = nullptr;
EncodingEdit *CFileViewer::pNewDirEdit  = nullptr;
BYTE *CFileViewer::pNewDirX             = nullptr;
EncodingEdit *CFileViewer::pNewDirEditX = nullptr;

INT_PTR CALLBACK CFileViewer::NewDirDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch ( message )
	{
	case WM_INITDIALOG:
		{
			if ( ( StaticFlags & FSF_Uses_Extensions ) && ( ! (StaticFlags & FSF_NoDir_Extensions) ) )
			{
				pNewDirEdit  = new EncodingEdit( hDlg, 12, 52, 348, false );
				pNewDirEditX = new EncodingEdit( hDlg, 362, 52, 105, true );

				pNewDirEditX->Encoding = StaticEncoding;

				pNewDirEditX->SetText( pNewDirX );

				pNewDirEditX->SetBuddy( pNewDirEdit );

				if ( StaticFlags & FSF_Fake_Extensions )
				{
					pNewDirEditX->Disabled = true;
				}

				if ( !( StaticFlags & FSF_Supports_Spaces ) )
				{
					pNewDirEditX->AllowSpaces = false;
				}
			}
			else
			{
				pNewDirEdit = new EncodingEdit( hDlg, 12, 52, 455, true );
			}

			pNewDirEdit->Encoding = CFileViewer::StaticEncoding;

			if ( !( StaticFlags & FSF_Supports_Spaces ) )
			{
				pNewDirEdit->AllowSpaces = false;
			}

			pNewDirEdit->SetFocus();
		}
		break;

	case WM_COMMAND:
		{
			if ( LOWORD( wParam ) == IDOK )
			{
				rstrncpy( pNewDir, pNewDirEdit->GetText(), 256 );

				if ( StaticFlags & FSF_Uses_Extensions )
				{
					if ( ! (StaticFlags & FSF_NoDir_Extensions ) )
					{
						rstrncpy( pNewDirX, pNewDirEditX->GetText(), 256 );
					}
				}

				EndDialog( hDlg, IDOK );
			}
			else if ( LOWORD( wParam ) == IDCANCEL )
			{
				CFileViewer::pNewDir = nullptr;

				EndDialog( hDlg, IDCANCEL );
			}
			else if ( pNewDirEditX != nullptr )
			{
				PostMessage( pNewDirEditX->hWnd, message, wParam, lParam );
			}
			else if ( pNewDirEdit != nullptr )
			{
				PostMessage( pNewDirEdit->hWnd, message, wParam, lParam );
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
	DWORD MaxFile  = TheseFiles.size();

	// The condition above should nix this, but still..
	if ( MaxFile == 0 )
	{
		return;
	}

	MaxFile--;

	if ( UpDown == 0x00 )
	{
		if ( SwapFile > 0 )
		{
			if ( FS->SwapFile( SwapFile - 1, SwapFile ) != NUTS_SUCCESS )
			{
				NUTSError::Report( L"File Swap", ParentWnd );

				return;
			}

			FileSelections[ SwapFile ]     = false;
			FileSelections[ SwapFile - 1 ] = true;
		}
	}
	else
	{
		if ( SwapFile < MaxFile )
		{
			if ( FS->SwapFile( SwapFile + 1, SwapFile ) != NUTS_SUCCESS )
			{
				NUTSError::Report( L"File Swap", ParentWnd );

				return;
			}

			FileSelections[ SwapFile ]     = false;
			FileSelections[ SwapFile + 1 ] = true;
		}
	}

	EnterCriticalSection( &CacheLock );

	TheseFiles = FS->pDirectory->Files;

	LeaveCriticalSection( &CacheLock );

	FreeLabels();

	Redraw();
}

void CFileViewer::SetSearching( bool s )
{
	IsSearching = s;

	if ( s )
	{
		ShowWindow( hProgress, SW_SHOW );
	}
	else
	{
		ShowWindow( hProgress, SW_HIDE );
	}
}


void CFileViewer::DoKeyControls( UINT message, WPARAM wParam, LPARAM lParam )
{
	if ( ( IsSearching ) || ( !HasFocus ) )
	{
		return;
	}

	int keyCode = (int) wParam;

	DWORD sc = GetSelectionCount();

	switch ( keyCode )
	{
	case VK_LEFT:
		{
			int si = FileSelections.size();

			std::vector<bool>::iterator iS;
			int fi = 0;

			for ( iS = FileSelections.begin(); iS != FileSelections.end(); iS++ )
			{
				if ( ( *iS ) && ( fi < si ) )
				{
					si = fi;
				}
				fi++;
			}

			if ( ( sc > 0 ) && ( si > 0 ) )
			{
				si--;
			}
			else
			{
				si = 0;
			}

			if ( !ShiftPressed() )
			{
				for ( iS = FileSelections.begin(); iS != FileSelections.end(); iS++ )
				{
					*iS = false;
				}
			}

			FileSelections[ si ] = true;
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
		}

		break;

	case VK_UP:
		{
			int   si = 0x7FFFFFFF;
			DWORD ei = 0x00000000;

			std::vector<bool>::iterator iS;
			DWORD fi = 0;

			if ( sc >0 ) 
			{
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
			}
			else
			{
				si = 0;
				ei = 0;
			}

			if ( !ShiftPressed() )
			{
				ClearItems( false );

				ei = si;
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

		break;

	case VK_DOWN:
		{
			int   si = 0x7FFFFFFF;
			DWORD ei = 0x00000000;

			std::vector<bool>::iterator iS;
			DWORD fi = 0;

			if ( sc > 0 )
			{
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
			}
			else
			{
				si = 0;
				ei = 0;
			}

			if ( !ShiftPressed() )
			{
				ClearItems( false );

				si = ei;
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

		break;

	case VK_RETURN:
		{
			if ( IgnoreKeys > 0 )
			{
				IgnoreKeys--;
			}
			else
			{
				DWORD si = GetSelectedIndex();

				ActivateItem( ( ( si % IconsPerLine ) * ItemWidth ) + ( ItemWidth / 2 ) + 128, ( (si / IconsPerLine ) * ItemHeight ) + (ItemHeight / 2 ) );
			}
		}
		break;

	case VK_APPS:
		DoContextMenu();
		break;
	}

	DoStatusBar();

	FreeLabels();

	UpdateSidePanelFlags();

	Redraw();
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

		if ( CurrentFSID == FS_Root )
		{
			DeleteMenu(hSubMenu, IDM_PROPERTIES, MF_BYCOMMAND);
		}
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

	if ( ! ( FS->Flags & FSF_Supports_Dirs ) )
	{
		EnableMenuItem( hPopup, IDM_NEWFOLDER, MF_BYCOMMAND | MF_DISABLED );
	}

	NativeFileIterator iFile;

	bool ImageSet = true;

	for ( iFile = TheseFiles.begin(); iFile != TheseFiles.end(); iFile++ )
	{
		if ( ( FileSelections[ iFile->fileID ] ) && ( iFile->Type != FT_MiscImage ) )
		{
			ImageSet = false;
		}
	}

	if ( ( Selected == 0 ) || ( !ImageSet ) )
	{
		EnableMenuItem( hPopup, IDM_INSTALL, MF_BYCOMMAND | MF_DISABLED );
	}

	if ( ( Selected == 0 ) || ( FS->Flags & FSF_ReadOnly ) )
	{
		EnableMenuItem( hPopup, IDM_DELETE, MF_BYCOMMAND | MF_DISABLED );
		EnableMenuItem( hPopup, IDM_RENAME, MF_BYCOMMAND | MF_DISABLED );
	}

	/* If no files are selected but at least one is playable, or at least one of the selected
	   files is playable, add the Play Audio menu option */

	bool Playable = false;

	DWORD c = GetSelectionCount();

	for ( iFile = TheseFiles.begin(); iFile != TheseFiles.end(); iFile++ )
	{
		if ( c > 0 )
		{
			if ( ( FileSelections[ iFile->fileID ] ) && ( iFile->Flags & FF_Audio) )
			{
				Playable = true;
			}
		}
		else
		{
			if ( iFile->Flags & FF_Audio )
			{
				Playable = true;
			}
		}
	}

	if ( Playable )
	{
		AppendMenu( hSubMenu, MF_SEPARATOR, 0, 0 );

		AppendMenu( hSubMenu, MF_STRING, IDM_PLAY_AUDIO, L"Play/Save &Tape Audio" );
	}

	DoLocalCommandMenu( hPopup );

	TrackPopupMenu(hSubMenu, 0, rect.left + mouseX, rect.top + mouseY, 0, hWnd, NULL);

	DestroyMenu(hPopup);
}

void CFileViewer::DoStatusBar( void )
{
	static BYTE * const status = (BYTE*) "N.U.T.S.";

	BYTE *pStatus = status;
	
	if ( FS != nullptr )
	{
		pStatus = FS->GetStatusString( GetSelectedIndex(), GetSelectionCount() );
	}

	::PostMessage( ParentWnd, WM_UPDATESTATUS, (WPARAM) this, (LPARAM) pStatus );
}

void CFileViewer::FreeLabels( void )
{
	std::vector<FontBitmap *>::iterator iF;

	EnterCriticalSection( &CacheLock );

	for ( iF = FileLabels.begin(); iF !=FileLabels.end(); iF++ )
	{
		if ( *iF != nullptr )
		{
			delete *iF;
		}

		*iF = nullptr;
	}

	for ( iF = FileDescs.begin(); iF !=FileDescs.end(); iF++ )
	{
		if ( *iF != nullptr )
		{
			delete *iF;
		}

		*iF = nullptr;
	}

	LeaveCriticalSection( &CacheLock );
}

void CFileViewer::DoLocalCommandMenu( HMENU hPopup )
{
	LocalCommands cmds = FS->GetLocalCommands();

	if ( !cmds.HasCommandSet )
	{
		return;
	}

	HMENU hSubMenu = GetSubMenu(hPopup, 0);	

	HMENU hCommandMenu = CreatePopupMenu();

	AppendMenu( hSubMenu, MF_SEPARATOR, 0, NULL );
	AppendMenu( hSubMenu, MF_STRING | MF_POPUP, (UINT_PTR) hCommandMenu, cmds.Root.c_str());

	UINT index = LC_MENU_BASE + 1;

	std::vector<LocalCommand>::iterator iter;

	for ( iter = cmds.CommandSet.begin(); iter != cmds.CommandSet.end(); iter++ )
	{
		UINT Flags = MF_STRING;

		DWORD s = GetSelectionCount();

		if ( ( s == 0 ) && ( ! ( iter->Flags & LC_ApplyNone ) ) ) { Flags |= MF_GRAYED; }
		if ( ( s == 1 ) && ( ! ( iter->Flags & LC_ApplyOne  ) ) ) { Flags |= MF_GRAYED; }
		if ( ( s >  1 ) && ( ! ( iter->Flags & LC_ApplyMany ) ) ) { Flags |= MF_GRAYED; }

		if ( iter->Flags & LC_IsChecked ) { Flags |= MF_CHECKED; }

		if ( iter->Flags & LC_IsSeparator )
		{
			AppendMenu( hCommandMenu, MF_SEPARATOR, 0, 0 );
		}
		else
		{
			AppendMenu( hCommandMenu, Flags, index, iter->Name.c_str() );
		}

		index++;
	}
	
}

void CFileViewer::DoPlayAudio( void )
{
	std::vector<NativeFile> TapeParts;

	NativeFileIterator iFile;

	DWORD c = GetSelectionCount();

	for ( iFile = TheseFiles.begin(); iFile != TheseFiles.end(); iFile++ )
	{
		if ( iFile->Flags & FF_Audio )
		{
			if ( c == 0 )
			{
				TapeParts.push_back( *iFile );
			}
			else
			{
				if ( FileSelections[ iFile->fileID ] )
				{
					TapeParts.push_back( *iFile );
				}
			}
		}
	}

	CTempFile store;
	TapeIndex indexes;

	FS->MakeAudio( TapeParts, indexes, store );

	store.Keep();

	AudioPlayer *player = new AudioPlayer( store, indexes );

	player->Create( hWnd, hInst );
}

void CFileViewer::ReCalculateTitleStack( std::vector<FileSystem *> *pFS, std::vector<TitleComponent> *pTitleStack )
{
	EnterCriticalSection( &CacheLock );

	std::vector< FileSystem *>::iterator iStack;
	
	pTitleStack->clear();

	iStack = pFS->begin();

	// Skip over the root if we have more than 1 FS deep 
	if ( pFS->size() > 1U )
	{
		iStack++;
	}

	while ( iStack != pFS->end() )
	{
		TitleComponent t;

		if ( (*iStack)->EnterIndex == 0xFFFFFFFF )
		{
			strncpy_s( (char *) t.String, 512, (char *) (*iStack)->GetTitleString( nullptr ), 511 );
		}
		else
		{
			strncpy_s( (char *) t.String, 512, (char *) (*iStack)->GetTitleString( &(*iStack)->pDirectory->Files[ (*iStack)->EnterIndex ] ), 511 );
		}

		t.Encoding = (*iStack)->GetEncoding();

		pTitleStack->push_back( t );

		iStack++;
	}
	
	SetTitleStack( *pTitleStack );

	LeaveCriticalSection( &CacheLock );
}

void CFileViewer::UpdateSidePanelFlags()
{
	DWORD f = 0;

	EnterCriticalSection( &CacheLock );

	DWORD sc = GetSelectionCount();

	if ( CurrentFSID == FS_Root )
	{
		f |= SPF_RootFS;
	}

	if ( FS->Flags & FSF_Supports_Dirs )
	{
		f |= SPF_NewDirs;
	}

	if ( ! ( FS->Flags & FSF_Prohibit_Nesting ) )
	{
		f |= SPF_NewImages;
	}

	if ( ( FS->Flags & FSF_Reorderable ) && ( sc > 0 ) )
	{
		f |= SPF_ReOrderable;
	}

	std::vector<bool>::iterator i;

	DWORD Index = 0;
	bool Rename = true;
	bool Delete = true;
	bool Copy   = true;
	bool Audio  = true;

	for ( i = FileSelections.begin(); i != FileSelections.end(); i++ )
	{
		if ( ( *i ) || ( sc == 0 ) )
		{
			if ( TheseFiles[ Index ].Flags & FF_NotRenameable )
			{
				Rename = false;
			}

			if ( TheseFiles[Index].Flags & FF_Pseudo )
			{
				Delete = false;
				Rename = false;
				Copy   = false;
			}

			if ( !( TheseFiles[Index].Flags & FF_Audio ) )
			{
				Audio = false;
			}
		}

		Index++;
	}

	if ( sc == 0 )
	{
		Delete = false;
		Rename = false;
		Copy   = false;
	}

	if ( Audio ) { f |= SPF_Playable; }
	if ( Delete ) { f |= SPF_Delete; }
	if ( Copy ) { f |= SPF_Copy; }
	if ( Rename ) { f |= SPF_Rename; }

	LeaveCriticalSection( &CacheLock );

	SideBar.SetFlags( f );
}
