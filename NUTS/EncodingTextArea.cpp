#include "StdAfx.h"
#include "EncodingTextArea.h"
#include "FontBitmap.h"
#include "Plugins.h"

#include <map>


bool EncodingTextArea::_HasWindowClass = false;

std::map<HWND, EncodingTextArea *> EncodingTextArea::_EncodingTextAreaClassMap;

const wchar_t EncodingTextAreaClass[]  = L"Encoding Text Area Control";

LRESULT CALLBACK EncodingTextArea::EncodingTextAreaWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::map<HWND, EncodingTextArea *>::iterator iWindow = EncodingTextArea::_EncodingTextAreaClassMap.find( hWnd );

	if ( iWindow != EncodingTextArea::_EncodingTextAreaClassMap.end() )
	{
		return iWindow->second->WindowProc( uMsg, wParam, lParam );
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

EncodingTextArea::EncodingTextArea( HWND hParent, int x, int y, int w, int h )
{
	if ( !_HasWindowClass )
	{
		WNDCLASS wc = { };

		wc.lpfnWndProc   = EncodingTextAreaWindowProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = EncodingTextAreaClass;
		wc.hCursor       = LoadCursor( NULL, IDC_IBEAM );

		ATOM atom = RegisterClass(&wc);

		_HasWindowClass = true;
	}

	hWnd = CreateWindowEx(
		0,
		EncodingTextAreaClass,
		L"Encoding Text Area",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP,

		x, y, w, h,

		hParent, NULL, hInst, NULL
	);

	_EncodingTextAreaClassMap[ hWnd ] = this;

	pTextBody = nullptr;
	lTextBody = 0;
	Parent    = hParent;
	MaxLine   = 0;
	StartLine = 0;

	LinePointers.clear();

	hScrollBar = CreateWindowEx(
		NULL,
		L"SCROLLBAR", L"",
		WS_CHILD|WS_VISIBLE|SBS_VERT|WS_DISABLED,
		w - 16, 0, 16, h,
		hWnd, NULL, hInst, NULL
	);
}

EncodingTextArea::~EncodingTextArea(void)
{
	NixWindow( hScrollBar );
	NixWindow( hWnd );
}

LRESULT EncodingTextArea::WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch (uMsg )
	{
	case WM_PAINT:
		{
			PaintTextArea();
		}

		break;

	case WM_CLOSE:
		{
			_EncodingTextAreaClassMap.erase( hWnd );
		}
		break;

	case WM_VSCROLL:
		{
			DoScroll(wParam, lParam);
		}
		return 0;

	case WM_MOUSEWHEEL:
		{
			long WheelDelta = 0 - (GET_WHEEL_DELTA_WPARAM(wParam) / 30);

			if ( ( StartLine > 0 ) && ( WheelDelta < 0 ) )
			{
				if ( abs(WheelDelta) < StartLine )
				{
					StartLine += WheelDelta;
				}
				else
				{
					StartLine = 0;
				}
			}

			if ( ( StartLine < MaxLine ) && ( WheelDelta > 0 ) )
			{
				if ( ( MaxLine - StartLine ) > WheelDelta )
				{
					StartLine += WheelDelta;
				}
				else
				{
					StartLine = MaxLine;
				}
			}

			SetScrollPos(hScrollBar, SB_CTL, StartLine, TRUE);

			Update();
		}

		break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

void EncodingTextArea::PaintTextArea( void )
{
	RECT r;

	GetClientRect( hWnd, &r );

	r.right -= 16; // Scrollbar allowance

	HDC hDC = GetDC( hWnd );

	FillRect( hDC, &r, (HBRUSH) GetStockObject( WHITE_BRUSH ) );

	/* Draw out the text body */
	std::vector<DWORD>::iterator iLinePtr;

	iLinePtr      = LinePointers.begin();
	DWORD TextPtr = 0;
	DWORD LineLen = 0;
	DWORD LineNum = 0;
	DWORD MaxChars = ( r.right - r.left - 16 ) / 8;
	DWORD MaxLines = ( r.bottom - r.top ) / 16;

	if ( MaxChars > 255 ) { MaxChars = 255; }

	std::vector<DWORD> FontList = FSPlugins.FontListForEncoding( Encoding );

	DWORD FontID = FontList[ 0 ];

	for ( DWORD SkipLine = 0; SkipLine<StartLine; SkipLine++ )
	{
		if ( iLinePtr == LinePointers.end() )
		{
			LineLen = lTextBody - TextPtr;
		}
		else
		{
			LineLen = *iLinePtr - TextPtr;
			iLinePtr++;
			TextPtr += LineLen;
		}
	}

	DWORD DisplayedLines = 0;

	while ( 1 )
	{
		/* This stops us wasting CPU cycles drawing off screen */
		if ( DisplayedLines > MaxLines )
		{
			break;
		}

		if ( iLinePtr == LinePointers.end() )
		{
			LineLen = lTextBody - TextPtr;
		}
		else
		{
			LineLen = *iLinePtr - TextPtr;
		}

		/* The line must now be divided into logical units of the window width */
		DWORD ThisLinePtr = 0;

		while ( LineLen > 0 )
		{
			DWORD ThisLineLen = LineLen;

			if ( ThisLineLen > MaxChars ) { ThisLineLen = MaxChars; }

			FontBitmap textline( FontID, &pTextBody[ TextPtr + ThisLinePtr ], (BYTE) ThisLineLen, false, false );

			textline.DrawText( hDC, 8, 8 + LineNum * 18, DT_LEFT | DT_TOP );

			LineNum++;
			DisplayedLines++;

			LineLen     -= ThisLineLen;
			ThisLinePtr += ThisLineLen;
		}

		if ( iLinePtr != LinePointers.end() )
		{
			TextPtr = *iLinePtr;

			iLinePtr++;
		}

		if ( iLinePtr == LinePointers.end() )
		{
			break;
		}
	}

	DrawEdge( hDC, &r, EDGE_SUNKEN, BF_RECT );

	ValidateRect( hWnd, &r );

	ReleaseDC( hWnd, hDC );
}

int EncodingTextArea::SetTextBody( DWORD EncodingID, BYTE *pBody, DWORD lBody, std::vector<DWORD> &rLinePointers )
{
	pTextBody = pBody;
	lTextBody = lBody;
	Encoding  = EncodingID;

	LinePointers = rLinePointers;

	RECT r;

	GetClientRect( hWnd, &r );

	DWORD MaxLines = ( r.bottom - r.top - 8 ) / 18;
	DWORD LastLine = LinePointers.size() + 1;

	if ( LastLine > MaxLines )
	{
		MaxLine = LastLine - MaxLines;
	}
	else
	{
		MaxLine = LastLine;
	}

	SCROLLINFO si;

	si.cbSize = sizeof( SCROLLINFO );
	si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;
	si.nMin   = 0;
	si.nMax   = MaxLine;
	si.nPage  = 4;
	si.nPos   = 0;

	SetScrollInfo( hScrollBar, SB_CTL, &si, TRUE );

	if ( MaxLine < LastLine )
	{
		EnableWindow( hScrollBar, TRUE );
	}
	else
	{
		EnableWindow( hScrollBar, FALSE );
	}

	return 0;
}

int EncodingTextArea::DoResize( int w, int h )
{
	::SetWindowPos( hWnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION );
	::SetWindowPos( hScrollBar, NULL, w - 16, 0, 16, h, SWP_NOZORDER | SWP_NOREPOSITION );

	Update();

	return 0;
}

void EncodingTextArea::Update( void )
{
	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );
}

void EncodingTextArea::DoScroll(WPARAM wParam, LPARAM lParam)
{
	if (LOWORD(wParam) == SB_THUMBTRACK) {
		StartLine = HIWORD(wParam);

		Update();
	}

	if (LOWORD(wParam) == SB_LINEUP) {
		if ( StartLine > 0 )
		{
			StartLine--;
		}

		SetScrollPos(hScrollBar, SB_CTL, StartLine, TRUE);

		Update();
	}

	if (LOWORD(wParam) == SB_PAGEUP) {
		if ( StartLine > 4 )
		{
			StartLine -= 4;
		}
		else if ( StartLine > 0 )
		{
			StartLine--;
		}

		SetScrollPos(hScrollBar, SB_CTL, StartLine, TRUE);

		Update();
	}

	if (LOWORD(wParam) == SB_LINEDOWN) {
		if ( StartLine < MaxLine )
		{
			StartLine++;
		}

		SetScrollPos(hScrollBar, SB_CTL, StartLine, TRUE);

		Update();
	}

	if (LOWORD(wParam) == SB_PAGEDOWN) {
		if ( StartLine < ( MaxLine - 4 ) )
		{
			StartLine += 4;
		}
		else if ( StartLine < MaxLine )
		{
			StartLine++;
		}

		SetScrollPos(hScrollBar, SB_CTL, StartLine, TRUE);

		Update();
	}

	if (LOWORD(wParam) == SB_ENDSCROLL) {
		SetScrollPos(hScrollBar, SB_CTL, StartLine, TRUE);

		Update();
	}
}
