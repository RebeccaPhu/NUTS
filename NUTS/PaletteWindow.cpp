#include "StdAfx.h"
#include "PaletteWindow.h"
#include "Defs.h"
#include "FontBitmap.h"
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
	hSaveButton  = nullptr;
	hLoadButton  = nullptr;
	hResetButton = nullptr;
	hColourArea  = nullptr;
	ColIn        = false;
	ColIndex     = 0xFFFF;
	SelectedTab  = TabLogical;
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

		if ( srcButton == hResetButton )
		{
			::SendMessage( Parent, WM_RESETPALETTE, (WPARAM) SelectedTab, 0 );
		}

		if ( srcButton == hLoadButton )
		{
			DoLoadPalette();
		}

		if ( srcButton == hSaveButton )
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
	HBRUSH   TempBrush;

	GetClientRect( hColourArea, &cf );

	brsh.lbStyle = BS_SOLID;
	brsh.lbColor = GetSysColor( COLOR_WINDOW );

	TempBrush = CreateBrushIndirect( &brsh );

	FillRect( hDC, &cf, TempBrush );

	DeleteObject( TempBrush );

	DWORD TotalCols = ( SelectedTab == TabLogical ) ? pLogical->size() : pPhysical->size();

	while ( pi < TotalColourSlots )
	{

		RECT r;

		r.left   = 1 + 32 * px;
		r.top    = 1 + 32 * py;
		r.right  = r.left  + 31;
		r.bottom = r.top   + 31;

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
				}
				else
				{
					brsh.lbColor = pPhysical->at( PhysicalIndex.second );
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

		TempBrush = CreateBrushIndirect( &brsh );

		FillRect( hDC, &r, TempBrush );

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
		}

		DeleteObject( (HGDIOBJ) TempBrush );

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
		FontBitmap note( FONTID_PC437, (BYTE *) "No Logical Palette in This Mode", 64, false, false );

		note.DrawText( hDC, 6, 10, DT_LEFT );
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
		WS_CLIPSIBLINGS | WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
		x, y, 100, 100,
		Parent, NULL, hInst, NULL
	);

	WndMap[ hWnd ] = this;

	SetTimer( hWnd, (UINT_PTR) 0x08A1E77E, 500, NULL );

	hTabs = CreateWindowEx(
		NULL,
		WC_TABCONTROL,
		NULL,
		WS_CHILD | WS_VISIBLE | TCS_FOCUSNEVER,
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
	hResetButton = CreateWindowEx(NULL, L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_ICON,  0, 24, 24, 24, hWnd, NULL, hInst, NULL);
	hSaveButton  = CreateWindowEx(NULL, L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_ICON, 24, 24, 24, 24, hWnd, NULL, hInst, NULL);
	hLoadButton  = CreateWindowEx(NULL, L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_ICON, 48, 24, 24, 24, hWnd, NULL, hInst, NULL);

	HICON hRootIcon = LoadIcon( hInst, MAKEINTRESOURCE(IDI_ROOTFS) );

	SendMessage( hResetButton, BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hRootIcon );

	HICON hOpenIcon = LoadIcon( hInst, MAKEINTRESOURCE(IDI_OPEN) );

	SendMessage( hLoadButton, BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hOpenIcon );

	HICON hSaveIcon = LoadIcon( hInst, MAKEINTRESOURCE(IDI_SAVE) );

	SendMessage( hSaveButton, BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hSaveIcon );

	ComputeWindowSize();
}

void CPaletteWindow::DestroyWindows( void )
{
	NixWindow( hColourArea );
	NixWindow( hTabs );
	NixWindow( hResetButton );
	NixWindow( hSaveButton );
	NixWindow( hLoadButton );
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

	::SetWindowPos( hResetButton, NULL, 8 + r.left + brdx,      4 + tity + r.bottom, 24, 24, SWP_NOREPOSITION | SWP_NOZORDER );
	::SetWindowPos( hLoadButton,  NULL, 8 + r.left + brdx + 24, 4 + tity + r.bottom, 24, 24, SWP_NOREPOSITION | SWP_NOZORDER );
	::SetWindowPos( hSaveButton,  NULL, 8 + r.left + brdx + 48, 4 + tity + r.bottom, 24, 24, SWP_NOREPOSITION | SWP_NOZORDER );

	GetClientRect( hTabs, &r );

	::SendMessage( hTabs, TCM_ADJUSTRECT, (WPARAM) FALSE, (LPARAM) &r );

	::SetWindowPos( hColourArea, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOREPOSITION | SWP_NOZORDER ); 

	TotalColourSlots = sqrBiggest * ry;

}

void CPaletteWindow::DoLogicalPaletteChange( void )
{
	DialogBoxParam( hInst, MAKEINTRESOURCE( IDD_LOG_SELECT ), hColourArea, LogWindowProc, (LPARAM) this );

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

			::SetWindowPos( hwndDlg, NULL, 0, 0, ( sqrColours * 32 ) + (2 * brdx), ( ry * 32 ) + ( 2 * brdy ), SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOZORDER );

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
	WCHAR Filename[ MAX_PATH + 4 ];

	Filename[ 0 ] = 0;

	OPENFILENAME of;

	ZeroMemory( &of, sizeof( of ) );

	of.lStructSize       = sizeof( OPENFILENAME );
	of.hwndOwner         = hWnd;
	of.lpstrCustomFilter = NULL;
	of.lpstrFile         = Filename;
	of.nMaxFile          = MAX_PATH - 1;
	of.lpstrFileTitle    = NULL;
	of.nMaxFileTitle     = 0;
	of.lpstrInitialDir   = NULL;
	of.nFilterIndex      = 1;
	of.Flags             = OFN_OVERWRITEPROMPT;

	if ( SelectedTab == TabLogical )
	{
		of.lpstrFilter = L"NUTS Logical Palette\0*.LPL\0\0";
		of.lpstrTitle  = L"Save Logical Palette";
	}
	else
	{
		of.lpstrFilter = L"NUTS Physical Palette\0*.PPL\0\0";
		of.lpstrTitle  = L"Save Physical Palette";
	}

	if ( GetSaveFileName( &of ) )
	{
		if ( SelectedTab == TabLogical )
		{
			wcscat_s( Filename, MAX_PATH + 4, L".LPL" );
		}
		else
		{
			wcscat_s( Filename, MAX_PATH + 4, L".PPL" );
		}

		FILE	*fFile;

		_wfopen_s( &fFile, of.lpstrFile, L"wb" );

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
	WCHAR Filename[ MAX_PATH ];

	Filename[ 0 ] = 0;

	OPENFILENAME of;

	ZeroMemory( &of, sizeof( of ) );

	of.lStructSize       = sizeof( OPENFILENAME );
	of.hwndOwner         = hWnd;
	of.lpstrCustomFilter = NULL;
	of.lpstrFile         = Filename;
	of.nMaxFile          = MAX_PATH;
	of.lpstrFileTitle    = NULL;
	of.nMaxFileTitle     = 0;
	of.lpstrInitialDir   = NULL;
	of.nFilterIndex      = 1;
	of.Flags             = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

	if ( SelectedTab == TabLogical )
	{
		of.lpstrFilter = L"NUTS Logical Palette\0*.LPL\0\0";
		of.lpstrTitle  = L"Load Logical Palette";
	}
	else
	{
		of.lpstrFilter = L"NUTS Physical Palette\0*.PPL\0\0";
		of.lpstrTitle  = L"Load Physical Palette";
	}

	if ( GetOpenFileName( &of ) )
	{
		FILE	*fFile;

		_wfopen_s( &fFile, of.lpstrFile, L"rb" );

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
