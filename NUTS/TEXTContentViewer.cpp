#include "StdAfx.h"
#include "TEXTContentViewer.h"
#include "Plugins.h"
#include "NUTSError.h"
#include "Preference.h"
#include "FileDialogs.h"
#include "BuiltIns.h"
#include "libfuncs.h"

#include <richedit.h>
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>
#include <process.h>

#include "resource.h"

std::map<HWND,CTEXTContentViewer *> CTEXTContentViewer::viewers;

bool CTEXTContentViewer::WndClassReg = false;

#define ProgW 128
#define ProgH 18

LRESULT CALLBACK CTEXTContentViewer::TEXTViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( viewers.find( hWnd ) != viewers.end() )
	{
		return viewers[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

CTEXTContentViewer::CTEXTContentViewer( CTempFile &FileObj, TXIdentifier TUID )
{
	if ( !WndClassReg )
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= CTEXTContentViewer::TEXTViewerProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInst;
		wcex.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(IDI_BASIC));
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
		wcex.lpszMenuName	= NULL; // MAKEINTRESOURCE(IDC_NUTS);
		wcex.lpszClassName	= L"NUTS Text Content Renderer";
		wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_BASIC));

		RegisterClassEx(&wcex);

		WndClassReg = true;
	}

	XlatorID = TUID;

	pXlator = (TEXTTranslator *) NUTSBuiltIns.LoadTranslator( TUID );

	if ( pXlator == nullptr )
	{
		pXlator  = (TEXTTranslator *) FSPlugins.LoadTranslator( TUID );
	}

	TempPath = FileObj.Name();

	FileObj.Keep();

	pTextBuffer = nullptr;

	Translating      = true;
	Retranslate      = false;
	hTranslateThread = NULL;
	hStopEvent       = CreateEvent( NULL, TRUE, FALSE, NULL );
	hProgress        = NULL;

	ResizeTime       = 0;

	pChanger = nullptr;
	pSave    = nullptr;
	pCopy    = nullptr;
	pPrint   = nullptr;
}

CTEXTContentViewer::~CTEXTContentViewer(void) {
	if ( hTranslateThread != NULL )
	{
		SetEvent( hStopEvent );

		if ( WaitForSingleObject( hTranslateThread, 500 ) == WAIT_TIMEOUT )
		{
			TerminateThread( hTranslateThread, 500 );
		}

		CloseHandle( hTranslateThread );
		CloseHandle( hStopEvent );
	}

	KillTimer( hWnd, 0x7e7 );

	NixWindow( hCopy );
	NixWindow( hSave );
	NixWindow( hPrint );
	NixWindow( hChanger );
	NixWindow( hProgress );

	NixWindow( hWnd );

	delete pTextArea;
	delete pStatusBar;

	if ( pTextBuffer != nullptr )
	{
		free( pTextBuffer );
	}

	if ( pChanger != nullptr )
	{
		delete pChanger;
		delete pSave;
		delete pCopy;
		delete pPrint;
	}
}

unsigned int __stdcall CTEXTContentViewer::TranslateThread(void *param)
{
	CTEXTContentViewer *pClass = (CTEXTContentViewer *) param;

	pClass->Translate();

	return 0;
}

int CTEXTContentViewer::Create(HWND Parent, HINSTANCE hInstance, int x, int w, int h) {
	DWORD ww = Preference( L"TextTranslatorWidth",  (DWORD) 800 );
	DWORD wh = Preference( L"TextTranslatorHeight", (DWORD) 500 );

	hWnd = CreateWindowEx(
		NULL,
		L"NUTS Text Content Renderer",
		L"NUTS Text Content Renderer",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
		CW_USEDEFAULT, 0, ww, wh,
		GetDesktopWindow(), NULL, hInstance, NULL
	);

	CTEXTContentViewer::viewers[ hWnd ] = this;

	ParentWnd = Parent;

	RECT rect;

	GetClientRect(hWnd, &rect);

	FontNum  = 0;
	FontList = FSPlugins.FontListForEncoding( FSEncodingID );

	pTextArea = new EncodingTextArea( hWnd, 0, 32, rect.right - rect.left, rect.bottom - rect.top - 32 - 26 );

	ShowWindow( pTextArea->hWnd, SW_HIDE );

	pStatusBar = new EncodingStatusBar( hWnd );

	pStatusBar->AddPanel( 0, 100, (BYTE *) "PC437", FONTID_PC437, PF_RIGHT | PF_SIZER | PF_SPARE );

	hProgress = CreateWindowEx(
		0, PROGRESS_CLASS, NULL,
		WS_CHILD | WS_VISIBLE,
		0, 0, ProgW, ProgH,
		hWnd, NULL, hInst, NULL
	);

	SendMessage( hProgress, PBM_SETRANGE32, 0, 100 );

	CreateToolbar();

	DoResize();

	opts.RetranslateOnResize = false;

	hTranslateThread = (HANDLE) _beginthreadex(NULL, NULL, TranslateThread, this, NULL, (unsigned int *) &dwthreadid);

	SetFocus( pChanger->hWnd );

	SetActiveWindow( hWnd );
	SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE );

	UpdateWindow( hWnd );

	SetTimer( hWnd, 0x7e7, 1000, NULL );

	SetFocus( pChanger->hWnd );

	return 0;
}

void CTEXTContentViewer::BeginTranslate( void )
{
	Retranslate = false;

	ShowWindow( hProgress, TRUE );

	hTranslateThread = (HANDLE) _beginthreadex(NULL, NULL, TranslateThread, this, NULL, (unsigned int *) &dwthreadid);
}

LRESULT	CTEXTContentViewer::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	switch ( message )
	{
		case WM_ACTIVATE:
			if ( wParam == 0 )
			{
				hActiveWnd = NULL;
			}
			else
			{
				hActiveWnd = hWnd;
			}
			return FALSE;

		case WM_PAINT:
			{
				PaintToolBar();
			}
		break;

		case WM_TIMER:
			RetranslateCheck();
			break;

		case WM_SIZE:
			{
				DoResize();
			}
			break;

		case WM_DESTROY:
			{
				viewers.erase( hWnd );

				delete this;
			}
			break;

		case WM_TEXT_PROGRESS:
			{
				SendMessage( hProgress, PBM_SETPOS, wParam, 0 );
			}
			break;

		case WM_COMMAND:
			{
				if ( lParam == (LPARAM) pChanger->hWnd )
				{
					if ( !Translating )
					{
						FontNum = ( FontNum + 1 ) % FontList.size();

						pTextArea->SetFont( FontList[ FontNum ] );
						pStatusBar->SetPanelText( 0, FONTID_PC437, (BYTE *) AString( (WCHAR *) FSPlugins.FontName( FontList[ FontNum ] ).c_str() ) );
					}
				}

				if ( lParam == (LPARAM) pSave->hWnd )
				{
					DoSave();
				}

				if ( lParam == (LPARAM) pPrint->hWnd )
				{
					DoPrint();
				}

				if ( lParam == (LPARAM) pCopy->hWnd )
				{
					pTextArea->CopySelection();
				}
			}
			break;
	}

	return DefWindowProc( hWnd, message, wParam, lParam);
}

void CTEXTContentViewer::Translate( void )
{
	opts.EncodingID  = ENCODING_ASCII;
	opts.pTextBuffer = &pTextBuffer;
	opts.ProgressWnd = hWnd;
	opts.hStop       = hStopEvent;

	RECT r;

	GetClientRect( hWnd, &r );

	WORD width = r.right - r.left;

	if ( width > 24 ) { width -= 24; }
	
	opts.CharacterWidth = width / 8;

	opts.LinePointers.clear();

	if ( pXlator != nullptr )
	{
		CTempFile FileObj( TempPath );

		FileObj.Keep();

		pGlobalError->GlobalCode = NUTS_SUCCESS;

		pXlator->TranslateText( FileObj, &opts );

		if ( pGlobalError->GlobalCode != NUTS_SUCCESS )
		{
			NUTSError::Report( L"Read File", hWnd );
		}

		if ( XlatorID == TUID_TEXT )
		{
			opts.EncodingID = FSEncodingID;
		}

		FontList = FSPlugins.FontListForEncoding( FSEncodingID );

		pTextArea->SetTextBody( FontList[ FontNum ], *opts.pTextBuffer, opts.TextBodyLength, opts.LinePointers );

		pStatusBar->SetPanelText( 0, FONTID_PC437, (BYTE *) AString( (WCHAR *) FSPlugins.FontName( FontList[ FontNum ] ).c_str() ) );

		pTextBuffer = *opts.pTextBuffer;

		ShowWindow( hProgress, SW_HIDE );
		ShowWindow( pTextArea->hWnd, SW_SHOW );	
	}

	Translating = false;
	Retranslate = false;
}

void CTEXTContentViewer::DoResize()
{
	RECT r;

	GetClientRect( hWnd, &r );

	pTextArea->DoResize( r.right - r.left, r.bottom - r.top - 32 - 26 );
	
	pStatusBar->NotifyWindowSizeChanged();

	SetWindowPos( hProgress, NULL,
		( ( r.right - r.left ) / 2 ) - ( ProgW / 2 ), ( ( r.bottom - r.top ) / 2 ) - ( ProgH / 2 ),
		ProgW, ProgH, SWP_NOREPOSITION | SWP_NOZORDER
	);

	/* Store the size as prefs - need to account for title bar */
	GetWindowRect( hWnd, &r );

	Preference( L"TextTranslatorWidth" )  = (DWORD) ( r.right - r.left );
	Preference( L"TextTranslatorHeight" ) = (DWORD) ( r.bottom - r.top );

	Retranslate = true;

	ResizeTime = GetTickCount();
}

void CTEXTContentViewer::RetranslateCheck( void )
{
	if ( ( !Retranslate ) || ( Translating ) || ( !opts.RetranslateOnResize ) )
	{
		return;
	}

	if ( hTranslateThread != NULL )
	{
		if ( WaitForSingleObject( hTranslateThread, 500 ) == WAIT_TIMEOUT )
		{
			TerminateThread( hTranslateThread, 500 );

			CloseHandle( hTranslateThread );
		}

		hTranslateThread = NULL;
	}

	DWORD now = GetTickCount();

	if ( ResizeTime != 0 )
	{
		DWORD diff = now - ResizeTime;

		if ( ResizeTime > now )
		{
			diff = ( 0 - ResizeTime ) + now;
		}

		if ( diff > 250 )
		{
			BeginTranslate();
		}
	}
}

int CTEXTContentViewer::CreateToolbar( void ) {

	int		h = 4;

	pChanger = new IconButton( hWnd, h, 4, LoadIcon( hInst, MAKEINTRESOURCE( IDI_FONTSWITCH ) ) ); h+= 28;

	pChanger->SetTip( L"Change the font used for rendering the text" );


	pCopy = new IconButton( hWnd, h, 4, LoadIcon( hInst, MAKEINTRESOURCE( IDI_COPYFILE ) ) ); h+= 28;
	
	pCopy->SetTip( L"Copy the text to the clipboard (may lose encoding)" );


	pSave = new IconButton( hWnd, h, 4, LoadIcon( hInst, MAKEINTRESOURCE( IDI_SAVE ) ) ); h+= 28;
	
	pSave->SetTip( L"Save the text to a file (may lose encoding)" );


	pPrint = new IconButton( hWnd, h, 4, LoadIcon( hInst, MAKEINTRESOURCE( IDI_PRINT ) ) ); h+= 28;

	pPrint->SetTip( L"Print the text in the current font" );

	return 0;
}

int CTEXTContentViewer::PaintToolBar( void )
{
	RECT	rect;
	HDC		hDC;

	hDC	= GetDC(hWnd);

	GetClientRect(hWnd, &rect);

	rect.bottom	= rect.top + 32;

	DrawEdge(hDC, &rect, EDGE_RAISED, BF_RECT);

	ReleaseDC(hWnd, hDC);

	return 0;
}

void CTEXTContentViewer::DoSave( void )
{
	FILE *fFile;

	std::wstring filename;

	if ( SaveFileDialog( hWnd, filename, L"Text File", L"TXT", L"Save Text Content" ) )
	{
		_wfopen_s( &fFile, filename.c_str(), L"wb" );

		if ( !fFile )
		{
			MessageBox( hWnd, L"Unable to save text file", L"NUTS Text Processor", MB_ICONEXCLAMATION | MB_OK );

			return;
		}

		pTextArea->SaveText( fFile );

		fclose( fFile );

		MessageBox( hWnd, L"Text file has been saved.\n\nNo translation of the text encoding has been performed - the text may not display correctly.", L"NUTS Text Processor", MB_ICONINFORMATION | MB_OK );
	}
}

void CTEXTContentViewer::DoPrint( void )
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

		DOCINFO docInfo;

		ZeroMemory( &docInfo, sizeof( DOCINFO ) );
		docInfo.cbSize =sizeof( DOCINFO );
		
		StartDoc( hDC, &docInfo );

		pTextArea->PrintText( hDC, pw, ph );

		EndDoc( hDC );

		DeleteDC( hDC );
	}
}
