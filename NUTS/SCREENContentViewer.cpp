#include "StdAfx.h"
#include "SCREENContentViewer.h"
#include "resource.h"
#include "PaletteWindow.h"
#include "FileDialogs.h"
#include "Plugins.h"
#include "IconRatio.h"
#include "NUTSError.h"
#include "libfuncs.h"

#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <process.h>
#include <assert.h>
#include <math.h>

std::map<HWND, CSCREENContentViewer *> CSCREENContentViewer::viewers;

bool CSCREENContentViewer::WndClassReg = false;

static DWORD dwthreadid;

#define ProgW 128
#define ProgH 18

LRESULT CALLBACK CSCREENContentViewer::SCViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( viewers.find( hWnd ) != viewers.end() )
	{
		return viewers[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

CSCREENContentViewer::CSCREENContentViewer( CTempFile &FileObj, DWORD TUID ) {
	if ( !WndClassReg )
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= CSCREENContentViewer::SCViewerProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInst;
		wcex.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(IDI_SPRITE));
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
		wcex.lpszMenuName	= NULL;
		wcex.lpszClassName	= L"NUTS Graphic Viewer";
		wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SPRITE));

		RegisterClassEx(&wcex);

		WndClassReg = true;
	}

	Mode       = 5;
	bAntiAlias = false;
	palWnd     = nullptr;
	pixels1    = nullptr;
	pixels2    = nullptr;
	Effects    = 0;
	lContent   = FileObj.Ext();
	Length     = lContent;
	Offset     = 0;

	bmi        = (BITMAPINFO *) malloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));

	pXlator     = NULL;
	XlatorID    = TUID;

	FirstTranslate = true;

	Path = FileObj.Name();

	FileObj.Keep();

	hWnd           = nullptr;
	ParentWnd      = nullptr;
	hToolbar       = nullptr;
	hModeList      = nullptr;
	pPaletteButton = nullptr;
	hEffects       = nullptr;
	pCopy          = nullptr;
	pSave          = nullptr;
	pPrint         = nullptr;
	hOffsetPrompt  = nullptr;
	hLengthPrompt  = nullptr;
	pOffset        = nullptr;
	pLength        = nullptr;

	hTranslateThread = NULL;
	hTerminate       = NULL;
	hPoke            = NULL;

	hModeTip    = NULL;
	hEffectsTip = NULL;
	hOffsetTip  = NULL;
	hLengthTip  = NULL;
}

CSCREENContentViewer::~CSCREENContentViewer(void) {
	DestroyWindows();

	free(bmi);

	if ( pixels1 != nullptr )
	{
		free(pixels1);
	}

	if ( pixels2 != nullptr )
	{
		free(pixels2);
	}

	_wunlink( Path.c_str() );

	if ( pXlator != nullptr )
	{
		delete pXlator;
	}

	if ( hTranslateThread )
	{
		SetEvent( hTerminate );

		if ( WaitForSingleObject( hTranslateThread, 5000 ) == WAIT_TIMEOUT )
		{
			TerminateThread( hTranslateThread, 500 );
		}

		CloseHandle( hTranslateThread );
		CloseHandle( hTerminate );
		CloseHandle( hPoke );
	}
}

int CSCREENContentViewer::CreateToolbar( void ) {

	int		h = 4;

	hModeList	= CreateWindowEx(NULL, L"COMBOBOX", L"", CBS_DROPDOWNLIST|WS_CHILD|WS_VISIBLE|WS_TABSTOP, h, 4, 200, 250, hWnd, NULL, hInst, NULL); h += 208;

	if ( pXlator != nullptr )
	{
		ModeList Modes = pXlator->GetModes();

		if ( Modes.size() > 0 )
		{
			ModeList_iter iMode;

			for ( iMode = Modes.begin(); iMode != Modes.end(); iMode++ )
			{
				SendMessage(hModeList, CB_ADDSTRING, 0, (LPARAM) iMode->FriendlyName.c_str() );
			}

			CTempFile FileObj( Path );

			FileObj.Keep();

			Mode = pXlator->GuessMode( FileObj );

			SendMessage(hModeList, CB_SETCURSEL, Mode, 0);

			hModeTip = CreateToolTip( hModeList, hWnd, L"Select the screen mode to use to interpret the graphic data", hInst );
		}
		else
		{
			EnableWindow( hModeList, FALSE );
		}
	}
	else
	{
		EnableWindow(hModeList, FALSE);
	}

	pPaletteButton = new IconButton( hWnd, h, 4, LoadIcon( hInst, MAKEINTRESOURCE( IDI_PALETTE ) ) ); h += 28;

	pPaletteButton->SetTip( L"Adjust the logical palette assignments and/or physical video palette" );


	pCopy = new IconButton( hWnd, h, 4, LoadIcon( hInst, MAKEINTRESOURCE( IDI_COPYFILE ) ) ); h += 28;

	pCopy->SetTip( L"Copy the image to the clipboard" );


	pSave = new IconButton( hWnd, h, 4, LoadIcon( hInst, MAKEINTRESOURCE( IDI_SAVE ) ) ); h += 28;

	pSave->SetTip( L"Save the image as a Windows 32-bit Bitmap image file" );


	pPrint = new IconButton( hWnd, h, 4, LoadIcon( hInst, MAKEINTRESOURCE( IDI_PRINT ) ) ); h += 28;

	pPrint->SetTip( L"Print the image filling the page" );

	

	hEffects = CreateWindowEx(NULL, L"BUTTON", L"Effects ▼", WS_CHILD|WS_VISIBLE|WS_TABSTOP,h,6,100,20,hWnd,NULL,hInst,NULL); h += 104;

	hEffectsTip = CreateToolTip( hEffects, hWnd, L"Apply processing effects to the image", hInst );


	hOffsetPrompt = CreateWindowEx( NULL, L"STATIC", L"Offset:", WS_VISIBLE|WS_CHILD,h,8,50,20, hWnd, NULL, hInst, NULL ); h += 44;

	pOffset = new EncodingEdit( hWnd, h, 4, 50, false ); h+= 56;

	pOffset->AllowedChars  = EncodingEdit::textInputHex;
	pOffset->MaxLen        = 8;
	pOffset->AllowNegative = true;
	pOffset->SetText( (BYTE *) "0" );

	hOffsetTip = CreateToolTip( pOffset->hWnd, hWnd, L"The offset (in hex bytes) to start process the data. Can be negative. (Not always applicable)", hInst );


	hLengthPrompt = CreateWindowEx( NULL, L"STATIC", L"Length:", WS_VISIBLE|WS_CHILD,h,8,50,20, hWnd, NULL, hInst, NULL ); h += 50;

	pLength = new EncodingEdit( hWnd, h, 4, 50, false ); h += 56;

	std::string sLen = std::to_string( (QWORD) lContent );

	pLength->AllowedChars = EncodingEdit::textInputHex;
	pLength->MaxLen       = 8;
	pLength->SetText( (BYTE *) sLen.c_str() );

	hLengthTip = CreateToolTip( pLength->hWnd, hWnd, L"Number of bytes of source image data to process. (Not always applicable)", hInst );

	return 0;
}

int CSCREENContentViewer::Create(HWND Parent, HINSTANCE hInstance, int x, int w, int h) {
	int	brdx	= GetSystemMetrics(SM_CXBORDER);
	int	brdy	= GetSystemMetrics(SM_CYBORDER);
	int	tity	= GetSystemMetrics(SM_CYCAPTION);
	int	frmx	= GetSystemMetrics(SM_CXFIXEDFRAME);
	int	frmy	= GetSystemMetrics(SM_CYFIXEDFRAME);

	int	wx		= 640;
	int	wy		= 512;

	hWnd = CreateWindowEx(
		NULL,
		L"NUTS Graphic Viewer",
		L"NUTS Graphic Viewer",
		WS_SYSMENU | WS_CLIPCHILDREN | WS_OVERLAPPED | WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED,
		CW_USEDEFAULT,
		0, wx + (2 * frmx), wy + (2 * frmy) + tity + 32,
		GetDesktopWindow(), NULL, hInstance, NULL
	);

	pXlator = (SCREENTranslator *) FSPlugins.LoadTranslator( XlatorID );

	viewers[ hWnd ] = this;

	ParentWnd	= Parent;

	CreateToolbar();

	PhysicalPalette = pXlator->GetPhysicalPalette();
	LogicalPalette  = pXlator->GetLogicalPalette( Mode );
	PhysicalColours = pXlator->GetPhysicalColours( );

	hTerminate = CreateEvent( NULL, TRUE, FALSE, NULL );
	hPoke      = CreateEvent( NULL, FALSE, TRUE, NULL );

	hProgress = CreateWindowEx(
		0, PROGRESS_CLASS, NULL,
		WS_CHILD | PBS_MARQUEE,
		0, 0, ProgW, ProgH,
		hWnd, NULL, hInst, NULL
	);
	
	::PostMessage( hProgress, PBM_SETMARQUEE, (WPARAM) TRUE, 0 );

	hTranslateThread = (HANDLE) _beginthreadex(NULL, NULL, _TranslateThread, this, NULL, (unsigned int *) &dwthreadid);

	SetFocus( pPaletteButton->hWnd );

	SetForegroundWindow( hWnd );
	SetActiveWindow( hWnd );
	SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE );

	ShowWindow(hWnd, TRUE);
	UpdateWindow(hWnd);

	SetTimer(hWnd, 0x1001, 500, NULL);

	return 0;
}

void CSCREENContentViewer::DestroyWindows( void )
{
	NixWindow( hModeTip );
	NixWindow( hEffectsTip );
	NixWindow( hOffsetTip );
	NixWindow( hLengthTip );

	NixWindow( hProgress );
	NixWindow( hLengthPrompt );
	NixWindow( hOffsetPrompt );
	NixWindow( hEffects );
	NixWindow( hModeList );
	NixWindow( hToolbar );
	NixWindow( hWnd );

	if ( pPaletteButton != nullptr )
	{
		delete pPaletteButton;
		delete pPrint;
		delete pSave;
		delete pCopy;
	}

	if ( pOffset != nullptr ) { delete pOffset; pOffset = nullptr; }
	if ( pLength != nullptr ) { delete pLength; pLength = nullptr; }
}

LRESULT	CSCREENContentViewer::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	if ( ( message == WM_ACTIVATE ) || ( message == WM_ACTIVATEAPP ) )
	{
		if ( wParam == 0 )
		{
			hActiveWnd = NULL;
		}
		else
		{
			hActiveWnd = hWnd;
		}

		return FALSE;
	}

	if (message == WM_PAINT) {
		PaintToolBar();

		DisplayImage();

		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	if ((message == WM_COMMAND) && (HIWORD(wParam) == CBN_SELCHANGE)) {
		Mode = SendMessage(hModeList, CB_GETCURSEL,0,0);

		if ( pXlator != nullptr )
		{
			LogicalPalette = pXlator->GetLogicalPalette( Mode );
		}

		if (palWnd) {
			SendMessage(palWnd->hWnd, WM_SCPALCHANGED, 0, 0);
		}

		SetEvent( hPoke );

		RECT	rect;

		GetClientRect(hWnd, &rect);

		InvalidateRect(hWnd, &rect, FALSE);

		return 0;
	}

	if ((message == WM_COMMAND) && (lParam == (LPARAM) hEffects)) {
		DoEffectsMenu();

		return 0;
	}

	if ((message == WM_COMMAND) && (lParam == (LPARAM) pCopy->hWnd)) {
		DoCopyImage( false );

		return 0;
	}

	if ((message == WM_COMMAND) && (lParam == (LPARAM) pSave->hWnd)) {
		DoCopyImage( true );

		return 0;
	}

	if ((message == WM_COMMAND) && (lParam == (LPARAM) pPrint->hWnd)) {
		DoPrintImage();

		return 0;
	}

	if ((message == WM_COMMAND) && (lParam == (LPARAM) pOffset->hWnd)) {
		Offset = pOffset->GetHexText();

		if ( Offset >= (long) lContent )
		{
			Offset = lContent - 1;
		}

		if ( Offset <= -1048576 )
		{
			Offset = 0U;
		}

		if ( lContent == 0U ) { Offset = 0U; }

		KillTimer( hWnd, 0x5330 );
		SetTimer( hWnd, 0x5330, 500, NULL );

		return 0;
	}

	if ((message == WM_COMMAND) && (lParam == (LPARAM) pLength->hWnd)) {
		Length = pLength->GetHexText();

		if ( Length > lContent )
		{
			Length = lContent;
		}

		KillTimer( hWnd, 0x5330 );
		SetTimer( hWnd, 0x5330, 500, NULL );

		return 0;
	}

	if ((message == WM_COMMAND) && (lParam == (LPARAM) pPaletteButton->hWnd)) {
		RECT rect;

		if ( palWnd == nullptr ) {
			palWnd	= new CPaletteWindow();

			GetWindowRect( pPaletteButton->hWnd, &rect);

			palWnd->Create( rect.left, rect.top + 32, hWnd, &LogicalPalette, &PhysicalPalette, &PhysicalColours );
		}

		GetClientRect(hWnd, &rect);

		InvalidateRect(hWnd, &rect, FALSE);

		return 0;
	}

	if ( message == WM_PALETTECLOSED )
	{
		palWnd = nullptr;

		return 0;
	}

	if ( (message == WM_TIMER) && ( wParam == 0x5330 ) ) {
		message = WM_SCPALCHANGED;

		KillTimer( hWnd, 0x5330 );

		return 0;
	}

	if ( ( message == WM_COMMAND ) && ( LOWORD(wParam) == (LPARAM) ID_EFFECTS_ANTIALIAS  ) ) { Effects ^= Effect_Antialias; message = WM_SCPALCHANGED; }
	if ( ( message == WM_COMMAND ) && ( LOWORD(wParam) == (LPARAM) ID_EFFECTS_CRTGLOW    ) ) { Effects ^= Effect_CRTGlow;   message = WM_SCPALCHANGED; }
	if ( ( message == WM_COMMAND ) && ( LOWORD(wParam) == (LPARAM) ID_EFFECTS_SCANLINES  ) ) { Effects ^= Effect_Scanlines; message = WM_SCPALCHANGED; }
	if ( ( message == WM_COMMAND ) && ( LOWORD(wParam) == (LPARAM) ID_EFFECTS_SNOW       ) ) { Effects ^= Effect_Snow;      message = WM_SCPALCHANGED; }
	if ( ( message == WM_COMMAND ) && ( LOWORD(wParam) == (LPARAM) ID_EFFECTS_GHOSTING   ) ) { Effects ^= Effect_Ghosting;  message = WM_SCPALCHANGED; }
	if ( ( message == WM_COMMAND ) && ( LOWORD(wParam) == (LPARAM) ID_EFFECTS_RAINBOWING ) ) { Effects ^= Effect_Rainbow;   message = WM_SCPALCHANGED; }


	if (message == WM_SCPALCHANGED) {
		SetEvent( hPoke );

		RECT rect;

		GetClientRect(hWnd, &rect);

		InvalidateRect(hWnd, &rect, FALSE);

		return 0;
	}

	if ( (message == WM_TIMER) && ( wParam == 0x1001 ) ) {
		Flash = ! Flash;

		DisplayImage();

		return 0;
	}

	if ( message == WM_RESETPALETTE )
	{
		if ( ( wParam == 0 ) && ( pXlator != nullptr ) )
		{
			LogicalPalette = pXlator->GetLogicalPalette( Mode );
		}

		if ( ( wParam == 1 ) && ( pXlator != nullptr ) )
		{
			PhysicalPalette = pXlator->GetPhysicalPalette();
		}

		SendMessage( palWnd->hWnd, WM_SCPALCHANGED, 0, 0 );
		PostMessage( hWnd, WM_SCPALCHANGED, 0, 0 );

		return 0;
	}

	if ( message == WM_DESTROY )
	{
		/* Nix this from the window map so we don't get further messages */
		viewers.erase( hWnd );

		delete this;

		return 0;
	}

	return DefWindowProc( hWnd, message, wParam, lParam);
}

int CSCREENContentViewer::PaintToolBar( void ) {
	RECT	rect;
	HDC		hDC;

	hDC	= GetDC(hWnd);

	GetClientRect(hWnd, &rect);

	rect.bottom	= rect.top + 36;

	DrawEdge(hDC, &rect, EDGE_RAISED, BF_RECT);

	ReleaseDC(hWnd, hDC);

	GetClientRect(hModeList, &rect);      InvalidateRect(hModeList, &rect, FALSE);
	
	GetClientRect(hEffects, &rect);       InvalidateRect(hEffects, &rect, FALSE);

	return 0;
}

unsigned int __stdcall CSCREENContentViewer::_TranslateThread(void *param)
{
	CSCREENContentViewer *pClass = (CSCREENContentViewer *) param;

	return pClass->TranslateThread();
}

int CSCREENContentViewer::TranslateThread( void )
{
	Translating = false;

	RECT r;

	while ( 1 )
	{
		HANDLE handles[ 2 ] = { hPoke, hTerminate };

		switch ( WaitForMultipleObjects( 2, handles, FALSE, 100 ) )
		{
		case WAIT_OBJECT_0:
			{
				Translating = true;

				GetClientRect( hWnd, &r );

				::SetWindowPos( hProgress, NULL,
					( ( r.right - r.left ) / 2 ) - ( ProgW / 2 ), ( ( r.bottom - r.top ) / 2 ) - ( ProgH / 2 ), ProgW, ProgH,
					SWP_NOREPOSITION | SWP_NOZORDER
				);

				ShowWindow( hProgress, SW_SHOW );

				Translate();
				
				if ( FirstTranslate )
				{
					PhysicalPalette = pXlator->GetPhysicalPalette();
					LogicalPalette  = pXlator->GetLogicalPalette( Mode );
					PhysicalColours = pXlator->GetPhysicalColours( );
				}

				FirstTranslate = false;

				ShowWindow( hProgress, SW_HIDE );

				Translating = false;

				UpdateWindow( hWnd );
			}

			break;

		case ( WAIT_OBJECT_0 + 1 ):
			return 0;

		default:
			break;
		}
	}

	return 0;
}

int CSCREENContentViewer::Translate( void ) {
	DWORD OriginalWidth  = 1;
	DWORD OriginalHeight = 1;

	if ( pXlator != nullptr )
	{
		CTempFile FileObj( Path );

		FileObj.Keep();

		GFXTranslateOptions opts;

		opts.bmi        = bmi;
		opts.FlashPhase = false;
		opts.Length     = Length;
		opts.Offset     = Offset;
		opts.ModeID     = Mode;
		opts.pGraphics  = (void **) &pixels1;

		opts.pLogicalPalette  = &LogicalPalette;
		opts.pPhysicalPalette = &PhysicalPalette;
		opts.pPhysicalColours = &PhysicalColours;

		pGlobalError->GlobalCode = NUTS_SUCCESS;

		pXlator->TranslateGraphics( opts, FileObj );

		if ( pGlobalError->GlobalCode != NUTS_SUCCESS )
		{
			NUTSError::Report( L"Read File", hWnd );
		}
		
		opts.pGraphics  = (void **) &pixels2;
		opts.FlashPhase = true;

		pGlobalError->GlobalCode = NUTS_SUCCESS;

		pXlator->TranslateGraphics( opts, FileObj );

		if ( pGlobalError->GlobalCode != NUTS_SUCCESS )
		{
			NUTSError::Report( L"Read File", hWnd );
		}
		
		OriginalWidth  = opts.bmi->bmiHeader.biWidth;
		OriginalHeight = opts.bmi->bmiHeader.biHeight;

		int DoneEffects = 0;

		if ( Effects & Effect_Antialias )
		{
			DoAntialias( &pixels1, bmi );
			DoAntialias( &pixels2, bmi );
		}

		if ( Effects & Effect_Snow )
		{
			DoSnow( &pixels1, bmi );
			DoSnow( &pixels2, bmi );

			DoneEffects++;
		}

		if ( Effects & Effect_Rainbow )
		{
			DoRainbow( &pixels1, bmi );
			DoRainbow( &pixels2, bmi );

			DoneEffects++;
		}

		if ( Effects & Effect_Ghosting )
		{
			DoGhosting( &pixels1, bmi );
			DoGhosting( &pixels2, bmi );

			DoneEffects++;
		}

		if ( Effects & Effect_Scanlines )
		{
			Upscale( &pixels1, bmi, false );
			Upscale( &pixels2, bmi, true  );

			DoScanlines( &pixels1, bmi );
			DoScanlines( &pixels2, bmi );

			DoneEffects++;
		}

		if ( Effects & Effect_CRTGlow )
		{
			DoCRTGlow( &pixels1, bmi );
			DoCRTGlow( &pixels2, bmi );

//			DoneEffects++;
		}

		if ( DoneEffects > 0 )
		{
			DoEffectMultiplier( DoneEffects, &pixels1, bmi );
			DoEffectMultiplier( DoneEffects, &pixels2, bmi );
		}
	}

	/* Figure out what our window size should be. Must be at least 640 */
	AspectRatio aspect = pXlator->GetAspect();

	DWORD pw = OriginalWidth;
	
	while ( pw < 640 )
	{
		pw += OriginalWidth;
	}

	double dpw = (double) pw;

	dpw /= (double) aspect.first;
	dpw *= (double) aspect.second;

	DWORD ph = (DWORD) (dpw);

	AspectWidth  = pw;
	AspectHeight = ph;

	int	brdx	= GetSystemMetrics(SM_CXBORDER);
	int	brdy	= GetSystemMetrics(SM_CYBORDER);
	int	tity	= GetSystemMetrics(SM_CYCAPTION);
	int	frmx	= GetSystemMetrics(SM_CXFIXEDFRAME);
	int	frmy	= GetSystemMetrics(SM_CYFIXEDFRAME);

	DisplayPref pref = pXlator->GetDisplayPref();

	DWORD ow = AspectWidth;
	DWORD oh = AspectHeight;
	
	wh = AspectHeight;
	ww = AspectWidth;

	if ( pref == DisplayNatural )
	{
		if ( ( ow >= bmi->bmiHeader.biWidth ) && ( oh >= bmi->bmiHeader.biHeight ) )
		{
			wh = bmi->bmiHeader.biHeight;
			ww = 640;
		}
	}

	::SetWindowPos( hWnd, NULL, 0, 0, ww + (2 * frmx), wh + (2 * frmy) + tity + 36, SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION );

	return 0;
}


int CSCREENContentViewer::DisplayImage( void )
{
	if ( Translating )
	{
		return 0;
	}

	HDC hDC = GetDC( hWnd );

	RECT	rect;

	GetClientRect(hWnd, &rect);

	SetStretchBltMode(hDC, 3);

	DWORD *pixels = (Flash) ? pixels2 : pixels1;

	DisplayPref pref = DisplayScaledScreen;

	if ( pXlator != nullptr )
	{
		pref = pXlator->GetDisplayPref();
	}

	DWORD ox = 0;
	DWORD oy = 36;
	DWORD ow = AspectWidth;
	DWORD oh = AspectHeight;

	if ( pref == DisplayNatural )
	{
		if ( ( ow > bmi->bmiHeader.biWidth ) && ( oh > bmi->bmiHeader.biHeight ) )
		{
			AspectRatio CAR = ScreenCompensatedIconRatio( AspectRatio( bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight ), pXlator->GetAspect(), ww, wh );

			ox = ( ww / 2 ) - (CAR.first / 2);
			oy = 36 + ( wh / 2 ) - (CAR.second / 2);
			ow = CAR.first;
			oh = CAR.second;
		}
	}

	StretchDIBits( hDC, ox, oy, ow, oh, 0, 0, bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight, pixels, bmi, DIB_RGB_COLORS, SRCCOPY ); 

	ReleaseDC(hWnd, hDC);

	return 0;
}

int CSCREENContentViewer::DoEffectsMenu( void )
{
	HMENU hPopup   = LoadMenu(hInst, MAKEINTRESOURCE(IDR_EFFECTS));
	HMENU hSubMenu = GetSubMenu(hPopup, 0);

	if ( Effects & Effect_Antialias ) { CheckMenuItem( hSubMenu, ID_EFFECTS_ANTIALIAS,  MF_BYCOMMAND | MF_CHECKED ); }	
	if ( Effects & Effect_CRTGlow )   { CheckMenuItem( hSubMenu, ID_EFFECTS_CRTGLOW,    MF_BYCOMMAND | MF_CHECKED ); }	
	if ( Effects & Effect_Scanlines ) { CheckMenuItem( hSubMenu, ID_EFFECTS_SCANLINES,  MF_BYCOMMAND | MF_CHECKED ); }	
	if ( Effects & Effect_Snow )      { CheckMenuItem( hSubMenu, ID_EFFECTS_SNOW,       MF_BYCOMMAND | MF_CHECKED ); }	
	if ( Effects & Effect_Ghosting )  { CheckMenuItem( hSubMenu, ID_EFFECTS_GHOSTING,   MF_BYCOMMAND | MF_CHECKED ); }	
	if ( Effects & Effect_Rainbow )   { CheckMenuItem( hSubMenu, ID_EFFECTS_RAINBOWING, MF_BYCOMMAND | MF_CHECKED ); }	
	
	RECT rect;

	GetWindowRect( hEffects, &rect );

	TrackPopupMenu(hSubMenu, 0, rect.left, rect.bottom, 0, hWnd, NULL);

	DestroyMenu(hPopup);

	return -1;
}

void CSCREENContentViewer::DoEffectMultiplier( int DoneEffects, DWORD **pPixels, BITMAPINFO *pBMI )
{
	DWORD TotalPixels = pBMI->bmiHeader.biHeight * pBMI->bmiHeader.biWidth;

	/* No new buffer needed here */
	DWORD *pixels = *pPixels;

	for ( DWORD p = 0; p<TotalPixels; p++ )
	{
		BYTE *pix = (BYTE *) &pixels[ p ];

		double b = (double) pix[ 0 ];
		double g = (double) pix[ 1 ];
		double r = (double) pix[ 2 ];

		double o = (double) DoneEffects * 0.6;
		o += 1.0;

		b = b * o; b = max(0,b); b = min(255.0,b);
		g = g * o; g = max(0,g); g = min(255.0,g);
		r = r * o; r = max(0,r); r = min(255.0,r);

		pix[ 0 ] = (BYTE) b;
		pix[ 1 ] = (BYTE) g;
		pix[ 2 ] = (BYTE) r;
	}
}

void CSCREENContentViewer::DoScanlines( DWORD **pPixels, BITMAPINFO *pBMI )
{
	DWORD pitch = pBMI->bmiHeader.biWidth * 4;
	BYTE *src  = (BYTE *) *pPixels;
	BYTE *dst  = (BYTE *) malloc( pBMI->bmiHeader.biWidth * pBMI->bmiHeader.biHeight * 4 );

	for (LONG y=0; y<pBMI->bmiHeader.biHeight; y++ )
	{
		if ( y & 1 )
		{
			memset( &dst[y * pitch], 0, pitch );
		}
		else
		{
			memcpy( &dst[y * pitch], &src[y * pitch], pitch );
		}
	}

	free( src );

	*pPixels = (DWORD *) dst;
}

inline float CSCREENContentViewer::rgb2luma( DWORD *pix )
{
	BYTE *pVec = (BYTE *) pix;

	float r = (float) pVec[ 2 ];
	float g = (float) pVec[ 1 ];
	float b = (float) pVec[ 0 ];

	return (float) ( sqrt( ( r * 0.299 ) + ( g * 0.587 ) + ( b * 0.114 ) ) );
}

#define EDGE_THRESHOLD_MIN 0.0312
#define EDGE_THRESHOLD_MAX 0.125

void CSCREENContentViewer::DoAntialias( DWORD **pPixels, BITMAPINFO *pBMI )
{
	// This algorithm copied from Simon Rodriguez, on this page:
	// http://blog.simonrodriguez.fr/articles/30-07-2016_implementing_fxaa.html

	// I only just about understand it. It's complicated. But it works pretty well, especially in BBC mode 1.

	DWORD pitch    = pBMI->bmiHeader.biWidth * 4;
	DWORD pixpitch = pBMI->bmiHeader.biWidth ;

	BYTE *src  = (BYTE *) *pPixels;
	BYTE *dst  = (BYTE *) malloc( pBMI->bmiHeader.biWidth * pBMI->bmiHeader.biHeight * 4 );

	float inverseSX = 1.0f / (float) pBMI->bmiHeader.biWidth;
	float inverseSY = 1.0f / (float) pBMI->bmiHeader.biHeight;

	for ( LONG y=1; y<pBMI->bmiHeader.biHeight - 1; y++ )
	{
		for ( LONG x=1; x<pBMI->bmiHeader.biWidth - 1; x++ )
		{
			DWORD pix_offset = pitch * y + x * 4;

			DWORD *ploc = (DWORD *) &src[ pix_offset ];

			float lumaCenter = rgb2luma( ploc );
			float lumaDown   = rgb2luma( ploc - pixpitch );
			float lumaLeft   = rgb2luma( ploc - 1 );
			float lumaRight  = rgb2luma( ploc + 1 );
			float lumaUp     = rgb2luma( ploc + pixpitch );

			float lumaMin    = min( lumaCenter, min(min(lumaDown,lumaUp),min(lumaLeft,lumaRight)));
			float lumaMax    = max( lumaCenter, max(max(lumaDown,lumaUp),max(lumaLeft,lumaRight)));

			float lumaRange  = lumaMax - lumaMin;

			DWORD fragColor;

			if ( lumaRange < max(EDGE_THRESHOLD_MIN,lumaMax*EDGE_THRESHOLD_MAX))
			{
				fragColor = *ploc;
			}
			else
			{
				float lumaDownLeft  = rgb2luma( ( ploc - pixpitch ) - 1 );
				float lumaUpRight   = rgb2luma( ( ploc + pixpitch ) + 1 );
				float lumaUpLeft    = rgb2luma( ( ploc + pixpitch ) - 1 );
				float lumaDownRight = rgb2luma( ( ploc - pixpitch ) + 1 );

				float lumaDownUp    = lumaDown + lumaUp;
				float lumaLeftRight = lumaLeft = lumaRight;

				float lumaLeftCorners  = lumaDownLeft  + lumaUpLeft;
				float lumaDownCorners  = lumaDownLeft  + lumaDownRight;
				float lumaRightCorners = lumaDownRight + lumaUpRight;
				float lumaUpCorners    = lumaUpRight   + lumaUpLeft;
				
				float edgeHorizontal =  abs(-2.0f * lumaLeft + lumaLeftCorners) + abs(-2.0f * lumaCenter + lumaDownUp )   * 2.0f + abs(-2.0f * lumaRight + lumaRightCorners);
				float edgeVertical   =  abs(-2.0f * lumaUp + lumaUpCorners)     + abs(-2.0f * lumaCenter + lumaLeftRight) * 2.0f + abs(-2.0f * lumaDown + lumaDownCorners);

				bool isHorizontal = (edgeHorizontal >= edgeVertical );

				float luma1 = isHorizontal ? lumaDown : lumaLeft;
				float luma2 = isHorizontal ? lumaUp   : lumaRight;

				float gradient1 = luma1 - lumaCenter;
				float gradient2 = luma2 - lumaCenter;

				bool is1Steepest = abs(gradient1) >= abs(gradient2);

				float gradientScaled = 0.25f * max( abs(gradient1), abs(gradient2) );

				float stepLength = isHorizontal ? inverseSY : inverseSX;

				float lumaLocalAverage = 0.0;

				if ( is1Steepest )
				{
					stepLength = -stepLength;
					lumaLocalAverage = 0.5f * ( luma1 + lumaCenter );
				}
				else
				{
					lumaLocalAverage = 0.5f * ( luma2 + lumaCenter);
				}

				float currentX = (float) x; currentX += stepLength * 0.5f;
				float currentY = (float) y; currentY += stepLength * 0.5f;

				float offsetx = (isHorizontal) ? inverseSX : 0;
				float offsety = (isHorizontal) ? 0 : inverseSY;

				float uv1x = currentX - offsetx;
				float uv1y = currentY - offsety;

				float uv2x = currentX + offsetx;
				float uv2y = currentY + offsety;

				float lumaEnd1 = rgb2luma( (DWORD *) &src[ (DWORD) ( pitch * uv1y + uv1x * 4 ) ] );
				float lumaEnd2 = rgb2luma( (DWORD *) &src[ (DWORD) ( pitch * uv2y + uv2x * 4 ) ] );

				bool reached1    = abs(lumaEnd1) >= gradientScaled;
				bool reached2    = abs(lumaEnd2) >= gradientScaled;
				bool reachedBoth = reached1 && reached2;

				if ( !reached1) { uv1x -= offsetx; uv1y -= offsety; }
				if ( !reached2) { uv2x += offsetx; uv2y += offsety; }

				float Quality[13] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0 };

				if (!reachedBoth)
				{
					for (int i=2; i<12; i++) {
						if (!reached1) {
							lumaEnd1 = rgb2luma( (DWORD *) &src[ (DWORD) ( pitch * uv1y + uv1x * 4 ) ] );
							lumaEnd1 = lumaEnd1 - lumaLocalAverage;
						}

						if (!reached2) {
							lumaEnd2 = rgb2luma( (DWORD *) &src[ (DWORD) ( pitch * uv2y + uv2x * 4 ) ] );
							lumaEnd2 = lumaEnd2 - lumaLocalAverage;
						}

						reached1    = abs(lumaEnd1) >= gradientScaled;
						reached2    = abs(lumaEnd2) >= gradientScaled;
						reachedBoth = reached1 && reached2;

						if (!reached1) { uv1x -= offsetx * Quality[i]; uv1y -= offsety * Quality[i]; }
						if (!reached2) { uv2x += offsetx * Quality[i]; uv2y += offsety * Quality[i]; }

						if ( reachedBoth ) { break; }
					}
				}

				float distance1 = isHorizontal ? ( (float) x - uv1x ) : ( (float) y - uv1y );
				float distance2 = isHorizontal ? ( uv2x - (float) x ) : ( uv2y - (float) y );

				bool isDirection1 = distance1 < distance2;

				float distanceFinal = min(distance1, distance2);

				float edgeThickness = (distance1 + distance2);

				float pixelOffset = -distanceFinal / edgeThickness + 0.5f;

				bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;

				bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0f) != isLumaCenterSmaller;

				float finalOffset = correctVariation ? pixelOffset : 0.0f;

				float lumaAverage = (1.0f/12.0f) * (2.0f * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);

				float subPixelOffset1 = abs( lumaAverage - lumaCenter) / lumaRange;

				if ( subPixelOffset1 < 0.0f ) { subPixelOffset1 = 0.0f; }
				if ( subPixelOffset1 > 1.0f ) { subPixelOffset1 = 1.0f; }

				float subPixelOffset2 = ( -2.0f * subPixelOffset1 + 3.0f ) * subPixelOffset1 * subPixelOffset1;

				float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * 0.75f;

				finalOffset = max( finalOffset, subPixelOffsetFinal );

				float finalX = (float) x;
				float finalY = (float) y;

				if ( isHorizontal )
				{
					finalY += finalOffset + stepLength;
				}
				else
				{
					finalX += finalOffset + stepLength;
				}

				finalOffset = 1.0f - finalOffset;

				DWORD dfx = (DWORD) finalX;
				DWORD dfy = (DWORD) finalY;

				BYTE *finalLoc = &src[ dfy * pitch + dfx * 4 ];

				float or = (float) finalLoc[ 2 ];
				float og = (float) finalLoc[ 1 ];
				float ob = (float) finalLoc[ 0 ];

				BYTE nr = (BYTE) ( or * finalOffset );
				BYTE ng = (BYTE) ( og * finalOffset );
				BYTE nb = (BYTE) ( ob * finalOffset );

				fragColor = ( nr << 16 ) | ( ng << 8 ) | nb;
			}
			
			DWORD *targetPixel = (DWORD *) &dst[ y * pitch + x * 4 ];

			*targetPixel = fragColor;
		}
	}

	free( src );

	*pPixels = (DWORD *) dst;
}

void CSCREENContentViewer::Upscale( DWORD **pPixels, BITMAPINFO *pBMI, bool UpdateBMI )
{
	DWORD pitch    = pBMI->bmiHeader.biWidth * 4;
	DWORD pixpitch = pBMI->bmiHeader.biWidth ;

	DWORD newX = 1280;

	AspectRatio Aspect = pXlator->GetAspect();

	DWORD newY = ( 1280 / Aspect.first ) * Aspect.second;

	DWORD oldX = pBMI->bmiHeader.biWidth;
	DWORD oldY = pBMI->bmiHeader.biHeight;

	BYTE *src  = (BYTE *) *pPixels;
	BYTE *dst  = (BYTE *) malloc( newX * newY * 4 );

	float rx = (float) oldX / (float) newX;
	float ry = (float) oldY / (float) newY;

	DWORD np = newX * 4;

	for ( DWORD y=0; y<newY; y++ )
	{
		for ( DWORD x=0; x<newX; x++ )
		{
			float tx = (float) x;
			float ty = (float) y;

			DWORD srcX = (DWORD) ( tx * rx );
			DWORD srcY = (DWORD) ( ty * ry );

			DWORD *pSrc = (DWORD *) &src[ srcY * pitch + srcX * 4 ];
			DWORD *pDst = (DWORD *) &dst[ y * np + x * 4 ];

			*pDst = *pSrc;
		}
	}

	free( src );

	*pPixels = (DWORD *) dst;

	if ( UpdateBMI )
	{
		pBMI->bmiHeader.biWidth     = newX;
		pBMI->bmiHeader.biHeight    = newY;
		pBMI->bmiHeader.biSizeImage = newX * newY * 4;
	}
}

int CSCREENContentViewer::DoCopyImage( bool Save )
{
	BITMAPFILEHEADER bmf;

	bmf.bfType = 'MB'; // !sdrawkcab si di ehT
	bmf.bfSize = sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFO ) + bmi->bmiHeader.biSizeImage;

	bmf.bfReserved1 = 0;
	bmf.bfReserved2 = 0;

	bmf.bfOffBits = sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFO );

	BYTE *pData = (BYTE *) malloc( bmf.bfSize );

	memcpy( pData, &bmf, sizeof( BITMAPFILEHEADER ) );
	memcpy( &pData[sizeof(BITMAPFILEHEADER)], bmi, sizeof(BITMAPINFO) );

	DWORD PixelOffset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO);

	if ( !Save ) { PixelOffset -= 4; }

	if ( Flash )
	{
		memcpy( &pData[ PixelOffset ], pixels1, bmi->bmiHeader.biSizeImage );
	}
	else
	{
		memcpy( &pData[ PixelOffset ], pixels1, bmi->bmiHeader.biSizeImage );
	}

	if ( !Save )
	{
		HGLOBAL hMem = NULL;

		if ( OpenClipboard( hWnd ) != NULL )
		{
			EmptyClipboard();

			hMem = GlobalAlloc( GMEM_MOVEABLE, bmf.bfSize - sizeof( BITMAPFILEHEADER ) );
		
			if ( hMem != NULL )
			{
				void *pPtr = GlobalLock( hMem );

				if ( pPtr != nullptr )
				{
					memcpy( pPtr, &pData[sizeof(BITMAPFILEHEADER)], bmf.bfSize - sizeof(BITMAPFILEHEADER) );

					GlobalUnlock( hMem );
				}

				SetClipboardData( CF_DIB, hMem );
			}

			CloseClipboard();
		}

		if ( hMem != NULL )
		{
			GlobalFree( hMem );
		}
	}
	else
	{
		std::wstring filename;

		if ( SaveFileDialog( hWnd, filename, L"Windows Bitmap", L"BMP", L"Save Translated Image" ) )
		{
			FILE	*fFile;

			_wfopen_s( &fFile, filename.c_str(), L"wb" );

			if (!fFile) {
				MessageBox( hWnd, L"The image file could not be saved", L"File Save Error", MB_ICONEXCLAMATION | MB_ICONERROR | MB_OK );

				return -1;
			}

			fwrite( pData, 1, bmf.bfSize, fFile );

			fclose( fFile );
		}
	}

	free( pData );

	return 0;
}

int CSCREENContentViewer::DoPrintImage( void )
{
	PRINTDLG printDlgInfo = {0};

	printDlgInfo.lStructSize = sizeof( printDlgInfo );
	printDlgInfo.Flags = PD_RETURNDC | PD_NOSELECTION | PD_NOPAGENUMS;

	if ( PrintDlg( &printDlgInfo ) )
	{
		/* We're on */
		HDC hDC = printDlgInfo.hDC;

		int pw = GetDeviceCaps( hDC, HORZRES );
		int ph = GetDeviceCaps( hDC, VERTRES );

		int ox;
		int oy;
		int ow;
		int oh;
		int s;

		if ( pw > ph )
		{
			/* Landscape mode */
			s = ph / AspectHeight;

			ow = AspectWidth  * s;
			oh = AspectHeight * s;

			ox = (pw / 2) - (ow/2);
			oy = 0;
		}
		else
		{
			/* Portrait */
			s = pw / AspectWidth;

			ow = AspectWidth  * s;
			oh = AspectHeight * s;

			ox = 0;
			oy = (ph/2) - (oh/2);
		}

		DOCINFO docInfo;

		ZeroMemory( &docInfo, sizeof( DOCINFO ) );
		docInfo.cbSize =sizeof( DOCINFO );
		
		StartDoc( hDC, &docInfo );
		StartPage( hDC );

		SetStretchBltMode(hDC, 3);

		DWORD *pixels = (Flash) ? pixels2 : pixels1;

		StretchDIBits(hDC, ox, oy, ow, oh, 0,0, bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight, pixels, bmi, DIB_RGB_COLORS, SRCCOPY);

		EndPage( hDC );
		EndDoc( hDC );

		DeleteDC( hDC );
	}

	return 0;
}

void CSCREENContentViewer::DoSnow( DWORD **pPixels, BITMAPINFO *pBMI )
{
	DWORD TotalPixels = pBMI->bmiHeader.biHeight * pBMI->bmiHeader.biWidth;

	/* No new buffer needed here */
	DWORD *pixels = *pPixels;

	for ( DWORD p = 0; p<TotalPixels; p++ )
	{
		BYTE *pix = (BYTE *) &pixels[ p ];

		double b = (double) pix[ 0 ];
		double g = (double) pix[ 1 ];
		double r = (double) pix[ 2 ];

		double o = (double) ( rand() % 100 ); o -= 50.0;

		b = ( b + o ) / 2.0; b = max(0,b); b = min(255.0,b);
		g = ( g + o ) / 2.0; g = max(0,g); g = min(255.0,g);
		r = ( r + o ) / 2.0; r = max(0,r); r = min(255.0,r);

		pix[ 0 ] = (BYTE) b;
		pix[ 1 ] = (BYTE) g;
		pix[ 2 ] = (BYTE) r;
	}
}

#define GLOW_THRESHOLD 0x77
#define GLOW_RADIUS    4

void CSCREENContentViewer::AddGlowSpot( BYTE *out, LONG px, LONG py, LONG pix, LONG r, LONG t, LONG mx, LONG my )
{
	for ( long gy = 0 - r; gy < r; gy++ )
	{
		// Use pythagoras to get this line length
		long lx = (long) sqrt( ( (double) r * (double) r ) + ( (double) gy * (double) gy ) );

		long yp = py + gy;
		
		for ( long gx = 0 - lx; gx < lx; gx++ )
		{
			long xp = px + gx;

			if ( ( xp != px ) && ( yp != py ) ) // Don't fill in the center spot
			{
				if ( ( xp > 0 ) && ( xp < mx ) && ( yp > 0 ) && ( yp < my ) )
				{
					// Now we need to reverse the radius - pythagoras again.
					long xr = (long) sqrt( ( (double) gx * (double) gx ) + ( (double) gy * (double) gy ) );

					double tt = (double) t / pow( 2.0, (double) xr );

					DWORD offset = ( yp * (mx * 4 ) ) + ( xp * 4 ) + pix;

					if ( out[ offset ] < tt )
					{
						out[ offset ] = (BYTE) ( ( (double) out[offset] + (double) tt ) / 2.0 );
					}
				}
			}
		}
	}
}

void CSCREENContentViewer::DoCRTGlow( DWORD **pPixels, BITMAPINFO *pBMI )
{
	DWORD Pitch  = pBMI->bmiHeader.biWidth * 4;
	BYTE *pixels = (BYTE *) *pPixels;
	BYTE *out    = (BYTE *) malloc( pBMI->bmiHeader.biSizeImage );

	memcpy( out, pixels, pBMI->bmiHeader.biSizeImage );

	for ( LONG py=0; py<pBMI->bmiHeader.biHeight; py++ )
	{
		for ( LONG px=0; px<pBMI->bmiHeader.biWidth; px++ )
		{
			for ( LONG pix=0; pix<3; pix++ ) // B, G, R
			{
				DWORD offset = ( py * Pitch ) + ( px * 4 ) + pix;

				if ( pixels[ offset ] > GLOW_THRESHOLD )
				{
					AddGlowSpot( out, px, py, pix, GLOW_RADIUS, GLOW_THRESHOLD, pBMI->bmiHeader.biWidth, pBMI->bmiHeader.biHeight );
				}
			}
		}
	}

	memcpy( pixels, out, pBMI->bmiHeader.biSizeImage );

	free( (void *) out );
}

void CSCREENContentViewer::DoGhosting( DWORD **pPixels, BITMAPINFO *pBMI )
{
	DWORD Pitch  = pBMI->bmiHeader.biWidth * 4;
	BYTE *pLine  = (BYTE *) malloc( Pitch * 2 );
	BYTE *pixels = (BYTE *) *pPixels;

	for ( LONG py = 0; py<pBMI->bmiHeader.biHeight; py++ )
	{
		BYTE *pSrc = (BYTE *) &pixels[ py * Pitch ];

		memcpy( pLine, pSrc, Pitch );

		for ( LONG px=0; px<pBMI->bmiHeader.biWidth; px++ )
		{
			BYTE *pix = &pixels[ (py * Pitch) + (px * 4) ];

			WORD od = 4;
			WORD of = (WORD) ( px + od );

			BYTE b = pix[ 0 ];
			BYTE g = pix[ 1 ];
			BYTE r = pix[ 2 ];

			while ( ( of < (Pitch / 2 ) ) && ( od > 0 ) )
			{
				BYTE *gho = (BYTE *) &pLine[ of * 4 ];

				b >>= 1;
				g >>= 1;
				r >>= 1;

				WORD b2 = (WORD) b + (WORD) gho[ 0 ];
				WORD g2 = (WORD) g + (WORD) gho[ 1 ];
				WORD r2 = (WORD) r + (WORD) gho[ 2 ];

				gho[ 0 ] = (BYTE) ( b2 >> 1 );
				gho[ 1 ] = (BYTE) ( g2 >> 1 );
				gho[ 2 ] = (BYTE) ( r2 >> 1 );

				od >>= 1;

				of += od;
			}
		}		

		memcpy( pSrc, pLine, Pitch );


		for ( LONG px=0; px<pBMI->bmiHeader.biWidth; px++ )
		{
			BYTE *pix = &pixels[ (py * Pitch) + (px * 4) ];
			BYTE *gho = &pLine[ px * 4 ];

			WORD b = pix[ 0 ];
			WORD g = pix[ 1 ];
			WORD r = pix[ 2 ];

			b += (WORD) gho[ 0 ];
			g += (WORD) gho[ 1 ];
			r += (WORD) gho[ 2 ];

			pix[ 0 ] = (BYTE) b; //>> 1;
			pix[ 1 ] = (BYTE) g; //>> 1;
			pix[ 2 ] = (BYTE) r; //>> 1;
		}
	}

	free( pLine );
}

void CSCREENContentViewer::DoRainbow( DWORD **pPixels, BITMAPINFO *pBMI )
{
	DWORD TotalPixels = pBMI->bmiHeader.biHeight * pBMI->bmiHeader.biWidth;
	
	DWORD Pitch = pBMI->bmiHeader.biWidth * 4;

	BYTE *pLine  = (BYTE *) malloc( Pitch * 2 );
	BYTE *pixels = (BYTE *) *pPixels;

	ZeroMemory( pLine, Pitch * 2 );

	/* I can make a rainbow... */
	BYTE ph = 0;
	BYTE v  = 0;

	for ( LONG px=0; px < pBMI->bmiHeader.biWidth * 2; px++ )
	{
		BYTE *pix = &pLine[ px * 4 ];
		
		switch (ph)
		{
		case 0: // Red => Yellow
			pix[ 2 ] = 255;
			pix[ 1 ] = v+=10;
			pix[ 0 ] = 0;

			if ( v >= 250 ) { ph++; }

			break;
		
		case 1: // Yellow => Green
			pix[ 2 ] = v-=10;
			pix[ 1 ] = 255;
			pix[ 0 ] = 0;

			if ( v <= 10 ) { ph++; }

			break;

		case 2: // Green => Cyan
			pix[ 2 ] = 0;
			pix[ 1 ] = 255;
			pix[ 0 ] = v+=10;

			if ( v >= 250 ) { ph++; }

			break;

		case 3: // Cyan => Blue
			pix[ 2 ] = 0;
			pix[ 1 ] = v-=10;
			pix[ 0 ] = 255;

			if ( v <= 10 ) { ph++; }

			break;

		case 4: // Blue => Magenta

			pix[ 2 ] = v+=10;
			pix[ 1 ] = 0;
			pix[ 0 ] = 255;

			if ( v >= 250 ) { ph++; }

			break;

		case 5: // Magenta => Red
			pix[ 2 ] = 255;
			pix[ 1 ] = 0;
			pix[ 0 ] = v-=10;

			if ( v <= 10 ) { ph = 0; }

			break;
		}
	}

	for ( LONG py = 0; py < pBMI->bmiHeader.biHeight; py++ )
	{
		for ( LONG px = 0; px < pBMI->bmiHeader.biWidth; px++ )
		{
			BYTE *pSrc = &pixels[ (py * Pitch) + (px * 4) ];	
			BYTE *pDest = &pLine[ (px + (py & 3 ) ) * 4 ];

			WORD rr = (WORD) pDest[ 2 ];
			WORD rg = (WORD) pDest[ 1 ];
			WORD rb = (WORD) pDest[ 0 ];

			rr >>= 4;
			rg >>= 4;
			rb >>= 4;

			WORD r = pSrc[ 2 ]; r += rr + 32;
			WORD g = pSrc[ 1 ]; g += rg + 32;
			WORD b = pSrc[ 0 ]; b += rb + 32;

			pSrc[ 2 ] = r >> 1;
			pSrc[ 1 ] = g >> 1;
			pSrc[ 0 ] = b >> 1;
		}
	}

	free( pLine );
}

