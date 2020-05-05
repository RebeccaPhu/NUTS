#include "StdAfx.h"
#include "TEXTContentViewer.h"
#include "Plugins.h"
#include "NUTSError.h"
#include "Preference.h"

#include <richedit.h>
#include <CommCtrl.h>
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

CTEXTContentViewer::CTEXTContentViewer( CTempFile &FileObj, DWORD TUID )
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
		wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
		wcex.lpszMenuName	= NULL; // MAKEINTRESOURCE(IDC_NUTS);
		wcex.lpszClassName	= L"NUTS Text Content Renderer";
		wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_BASIC));

		RegisterClassEx(&wcex);

		WndClassReg = true;
	}

	XlatorID = TUID;
	pXlator  = (TEXTTranslator *) FSPlugins.LoadTextTranslator( TUID );
	TempPath = FileObj.Name();

	FileObj.Keep();

	FSEncodingID = ENCODING_ASCII;

	pTextBuffer = nullptr;

	Translating      = true;
	hTranslateThread = NULL;
	hStopEvent       = CreateEvent( NULL, TRUE, FALSE, NULL );
	hProgress        = NULL;
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

	NixWindow( hProgress );

	DestroyWindow( hWnd );

	delete pTextArea;

	if ( pTextBuffer != nullptr )
	{
		free( pTextBuffer );
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
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_VISIBLE,
		CW_USEDEFAULT, 0, ww, wh,
		Parent, NULL, hInstance, NULL
	);

	CTEXTContentViewer::viewers[ hWnd ] = this;

	ParentWnd = Parent;

	RECT rect;

	GetClientRect(hWnd, &rect);

	pTextArea = new EncodingTextArea( hWnd, 0, 0, rect.right - rect.left, rect.bottom - rect.top );

	ShowWindow( pTextArea->hWnd, SW_HIDE );

	hProgress = CreateWindowEx(
		0, PROGRESS_CLASS, NULL,
		WS_CHILD | WS_VISIBLE,
		0, 0, ProgW, ProgH,
		hWnd, NULL, hInst, NULL
	);

	SendMessage( hProgress, PBM_SETRANGE32, 0, 100 );

	DoResize();

	hTranslateThread = (HANDLE) _beginthreadex(NULL, NULL, TranslateThread, this, NULL, (unsigned int *) &dwthreadid);

	ShowWindow(hWnd, TRUE);
	UpdateWindow( hWnd );

	return 0;
}

LRESULT	CTEXTContentViewer::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	switch ( message )
	{
		case WM_PAINT:
			{
				UpdateText();
			}
		break;

		case WM_SIZE:
			{
				DoResize();
			}
			break;

		case WM_CLOSE:
			{
				viewers.erase( hWnd );
			}
			break;

		case WM_TEXT_PROGRESS:
			{
				SendMessage( hProgress, PBM_SETPOS, wParam, 0 );
			}
			break;
	}

	return DefWindowProc( hWnd, message, wParam, lParam);
}

void CTEXTContentViewer::Translate( void )
{
	TXTTranslateOptions opts;

	opts.EncodingID  = ENCODING_ASCII;
	opts.pTextBuffer = &pTextBuffer;
	opts.ProgressWnd = hWnd;
	opts.hStop       = hStopEvent;

	opts.LinePointers.clear();

	if ( pXlator != nullptr )
	{
		CTempFile FileObj( TempPath );

		FileObj.Keep();

		NUTSError::Code = NUTS_SUCCESS;

		pXlator->TranslateText( FileObj, &opts );

		if ( NUTSError::Code != NUTS_SUCCESS )
		{
			NUTSError::Report( L"Read File", hWnd );
		}

		if ( XlatorID == TUID_TEXT )
		{
			opts.EncodingID = FSEncodingID;
		}

		pTextArea->SetTextBody( opts.EncodingID, *opts.pTextBuffer, opts.TextBodyLength, opts.LinePointers );

		pTextBuffer = *opts.pTextBuffer;

		ShowWindow( hProgress, SW_HIDE );
		ShowWindow( pTextArea->hWnd, SW_SHOW );

		Translating = false;
	}
}

void CTEXTContentViewer::UpdateText( void )
{

}

void CTEXTContentViewer::DoResize()
{
	RECT r;

	GetClientRect( hWnd, &r );

	pTextArea->DoResize( r.right - r.left, r.bottom - r.top );

	SetWindowPos( hProgress, NULL,
		( ( r.right - r.left ) / 2 ) - ( ProgW / 2 ), ( ( r.bottom - r.top ) / 2 ) - ( ProgH / 2 ),
		ProgW, ProgH, SWP_NOREPOSITION | SWP_NOZORDER
	);

	/* Store the size as prefs - need to account for title bar */
	GetWindowRect( hWnd, &r );

	Preference( L"TextTranslatorWidth" )  = (DWORD) ( r.right - r.left );
	Preference( L"TextTranslatorHeight" ) = (DWORD) ( r.bottom - r.top );
}