#include "StdAfx.h"
#include "TEXTContentViewer.h"
#include "Plugins.h"
#include "NUTSError.h"
#include <richedit.h>
#include <stdio.h>
#include "resource.h"

std::map<HWND,CTEXTContentViewer *> CTEXTContentViewer::viewers;

bool CTEXTContentViewer::WndClassReg = false;

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
}

CTEXTContentViewer::~CTEXTContentViewer(void) {
	DestroyWindow( hWnd );

	delete pTextArea;

	if ( pTextBuffer != nullptr )
	{
		free( pTextBuffer );
	}
}

int CTEXTContentViewer::Create(HWND Parent, HINSTANCE hInstance, int x, int w, int h) {
	hWnd = CreateWindowEx(
		NULL,
		L"NUTS Text Content Renderer",
		L"NUTS Text Content Renderer",
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_VISIBLE,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
		Parent, NULL, hInstance, NULL
	);

	CTEXTContentViewer::viewers[ hWnd ] = this;

	ParentWnd = Parent;

	RECT rect;

	GetClientRect(hWnd, &rect);

	pTextArea = new EncodingTextArea( hWnd, 0, 0, rect.right - rect.left, rect.bottom - rect.top );

	Translate();

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
	}

	return DefWindowProc( hWnd, message, wParam, lParam);
}

void CTEXTContentViewer::Translate( void )
{
	TXTTranslateOptions opts;

	opts.EncodingID  = ENCODING_ASCII;
	opts.pTextBuffer = &pTextBuffer;

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
}