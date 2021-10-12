#include "StdAfx.h"
#include "PaletteWindow.h"
#include "Defs.h"
#include "FontBitmap.h"
#include "FileDialogs.h"
#include "Plugins.h"

#include "resource.h"

#include <math.h>
#include <CommCtrl.h>
#include <CommDlg.h>

CPaletteWindow::CPaletteWindow(void)
{
	FlashPhase   = false;
	hWnd         = nullptr;
	hTabs        = nullptr;
	hPhysDlg     = nullptr;
	hColourArea  = nullptr;
	ColIn        = false;
	ColIndex     = 0xFFFF;
	SelectedTab  = TabLogical;

	pResetButton = nullptr;
	pSaveButton  = nullptr;
	pLoadButton  = nullptr;

	LogColChanging = false;
}

CPaletteWindow::~CPaletteWindow(void)
{
	DestroyWindows();
}

std::map<HWND,CPaletteWindow *> CPaletteWindow::WndMap;

bool CPaletteWindow::WndReg = false;

LRESULT CALLBACK CPaletteWindow::PaletteWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( WndMap.find( hWnd ) != WndMap.end() )
	{
		return WndMap[ hWnd ]->WindowProc( hWnd, message, wParam, lParam );
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}

LRESULT CALLBACK CPaletteWindow::ColourAreaWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( WndMap.find( hWnd ) != WndMap.end() )
	{
		return WndMap[ hWnd ]->WindowProc( hWnd, message, wParam, lParam );
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}

INT_PTR CALLBACK CPaletteWindow::LogWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if ( uMsg == WM_INITDIALOG )
	{
		WndMap[ hwndDlg ] = (CPaletteWindow *) lParam;
	}

	if ( WndMap.find( hwndDlg ) != WndMap.end() )
	{
		return WndMap[ hwndDlg ]->LogicalWindowProc( hwndDlg, uMsg, wParam, lParam );
	}

	return FALSE;
}

LRESULT CPaletteWindow::WindowProc(HWND hTarget, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( message == WM_ACTIVATE )
	{
		if ( wParam == 0 )
		{
			hActiveWnd = NULL;
		}
		else
		{
			hActiveWnd = hWnd;
		}
	}

	if ( (message == WM_PAINT) && ( hTarget == hColourArea ) )
	{
		PaintColours();

		if ( hPhysDlg )
		{
			::PostMessage( hPhysDlg, WM_PAINT, 0, 0 );
		}

		return 0;
	}

	if ( ( message == WM_LBUTTONDOWN ) && ( hTarget == hColourArea ) )
	{
		ColIn = true;

		ColIndex = (WORD) ( LOWORD(lParam ) / 32 ) + ( ( HIWORD( lParam ) / 32 ) * sqrBiggest );

		RECT	rect;

		GetClientRect( hColourArea, &rect );

		InvalidateRect( hColourArea, &rect, FALSE );

		return DefWindowProc( hTarget, message, wParam, lParam );
	}

	if ( ( message == WM_LBUTTONUP ) && ( hTarget == hColourArea ) )
	{
		if ( SelectedTab == TabLogical )
		{
			if ( pLogical->size() > 0 )
			{
				DoLogicalPaletteChange();
			}
		}

		if ( SelectedTab == TabPhysical )
		{
			DoPhysicalPaletteChange();
		}

		return DefWindowProc( hTarget, message, wParam, lParam );
	}

	if ( hTarget != hWnd )
	{
		return DefWindowProc( hTarget, message, wParam, lParam );
	}

	if (message == WM_SCPALCHANGED) {
		ComputeWindowSize();

		RECT	rect;

		GetClientRect( hColourArea, &rect );

		InvalidateRect( hColourArea, &rect, FALSE );
	}

	if ( message == WM_NOTIFY )
	{
		NMHDR *pNotify = (NMHDR *) lParam;

		if ( pNotify->code == TCN_SELCHANGE )
		{
			SelectedTab = (PaletteTab) TabCtrl_GetCurSel( hTabs );

			RECT	rect;

			GetClientRect( hColourArea, &rect );

			InvalidateRect( hColourArea, &rect, FALSE );
		}
	}

	if ( message == WM_TIMER )
	{
		FlashPhase = !FlashPhase;

		RECT	rect;

		GetClientRect( hColourArea, &rect );

		InvalidateRect( hColourArea, &rect, FALSE );
	}

	if ( message == WM_COMMAND )
	{
		HWND srcButton = (HWND) lParam;

		if ( srcButton == pResetButton->hWnd )
		{
			::SendMessage( Parent, WM_RESETPALETTE, (WPARAM) SelectedTab, 0 );
		}

		if ( srcButton == pLoadButton->hWnd )
		{
			DoLoadPalette();
		}

		if ( srcButton == pSaveButton->hWnd )
		{
			DoSavePalette();
		}
	}

	if ( message == WM_CLOSE )
	{
		/* Remove this now, so that this class is no longer referenced */
		WndMap.erase( hWnd );

		DestroyWindows();

		::PostMessage( Parent, WM_PALETTECLOSED, 0, 0 );
	}

	return DefWindowProc(hTarget, message, wParam, lParam);
}

void CPaletteWindow::PaintColours( void )
{
	HDC hDC = GetDC( hColourArea );

	WORD px = 0;
	WORD py = 0;
	WORD pi = 0;

	/* Reminder: The Logical Palette says which logical colour on the target
	             maps to which physical colour on the target.
				 
				 The Physical Colours says which physical colours exist on
				 the target.

				 The Physical Palette says how the NUTS user would like those
				 physical colours to apepar.
	*/

	RECT     cf;
	LOGBRUSH brsh;

	GetClientRect( hColourArea, &cf );

	FillRect( hDC, &cf, GetSysColorBrush( COLOR_WINDOW ));

	DWORD TotalCols = ( SelectedTab == TabLogical ) ? pLogical->size() : pPhysical->size();

	while ( pi < TotalColourSlots )
	{

		RECT r;

		r.left   = 1 + 32 * px;
		r.top    = 1 + 32 * py;
		r.right  = r.left  + 31;
		r.bottom = r.top   + 31;

		COLORREF Marker = 0;

		if ( pi < TotalCols )
		{

			brsh.lbStyle = BS_SOLID;

			if ( SelectedTab == TabLogical )
			{
				WORD PhysicalColour = pLogical->at( (WORD) pi );

				std::pair<WORD,WORD> PhysicalIndex = pColours->at( PhysicalColour );

				if ( !FlashPhase )
				{
					brsh.lbColor = pPhysical->at( PhysicalIndex.first );

					Marker = pPhysical->at( PhysicalIndex.first ) ^ 0xFFFFFFFF;
				}
				else
				{
					brsh.lbColor = pPhysical->at( PhysicalIndex.second );

					Marker = pPhysical->at( PhysicalIndex.second ) ^ 0xFFFFFFFF;
				}
			}

			if ( SelectedTab == TabPhysical )
			{
				brsh.lbColor = (COLORREF) pPhysical->at( pi );
			}
		}
		else
		{
			brsh.lbColor = GetSysColor( COLOR_WINDOW );
		}

		HBRUSH TempBrush = CreateBrushIndirect( &brsh );

		FillRect( hDC, &r, TempBrush );

		DeleteObject( (HGDIOBJ) TempBrush );

		if ( pi < TotalCols )
		{
			if ( ( pi == ColIndex ) && ( ColIn ) )
			{
				DrawEdge( hDC, &r, EDGE_SUNKEN, BF_RECT );
			}
			else
			{
				DrawEdge( hDC, &r, EDGE_RAISED, BF_RECT );
			}

			if ( ( pi == ColIndex ) && ( SelectedTab == TabLogical ) && ( LogColChanging ) )
			{
				HPEN TempPen = CreatePen( PS_SOLID, 2, Marker & 0x00FFFFFF );

				HGDIOBJ hOld = SelectObject( hDC, TempPen );

				MoveToEx( hDC, r.left + 4, r.top + 4, NULL );
				LineTo( hDC, r.right - 4, r.bottom - 4 );

				MoveToEx( hDC, r.right - 4, r.top + 4, NULL );
				LineTo( hDC, r.left + 4, r.bottom - 4 );

				SelectObject( hDC, hOld );
			}
		}

		pi++;
		px++;

		if ( px == sqrBiggest )
		{
			py++;

			px = 0;
		}
	}

	if ( ( SelectedTab == TabLogical ) && ( pLogical->size() == 0U ) )
	{
		DrawText( hDC, L"No logical palette in this mode.", 32, &cf, DT_LEFT | DT_TOP | DT_WORDBREAK );
	}

	ReleaseDC( hColourArea, hDC );

	GetClientRect( hColourArea, &cf );
	ValidateRect( hColourArea, &cf );
}

void CPaletteWindow::PaintPhysicalColours( HWND hDlg )
{
	HDC hDC = GetDC( hDlg );

	WORD px = 0;
	WORD py = 0;
	WORD pi = 0;

	/* Reminder: The Logical Palette says which logical colour on the target
	             maps to which physical colour on the target.
				 
				 The Physical Colours says which physical colours exist on
				 the target.

				 The Physical Palette says how the NUTS user would like those
				 physical colours to apepar.
	*/

	while ( pi < pColours->size() )
	{
		LOGBRUSH brsh;

		brsh.lbStyle = BS_SOLID;

		std::pair<WORD,WORD> PhysicalIndex = pColours->at( pi );

		if ( !FlashPhase )
		{
			brsh.lbColor = pPhysical->at( PhysicalIndex.first );
		}
		else
		{
			brsh.lbColor = pPhysical->at( PhysicalIndex.second );
		}

		HBRUSH TempBrush = CreateBrushIndirect( &brsh );

		RECT r;

		r.left   = 32 * px;
		r.top    = 32 * py;
		r.right  = r.left  + 31;
		r.bottom = r.top   + 31;

		FillRect( hDC, &r, TempBrush );

		if ( ( pi == PColIndex ) && ( PColIn ) )
		{
			DrawEdge( hDC, &r, EDGE_SUNKEN, BF_RECT );
		}
		else
		{
			DrawEdge( hDC, &r, EDGE_RAISED, BF_RECT );
		}

		DeleteObject( (HGDIOBJ) TempBrush );

		pi++;
		px++;

		if ( px == sqrColours )
		{
			py++;

			px = 0;
		}
	}

	ReleaseDC( hDlg, hDC );

	RECT rw;

	GetClientRect( hDlg, &rw );
	ValidateRect( hDlg, &rw );
}

static WCHAR *ColourAreaClass = L"NUTS Colour Picker Area";

void CPaletteWindow::Create(int x, int y, HWND ParentWnd, LogPalette *pLog, PhysPalette *pPhys, PhysColours *pCols ) {
	if ( !WndReg )
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= PaletteWindowProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInst;
		wcex.hIcon			= NULL;
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
		wcex.lpszMenuName	= NULL;
		wcex.lpszClassName	= L"NUTS Palette Controls";
		wcex.hIconSm		= NULL;

		RegisterClassEx( &wcex );

		WNDCLASS wc = { };

		wc.lpfnWndProc   = ColourAreaWindowProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = ColourAreaClass;

		RegisterClass(&wc);

		WndReg = true;
	}

	Parent = ParentWnd;
	hWnd   = NULL;

	pLogical  = pLog;
	pPhysical = pPhys;
	pColours  = pCols;

	hWnd = CreateWindowEx(
		WS_EX_TOOLWINDOW,
		L"NUTS Palette Controls",
		L"Palette",
		WS_CLIPCHILDREN | WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
		x, y, 100, 100,
		Parent, NULL, hInst, NULL
	);

	WndMap[ hWnd ] = this;

	SetTimer( hWnd, (UINT_PTR) 0x08A1E77E, 500, NULL );

	hTabs = CreateWindowEx(
		NULL,
		WC_TABCONTROL,
		NULL,
		WS_CHILD | WS_VISIBLE | TCS_FOCUSONBUTTONDOWN | WS_TABSTOP, // | TCS_FOCUSNEVER,
		0, 0, 100, 100,
		hWnd, NULL, hInst, NULL
	);

	hColourArea = CreateWindowEx(
		NULL,
		ColourAreaClass,
		NULL,
		WS_CHILD | WS_VISIBLE,
		0, 0, 100, 100,
		hTabs, NULL, hInst, NULL
	);

	WndMap[ hColourArea ] = this;

	TCITEM item;

	item.mask    = TCIF_TEXT;
	item.pszText = L"Logical";

	::SendMessage( hTabs, TCM_INSERTITEM, 0, (LPARAM) &item );

	item.pszText = L"Physical";

	::SendMessage( hTabs, TCM_INSERTITEM, 1, (LPARAM) &item );

	// Create the 3 seashells
	pResetButton = new IconButton( hWnd, 0, 24, LoadIcon( hInst, MAKEINTRESOURCE( IDI_ROOTFS ) ) );
	pResetButton->SetTip( L"Reset this palette to default values" );

	pSaveButton  = new IconButton( hWnd, 24, 24, LoadIcon( hInst, MAKEINTRESOURCE( IDI_SAVE ) ) );
	pSaveButton->SetTip( L"Save this palette as a file" );

	pLoadButton  = new IconButton( hWnd, 0, 24, LoadIcon( hInst, MAKEINTRESOURCE( IDI_OPEN ) ) );
	pLoadButton->SetTip( L"Load this palette from a file" );

	ComputeWindowSize();
}

void CPaletteWindow::DestroyWindows( void )
{
	NixWindow( hColourArea );
	NixWindow( hTabs );
	
	if ( pResetButton != nullptr )
	{
		delete pResetButton;
		delete pSaveButton;
		delete pLoadButton;
	}

	pResetButton = nullptr;
	pSaveButton  = nullptr;
	pLoadButton  = nullptr;

	NixWindow( hWnd );
}

void CPaletteWindow::ComputeWindowSize( void )
{
	WORD brdx = GetSystemMetrics(SM_CXBORDER);
	WORD brdy = GetSystemMetrics(SM_CYBORDER);
	WORD tity = GetSystemMetrics(SM_CYSMCAPTION);

	sqrLogical  = (int) floor( sqrt( (double) pLogical->size() ) );
	sqrPhysical = (int) floor( sqrt( (double) pPhysical->size() ) );

	while ( ( sqrLogical * sqrLogical )   < (WORD) pLogical->size() )  { sqrLogical++;  }
	while ( ( sqrPhysical * sqrPhysical ) < (WORD) pPhysical->size() ) { sqrPhysical++; }

	sqrBiggest = sqrLogical;

	if ( sqrBiggest < sqrPhysical ) { sqrBiggest = sqrPhysical; } // Unlikely, but you never know....

	WORD ry = sqrBiggest;

	/* Avoid silly windows */
	if ( ( pLogical->size() <= 8U ) && ( pPhysical->size() <= 8U ) )
	{
		sqrBiggest = pLogical->size();

		if ( sqrBiggest < pPhysical->size() )
		{
			sqrBiggest = pPhysical->size();
		}

		ry = 1;
	}

	RECT r;

	r.top    = 0;
	r.left   = 0;
	r.right  = 32 * sqrBiggest;
	r.bottom = 32 * ry;

	::SendMessage( hTabs, TCM_ADJUSTRECT, (WPARAM) TRUE, (LPARAM) &r );

	::SetWindowPos( hTabs, NULL, 4 + r.left + brdx, 4 + r.top + tity, r.right - r.left, r.bottom - r.top, SWP_NOREPOSITION | SWP_NOZORDER );

	if ( hWnd != nullptr )
	{
		::SetWindowPos( hWnd, NULL, 0, 0, (r.right - r.left) + (2 * brdx) + 8, (r.bottom - r.top) + brdy + tity + 8 + 24, SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOZORDER );
	}

	::SetWindowPos( pResetButton->hWnd, NULL, 8 + r.left + brdx,      4 + tity + r.bottom, 24, 24, SWP_NOREPOSITION | SWP_NOZORDER );
	::SetWindowPos( pLoadButton->hWnd,  NULL, 8 + r.left + brdx + 24, 4 + tity + r.bottom, 24, 24, SWP_NOREPOSITION | SWP_NOZORDER );
	::SetWindowPos( pSaveButton->hWnd,  NULL, 8 + r.left + brdx + 48, 4 + tity + r.bottom, 24, 24, SWP_NOREPOSITION | SWP_NOZORDER );

	GetClientRect( hTabs, &r );

	::SendMessage( hTabs, TCM_ADJUSTRECT, (WPARAM) FALSE, (LPARAM) &r );

	::SetWindowPos( hColourArea, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOREPOSITION | SWP_NOZORDER ); 

	TotalColourSlots = sqrBiggest * ry;

}

void CPaletteWindow::DoLogicalPaletteChange( void )
{
	LogColChanging = true;

	RECT r;

	GetWindowRect( hWnd, &r );

	LogX = r.left;
	LogY = r.bottom;

	DialogBoxParam( hInst, MAKEINTRESOURCE( IDD_LOG_SELECT ), hColourArea, LogWindowProc, (LPARAM) this );

	LogColChanging = false;

	(*pLogical)[ ColIndex ] = PColIndex;

	ColIn    = false;
	ColIndex = 0xFFFF;
	hPhysDlg = nullptr;

	::PostMessage( Parent, WM_SCPALCHANGED, 0, 0 );
}

void CPaletteWindow::DoPhysicalPaletteChange( void )
{
	static COLORREF customs[16] = {
		RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF),
		RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF),
		RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF),
		RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xFF,0xFF)
	};

	CHOOSECOLOR cc;

	cc.lStructSize  = sizeof( CHOOSECOLOR );
	cc.hwndOwner    = hColourArea;
	cc.rgbResult    = pPhysical->at( ColIndex );
	cc.lpCustColors = customs;
	cc.Flags        = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT | CC_SOLIDCOLOR;

	ChooseColor( &cc );

	(*pPhysical)[ ColIndex ] = cc.rgbResult;

	ColIn    = false;
	ColIndex = 0xFFFF;

	::PostMessage( Parent, WM_SCPALCHANGED, 0, 0 );
}

INT_PTR CPaletteWindow::LogicalWindowProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
	case WM_INITDIALOG:
		{
			PColIn    = false;
			PColIndex = 0xFFFF;

			DWORD brdx = GetSystemMetrics(SM_CXBORDER);
			DWORD brdy = GetSystemMetrics(SM_CYBORDER);

			sqrColours = (int) floor( sqrt( (double) pColours->size() ) );

			while ( ( sqrColours * sqrColours ) < (WORD) pColours->size() ) { sqrColours++; }

			DWORD ry = sqrColours;

			/* Avoid silly windows */
			if ( pColours->size() <= 8U ) 
			{
				sqrColours = pColours->size();

				ry = 1;
			}

			RECT r;

			GetWindowRect( hWnd, &r );

			r.right  = r.left + ( sqrColours * 32 );
			r.bottom = r.top  + ( ry * 32 );

			AdjustWindowRectEx( &r, WS_POPUP | DS_3DLOOK | DS_MODALFRAME, FALSE, WS_EX_DLGMODALFRAME | WS_EX_STATICEDGE );

			::SetWindowPos( hwndDlg, NULL, LogX + 2, LogY - 3, ( r.right - r.left ) + 3, ( r.bottom - r.top ) + 3, SWP_NOREPOSITION | SWP_NOZORDER );

			hPhysDlg = hwndDlg;
		}
		break;

	case WM_LBUTTONDOWN:
		{
			PColIn = true;

			PColIndex = (WORD) ( LOWORD(lParam ) / 32 ) + ( ( HIWORD( lParam ) / 32 ) * sqrColours );

			RECT	rect;

			GetClientRect( hwndDlg, &rect );

			InvalidateRect( hwndDlg, &rect, FALSE );
		}
		break;

	case WM_LBUTTONUP:
		hPhysDlg = nullptr;

		EndDialog( hwndDlg, 0 );

		break;

	case WM_CLOSE:
		hPhysDlg = nullptr;
		break;

	case WM_PAINT:
		{
			PaintPhysicalColours( hwndDlg );
		}
		break;
	}

	return FALSE;
}

void CPaletteWindow::DoSavePalette( void )
{
	std::wstring filename;

	bool FileRes = false;

	if ( SelectedTab == TabLogical )
	{
		FileRes = SaveFileDialog( hWnd, filename, L"NUTS Logical Palette", L"LPL", L"Save Logical Palette" );
	}
	else
	{
		FileRes = SaveFileDialog( hWnd, filename, L"NUTS Physical Palette", L"PPL", L"Save Physical Palette" );
	}

	if ( FileRes )
	{
		FILE	*fFile;

		_wfopen_s( &fFile, filename.c_str(), L"wb" );

		if (!fFile) {
			MessageBox( hWnd, L"The palette file could not be saved", L"File Save Error", MB_ICONEXCLAMATION | MB_ICONERROR | MB_OK );

			return;
		}

		if ( SelectedTab == TabLogical )
		{
			std::map<WORD,WORD>::iterator iCol;

			for ( iCol = pLogical->begin(); iCol != pLogical->end(); iCol++ )
			{
				fwrite( &iCol->first, 1, sizeof(WORD), fFile );
				fwrite( &iCol->second, 1, sizeof(WORD), fFile );
			}
		}

		if ( SelectedTab == TabPhysical )
		{
			std::map<WORD,DWORD>::iterator iCol;

			for ( iCol = pPhysical->begin(); iCol != pPhysical->end(); iCol++ )
			{
				fwrite( &iCol->first, 1, sizeof(WORD), fFile );
				fwrite( &iCol->second, 1, sizeof(DWORD), fFile );
			}
		}

		fclose( fFile );
	}
}

void CPaletteWindow::DoLoadPalette( void )
{
	std::wstring filename;

	bool FileRes = false;

	if ( SelectedTab == TabLogical )
	{
		FileRes = OpenFileDialog( hWnd, filename, L"NUTS Logical Palette", L"LPL", L"Load Logical Palette" );
	}
	else
	{
		FileRes = OpenFileDialog( hWnd, filename, L"NUTS Physical Palette", L"PPL", L"Load Physical Palette" );
	}

	if ( FileRes )
	{
		FILE	*fFile;

		_wfopen_s( &fFile, filename.c_str(), L"rb" );

		if (!fFile) {
			MessageBox( hWnd, L"The palette file could not be opened", L"File Load Error", MB_ICONEXCLAMATION | MB_ICONERROR | MB_OK );

			return;
		}

		if ( SelectedTab == TabLogical )
		{
			pLogical->clear();

			while ( 1 )
			{
				if ( feof( fFile ) )
				{
					break;
				}

				WORD nCol,vCol;

				fread( &nCol, 1, sizeof(WORD), fFile );
				fread( &vCol, 1, sizeof(WORD), fFile );

				(*pLogical)[ nCol ] = vCol;
			}
		}

		if ( SelectedTab == TabPhysical )
		{
			
			pPhysical->clear();

			while ( 1 )
			{
				if ( feof( fFile ) )
				{
					break;
				}

				WORD  nCol;
				DWORD vCol;

				fread( &nCol, 1, sizeof(WORD), fFile );
				fread( &vCol, 1, sizeof(DWORD), fFile );

				(*pPhysical)[ nCol ] = vCol;
			}
		}

		fclose( fFile );
	}

	::SendMessage( Parent, WM_SCPALCHANGED, (WPARAM) SelectedTab, 0 );
}
