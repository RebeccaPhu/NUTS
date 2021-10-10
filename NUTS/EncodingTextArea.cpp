#include "StdAfx.h"
#include "EncodingTextArea.h"
#include "FontBitmap.h"
#include "Plugins.h"

#include "resource.h"

#include <map>

#include <WindowsX.h>
#include <assert.h>

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

	InitializeCriticalSection( &cs );

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
	StartLine = 0;

	LinePointers.clear();

	hScrollBar = CreateWindowEx(
		NULL,
		L"SCROLLBAR", L"",
		WS_CHILD|WS_VISIBLE|SBS_VERT|WS_DISABLED,
		w - 17, 1, 16, h - 1,
		hWnd, NULL, hInst, NULL
	);

	hCursor = CreatePen( PS_SOLID, 2, 0 );
	hArea   = NULL;
	hAreaOld = NULL;
	hAreaCanvas = NULL;

	TextPtrStart = 0xFFFFFFFF;
	TextPtrEnd   = 0xFFFFFFFF;
	SelStart     = 0;
	SelEnd       = 0;
	Selecting    = false;
	Blink        = false;
	Shift        = false;
	Ctrl         = false;
	Focus        = false;

	SetTimer( hWnd, 0x5858, GetCaretBlinkTime(), NULL );
}

EncodingTextArea::~EncodingTextArea(void)
{
	KillTimer( hWnd, 0x5858 );

	if ( hArea != NULL )
	{
		SelectObject( hArea, hAreaOld );

		DeleteDC( hArea );
	}

	NixObject( hAreaCanvas );
	NixObject( hCursor );

	NixWindow( hScrollBar );
	NixWindow( hWnd );

	DeleteCriticalSection( &cs );
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

	case WM_LBUTTONDOWN:
		{
			Selecting = true;

			SetCapture( hWnd );

			EnterCriticalSection( &cs );

			SelStart = GetMouseTextPtr( lParam );
			SelEnd   = SelStart;

			LeaveCriticalSection( &cs );

			Update();
		}
		break;

	case WM_MOUSEMOVE:
		{
			MouseParam = lParam;

			if ( Selecting )
			{
				EnterCriticalSection( &cs );

				DWORD Sel = GetMouseTextPtr( lParam );

				if ( Sel != 0xFFFFFFFF )
				{
					SelEnd = Sel;
				}

				LeaveCriticalSection( &cs );

				Update();
			}
		}
		break;

	case WM_LBUTTONUP:
		{
			Selecting = false;

			ReleaseCapture();

			Update();
		}
		break;

	case WM_RBUTTONUP:
		{
			RECT rect;

			GetWindowRect(hWnd, &rect);

			int mx = GET_X_LPARAM( lParam );
			int my = GET_Y_LPARAM( lParam );

			HMENU hTEA = LoadMenu( hInst, MAKEINTRESOURCE( IDR_TEA ) );

			HMENU hSubMenu = GetSubMenu( hTEA, 0 );

			TrackPopupMenu( hSubMenu, 0, rect.left + mx, rect.top + my, 0, hWnd, NULL );
		}
		break;

	case WM_COMMAND:
		if ( LOWORD(wParam) == IDM_COPY )
		{
			CopySelection( );
		}
		break;

	case WM_MOUSEWHEEL:
		{
			EnterCriticalSection( &cs );

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

			if ( ( StartLine < LineDefs.size() - MaxWinLines ) && ( WheelDelta > 0 ) )
			{
				if ( ( LineDefs.size() - MaxWinLines - StartLine ) > WheelDelta )
				{
					StartLine += WheelDelta;
				}
				else
				{
					StartLine = LineDefs.size() - MaxWinLines;
				}
			}

			SetScrollPos(hScrollBar, SB_CTL, StartLine, TRUE);

			if ( Selecting )
			{
				SelEnd = GetMouseTextPtr( MouseParam );
			}

			LeaveCriticalSection( &cs );

			Update();
		}

		break;

	case WM_TIMER:
		Blink = !Blink;

		Update();

		break;

	case WM_KEYDOWN:
		{
			const int keyboardScanCode = (lParam >> 16) & 0x00ff;
			const int virtualKey = wParam;

			DoKey( virtualKey, keyboardScanCode, true );
		}
		return 0;

	case WM_KEYUP:
		{
			const int keyboardScanCode = (lParam >> 16) & 0x00ff;
			const int virtualKey = wParam;

			DoKey( virtualKey, keyboardScanCode, false );
		}
		return 0;

	case WM_GETDLGCODE:
		switch ( wParam )
			{
				case VK_TAB:
				case VK_RETURN:
					return DLGC_STATIC;
			
				case VK_LEFT:
				case VK_RIGHT:
				case VK_UP:
				case VK_DOWN:
				case VK_PRIOR:
				case VK_NEXT:
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

	case WM_MOUSEACTIVATE:
		::SetFocus( hWnd );

		Update();

		return MA_ACTIVATE;

	case WM_SETFOCUS:
		Focus = true;

		Update();

		break;

	case WM_KILLFOCUS:
		Focus = false;

		Update();

		break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

void EncodingTextArea::PaintTextArea( void )
{
	EnterCriticalSection( &cs );

	RECT r;

	GetClientRect( hWnd, &r );

	r.right -= 15; // Scrollbar allowance

	HDC hDC = GetDC( hWnd );

	if ( hArea == NULL )
	{
		hArea = CreateCompatibleDC( hDC );

		hAreaCanvas = CreateCompatibleBitmap( hDC, r.right - r.left, r.bottom - r.top );

		hAreaOld = SelectObject( hArea, hAreaCanvas );
	}

	FillRect( hArea, &r, (HBRUSH) GetStockObject( WHITE_BRUSH ) );

	DWORD CursorLine     = 0xFFFFFFFF;
	DWORD CursorPtr      = 0;

	EnterCriticalSection( &cs );

	for ( int i=StartLine; i<(StartLine + MaxWinLines); i++ )
	{
		if ( LineDefs.size() < i + 1 )
		{
			continue;
		}

		LineDef l = LineDefs[ i ];

		DWORD ThisLineLen = l.NumChars;

		DWORD CLinePtr = l.StartPos;
		DWORD ELinePtr = l.StartPos + l.NumChars;

		if ( SelEnd >= CLinePtr )
		{
			CursorLine = i - StartLine;
			CursorPtr  = SelEnd - CLinePtr;
		}

		if ( ThisLineLen == 0 )
		{
			continue;
		}
		else
		{
			if ( ( TextPtrStart >= CLinePtr ) && ( TextPtrStart < ELinePtr ) && ( TextPtrEnd >= CLinePtr ) && ( TextPtrEnd < ELinePtr ) )
			{
				// Part of this line selected
				FontBitmap textline1( Font, &pTextBody[ CLinePtr ], (WORD) ( TextPtrStart - CLinePtr ), false, false );

				textline1.DrawText( hArea, 8, 8 + ( i - StartLine ) * 16, DT_LEFT | DT_TOP );

				FontBitmap textline2 = FontBitmap( Font, &pTextBody[ TextPtrStart ], (WORD) ( TextPtrEnd - TextPtrStart ), false, true );

				textline2.DrawText( hArea, 8 + ( 8 * ( TextPtrStart - CLinePtr ) ), 8 + ( i - StartLine ) * 16, DT_LEFT | DT_TOP );

				FontBitmap textline3 = FontBitmap( Font, &pTextBody[ TextPtrEnd ], (WORD) ( ThisLineLen - ( TextPtrEnd - CLinePtr ) ), false, false );

				textline3.DrawText( hArea, 8 + ( 8 * ( TextPtrEnd - CLinePtr ) ), 8 + ( i - StartLine ) * 16, DT_LEFT | DT_TOP );
			}
			else if ( ( TextPtrStart >= CLinePtr ) && ( TextPtrStart < ELinePtr ) && ( TextPtrEnd >= ELinePtr ) )
			{
				// End of this line selected
				FontBitmap textline1( Font, &pTextBody[ CLinePtr ], (WORD) ( TextPtrStart - CLinePtr ), false, false );

				textline1.DrawText( hArea, 8, 8 + ( i - StartLine ) * 16, DT_LEFT | DT_TOP );

				FontBitmap textline2 = FontBitmap( Font, &pTextBody[ TextPtrStart ], (WORD) ( ELinePtr - TextPtrStart ), false, true );

				textline2.DrawText( hArea, 8 + ( 8 * ( TextPtrStart - CLinePtr ) ), 8 + ( i - StartLine ) * 16, DT_LEFT | DT_TOP );
			}
			else if ( ( TextPtrStart < CLinePtr ) && ( TextPtrEnd < ELinePtr ) && ( TextPtrEnd >= CLinePtr ) )
			{
				// Start of this line selected
				FontBitmap textline1 = FontBitmap( Font, &pTextBody[ CLinePtr ], (WORD) ( TextPtrEnd - CLinePtr ), false, true );

				textline1.DrawText( hArea, 8, 8 + ( i - StartLine ) * 16, DT_LEFT | DT_TOP );

				FontBitmap textline2 = FontBitmap( Font, &pTextBody[ TextPtrEnd ], (WORD) ( ThisLineLen - ( TextPtrEnd - CLinePtr ) ), false, false );

				textline2.DrawText( hArea, 8 + ( 8 * ( TextPtrEnd - CLinePtr ) ), 8 + ( i - StartLine ) * 16, DT_LEFT | DT_TOP );
			}
			else
			{
				bool Selected = ( ( TextPtrStart < CLinePtr ) && ( TextPtrEnd >= ELinePtr ) );

				FontBitmap textline( Font, &pTextBody[ CLinePtr ], (WORD) ThisLineLen, false, Selected );

				textline.DrawText( hArea, 8, 8 + ( i - StartLine ) * 16, DT_LEFT | DT_TOP );
			}

		}
	}

	LeaveCriticalSection( &cs );

	if ( ( Blink ) && ( CursorLine != 0xFFFFFFFF ) && ( Focus ) )
	{
		SelectObject( hArea, hCursor );

		MoveToEx( hArea, 8 + CursorPtr * 8, 8 + CursorLine * 16, NULL );
		LineTo( hArea, 8 + CursorPtr * 8, 8 + ( CursorLine * 16 ) + 16 );
	}

	DrawEdge( hArea, &r, EDGE_SUNKEN, BF_RECT );

	/* Blit the off screen DC to the window */
	BitBlt( hDC, 0, 0, r.right - r.left, r.bottom - r.top, hArea, 0, 0, SRCCOPY );

	ValidateRect( hWnd, &r );

	ReleaseDC( hWnd, hDC );

	LeaveCriticalSection( &cs );
}

int EncodingTextArea::PrintText( HDC hDC, int pw, int ph )
{
	EnterCriticalSection( &cs );

	HDC hStaging = CreateCompatibleDC( hDC );

	HBITMAP hCanvas = CreateCompatibleBitmap( hDC, pw, ph );

	HGDIOBJ hOld = SelectObject( hStaging, hCanvas );

	StartPage( hDC );

	/* Draw out the text body */
	std::vector<DWORD>::iterator iLinePtr;

	iLinePtr      = LinePointers.begin();
	DWORD TextPtr = 0;
	DWORD LineLen = 0;
	DWORD LineNum = 0;
	DWORD MaxChars = ( ( pw - 800 ) / 7 ) / 8;
	DWORD MaxLines = ( ( ph - 800 ) / 7 ) / 16;

	if ( MaxChars > 255 ) { MaxChars = 255; }

	DWORD DisplayedLines = 0;

	while ( 1 )
	{
		/* This stops us wasting CPU cycles drawing off screen */
		if ( DisplayedLines > MaxLines )
		{
			EndPage( hDC );
			
			DisplayedLines = 0;
			LineNum        = 0;

			StartPage( hDC );
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

			FontBitmap textline( Font, &pTextBody[ TextPtr + ThisLinePtr ], (BYTE) ThisLineLen, false, false );

			textline.DrawText( hStaging, 0, 0, DT_LEFT | DT_TOP );

			StretchBlt( hDC, 400, 400 + LineNum * 16 * 7, ThisLineLen * 8 * 7, 16 * 5, hStaging, 0, 0, ThisLineLen * 8, 16, SRCCOPY );

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

	LeaveCriticalSection( &cs );

	EndPage( hDC );

	SelectObject( hStaging, hOld );

	DeleteObject( hCanvas );
	DeleteDC( hStaging );

	return 0;
}

int EncodingTextArea::SaveText( FILE *fFile )
{
	EnterCriticalSection( &cs );

	/* Draw out the text body */
	std::vector<DWORD>::iterator iLinePtr;

	iLinePtr      = LinePointers.begin();
	DWORD TextPtr = 0;
	DWORD LineLen = 0;
	DWORD LineNum = 0;

	while ( 1 )
	{
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

		BYTE *pLine = rstrndup( &pTextBody[ TextPtr + ThisLinePtr ], (WORD) LineLen );

		fprintf( fFile, "%s\n", (char *) pLine );

		free( pLine );

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

	LeaveCriticalSection( &cs );

	return 0;
}

int EncodingTextArea::SetFont( DWORD FontID )
{
	Font = FontID;

	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );

	return 0;
}

int EncodingTextArea::SetTextBody( DWORD FontID, BYTE *pBody, DWORD lBody, std::vector<DWORD> &rLinePointers )
{
	EnterCriticalSection( &cs );

	pTextBody = pBody;
	lTextBody = lBody;
	Font      = FontID;

	LinePointers = rLinePointers;

	ReGenerateLineDefs( );

	DWORD Max  = LineDefs.size() - 1;
	DWORD Page = MaxWinLines;

	LeaveCriticalSection( &cs );

	SetScrollbar( Max, Page );

	Update();

	return 0;
}

void EncodingTextArea::ReGenerateLineDefs( void )
{
	RECT r;

	GetClientRect( hWnd, &r );

	MaxWinLines = ( r.bottom - r.top - 8 ) / 16;
	MaxWinChars = ( r.right - r.left - 16 ) / 8;

	// Calculate the number of actual lines, taking into account line wrap
	std::vector<DWORD>::iterator iLine, iNextLine;

	DWORD ThisPos   = 0;

	LineDefs.clear();

	if ( LinePointers.size() > 0 )
	{
		iLine     = LinePointers.begin();
		iNextLine = LinePointers.begin()++;

		do {
			DWORD NextPos;

			if ( iNextLine == LinePointers.end() )
			{
				NextPos = lTextBody;
			}
			else
			{
				NextPos = *iNextLine;
			}

			while ( ( NextPos - ThisPos ) > MaxWinChars )
			{
				LineDef l = { ThisPos, MaxWinChars };

				LineDefs.push_back( l );

				ThisPos += MaxWinChars;
			}

			LineDef l = { ThisPos, NextPos - ThisPos };

			LineDefs.push_back( l );

			iLine++;
			
			if ( iNextLine != LinePointers.end() )
			{
				iNextLine++;
			}

			ThisPos = NextPos;
		} while ( iNextLine != LinePointers.end() );
	}
	else
	{
		if ( lTextBody > 0 )
		{
			DWORD lSize = lTextBody;
			DWORD pOff  = 0;

			while ( lSize > MaxWinChars )
			{
				LineDef l = { pOff, lSize };

				LineDefs.push_back( l );

				pOff  += MaxWinChars;
				lSize -= MaxWinChars;
			}

			LineDef l = { pOff, lSize };

			LineDefs.push_back( l ) ;
		}
	}

	while ( ( ( StartLine + MaxWinLines ) > LineDefs.size() ) && ( StartLine > 0 ) )
	{
		StartLine--;
	}
}

int EncodingTextArea::DoResize( int w, int h )
{
	::SetWindowPos( hWnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION );
	::SetWindowPos( hScrollBar, NULL, w - 16, 0, 16, h, SWP_NOZORDER | SWP_NOREPOSITION );

	EnterCriticalSection( &cs );

	ReGenerateLineDefs();

	DWORD Max  = LineDefs.size() - 1;
	DWORD Page = MaxWinLines;

	LeaveCriticalSection( &cs );

	if ( hArea != NULL )
	{
		SelectObject( hArea, hAreaOld );

		NixObject( hAreaCanvas );
		NixObject( hArea );
	}

	hArea       = NULL;
	hAreaCanvas = NULL;
	hAreaOld    = NULL;

	SetScrollbar( Max, Page );

	Update();

	return 0;
}

void EncodingTextArea::Update( void )
{
	/* Update selections */
	SetPtrs();

	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );
}

void EncodingTextArea::DoScroll(WPARAM wParam, LPARAM lParam)
{
	EnterCriticalSection( &cs );

	if (LOWORD(wParam) == SB_THUMBTRACK) {
		StartLine = HIWORD(wParam);

		SetScrollPos(hScrollBar, SB_CTL, StartLine, TRUE);

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
		if ( StartLine < LineDefs.size() - MaxWinLines )
		{
			StartLine++;
		}

		SetScrollPos(hScrollBar, SB_CTL, StartLine, TRUE);

		Update();
	}

	if (LOWORD(wParam) == SB_PAGEDOWN) {
		if ( StartLine < ( LineDefs.size() - MaxWinLines - 4 ) )
		{
			StartLine += 4;
		}
		else if ( StartLine < LineDefs.size() - MaxWinLines )
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

	LeaveCriticalSection( &cs );
}

DWORD EncodingTextArea::GetMouseTextPtr( LPARAM lParam )
{
	int MouseX = GET_X_LPARAM( lParam );
	int MouseY = GET_Y_LPARAM( lParam );

	RECT r;

	GetClientRect( hWnd, &r );

	int w = r.right - r.left;
	int h = r.bottom - r.top;

	if ( ( MouseX < 0 ) || ( MouseY < 0 ) ) { return 0xFFFFFFFF; }
	if ( MouseX > ( w - 8 ) ) { return 0xFFFFFFFF; }
	if ( MouseY > ( h - 8 ) ) { return 0xFFFFFFFF; }

	MouseX -= 8;
	MouseY -= 8;

	MouseX /= 8;
	MouseY /= 16;

	if ( MouseY + StartLine > LineDefs.size() )
	{
		return 0;
	}

	if ( MouseX >= LineDefs[ MouseY + StartLine ].NumChars )
	{
		MouseX = LineDefs[ MouseY + StartLine ].NumChars;
	}

	return LineDefs[ MouseY + StartLine ].StartPos + MouseX;
}

void EncodingTextArea::SafeByteCopy( BYTE *dest, BYTE *src, DWORD len )
{
	for ( DWORD i = 0; i < len; i++ )
	{
		if ( ( src[ i ] >= 32 ) && ( src[ i ] <= 126 ) )
		{
			dest[ i ] = src[ i ];
		}
		else if ( ( src[ i ] >= 128 ) && ( src[ i ] <= 255 ) )
		{
			dest[ i ] = src[ i ];
		}
		else
		{
			dest[ i ] = '.';
		}
	}
}

int EncodingTextArea::CopySelection( void )
{
	if ( TextPtrEnd == TextPtrStart )
	{
		return 0;
	}

	EnterCriticalSection( &cs );

	DWORD CopySize = ( TextPtrEnd - TextPtrStart ) + 1024;
	DWORD TextSize =   0;

	BYTE *pArea = (BYTE *) malloc( CopySize );

	std::vector<DWORD>::iterator iLinePtr;

	iLinePtr      = LinePointers.begin();
	DWORD TextPtr = 0;
	DWORD LineLen = 0;

	while ( TextPtr < TextPtrEnd )
	{
		if ( iLinePtr == LinePointers.end() )
		{
			LineLen = lTextBody - TextPtr;
		}
		else
		{
			LineLen = *iLinePtr - TextPtr;
		}

		// Do partial line copies
		BYTE *pStart = &pTextBody[ TextPtr ];
		DWORD lLine  = LineLen;
		DWORD ELine  = TextPtr + LineLen;

		bool  CopyThis = false;

		if ( ( TextPtrStart >= TextPtr ) && ( TextPtrStart < ELine ) )
		{
			// Selection starts mid-line
			pStart = &pTextBody[ TextPtrStart ];
			lLine  = LineLen - ( TextPtrStart - TextPtr );

			CopyThis = true;
		}
		else if ( ( TextPtrEnd >= TextPtr ) && ( TextPtrEnd < ELine ) )
		{
			// Select ends mid-line
			lLine -= ELine - TextPtrEnd;

			CopyThis = true;
		}
		else if ( ( TextPtrStart < TextPtr ) && ( TextPtrEnd >= ELine ) )
		{
			CopyThis = true;
		}

		if ( CopyThis )
		{
			while ( ( TextSize + lLine + 3U ) > CopySize )
			{
				CopySize += 1024;

				pArea = (BYTE *) realloc( (void *) pArea, CopySize );
			}

			SafeByteCopy( &pArea[ TextSize ], pStart, lLine );

			TextSize += lLine;

			pArea[ TextSize++ ] = 0x0D;
			pArea[ TextSize++ ] = 0x0A;
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

	LeaveCriticalSection( &cs );

	pArea[ TextSize ] = 0;

	if ( IsClipboardFormatAvailable( CF_TEXT ) )
	{
		OpenClipboard( hWnd );
		EmptyClipboard();

		HGLOBAL hg=GlobalAlloc( GMEM_MOVEABLE, TextSize  );

		memcpy( GlobalLock( hg ), pArea, TextSize );

		GlobalUnlock( hg );

		SetClipboardData( CF_TEXT, hg );

		CloseClipboard();
				
		GlobalFree( hg );
	}

	free( pArea );

	return 0;
}

void EncodingTextArea::DoKey( const int virtualKey, const int scanCode, bool Down )
{
	EnterCriticalSection( &cs );

	if ( ( LinePointers.size() == 0 ) || ( lTextBody < 1 ) )
	{
		LeaveCriticalSection( &cs );

		return;
	}

	int cl = (int) GetCursorLine();

	if ( Down )
	{
		if ( ( virtualKey == VK_LEFT ) && ( SelEnd >= 0 ) )
		{
			SelEnd--;

			ScrollToLine( cl );

			if ( !Shift )
			{
				SelStart = SelEnd;
			}

			SetPtrs();
		}
		else if ( ( virtualKey == VK_RIGHT ) && ( SelEnd < ( lTextBody - 1 ) ) )
		{
			SelEnd++;

			ScrollToLine( cl );

			if ( !Shift )
			{
				SelStart = SelEnd;
			}

			SetPtrs();
		}
		else if ( virtualKey == VK_UP )
		{
			if ( cl > 0 )
			{
				SetCursorLine( cl, cl - 1 );

				ScrollToLine( cl - 1 );
			}

			if ( !Shift )
			{
				SelStart = SelEnd;
			}

			SetPtrs();
		}
		else if ( virtualKey == VK_DOWN )
		{
			if ( cl < LineDefs.size() - 1 )
			{
				SetCursorLine( cl, cl + 1 );

				ScrollToLine( cl + 1 );
			}

			if ( !Shift )
			{
				SelStart = SelEnd;
			}

			SetPtrs();
		}
		else if ( virtualKey == VK_PRIOR )
		{
			if ( ( cl - ( (int) MaxWinLines - 1 ) ) > 0 )
			{
				SetCursorLine( cl, cl - MaxWinLines );

				ScrollToLine( cl - MaxWinLines );
			}
			else
			{
				SetCursorLine( cl, 0 );

				ScrollToLine( 0 );
			}

			if ( !Shift )
			{
				SelStart = SelEnd;
			}

			SetPtrs();
		}
		else if ( virtualKey == VK_NEXT )
		{
			if ( cl + MaxWinLines < LineDefs.size() - 1 )
			{
				SetCursorLine( cl, cl + MaxWinLines );

				ScrollToLine( cl + MaxWinLines );
			}
			else
			{
				SetCursorLine( cl, LineDefs.size() - 1 );

				ScrollToLine( LineDefs.size() - 1 );
			}

			if ( !Shift )
			{
				SelStart = SelEnd;
			}

			SetPtrs();
		}
		else if ( virtualKey == VK_HOME )
		{
			if ( Ctrl )
			{
				SelEnd = 0;

				ScrollToLine( 0 );
			}
			else
			{
				SelEnd = LineDefs[ cl ].StartPos;

				ScrollToLine( cl );
			}

			if ( !Shift )
			{
				SelStart = SelEnd;
			}

			SetPtrs();
		}
		else if ( virtualKey == VK_END )
		{
			if ( Ctrl )
			{
				SelEnd = ( LineDefs.back().StartPos + LineDefs.back().NumChars ) - 1;

				ScrollToLine( LineDefs.size() - 1 );
			}
			else
			{
				SelEnd = ( LineDefs[ cl ].StartPos + LineDefs[ cl ].NumChars ) - 1;

				ScrollToLine( cl );
			}

			if ( !Shift )
			{
				SelStart = SelEnd;
			}

			SetPtrs();
		}
		else if ( virtualKey == VK_SHIFT ) { Shift = true; }
		else if ( virtualKey == VK_CONTROL ) { Ctrl = true; }
	}
	else
	{
		if ( virtualKey == VK_SHIFT ) { Shift = false; }
		else if ( virtualKey == VK_CONTROL ) { Ctrl = false; }
		else
		{
			BYTE keyboardState[256];
			GetKeyboardState(keyboardState);

			WORD ascii = 0;
			const int len = ToAscii(virtualKey, scanCode, keyboardState, &ascii, 0);

			if ( ( ascii & 0xFF ) == 3 /* CTRL+C */ )
			{
				CopySelection();
			}
		}
	}

	LeaveCriticalSection( &cs );

	Update();
}

void EncodingTextArea::SetPtrs()
{
	if ( SelStart < SelEnd )
	{
		TextPtrStart = SelStart;
		TextPtrEnd   = SelEnd;
	}
	else if ( SelStart > SelEnd )
	{
		TextPtrStart = SelEnd;
		TextPtrEnd   = SelStart;
	}
	else
	{
		TextPtrStart = 0xFFFFFFFF;
		TextPtrEnd   = 0xFFFFFFFF;
	}
}

void EncodingTextArea::ScrollToLine( int CLine )
{
	while ( ( CLine < StartLine ) && ( CLine >= 0 ) ) { StartLine--; }
	while ( ( CLine > ( StartLine + ( MaxWinLines - 1 ) ) ) && ( CLine < LineDefs.size() ) ) { StartLine++; }

	SetScrollPos(hScrollBar, SB_CTL, StartLine, TRUE);
}

DWORD EncodingTextArea::GetCursorLine( void )
{
	DWORD cl = 0;

	for ( DWORD i = 0; i<LineDefs.size(); i++ )
	{
		if ( SelEnd >= LineDefs[ i ].StartPos )
		{
			cl = i;
		}
		else
		{
			break;
		}
	}

	return cl;
}

void EncodingTextArea::SetCursorLine( DWORD cline, DWORD line )
{
	if ( ( LineDefs.size() > line ) && ( LineDefs.size() > cline ) )
	{
		DWORD cpos = SelEnd - LineDefs[ cline ].StartPos;

		if ( cpos > LineDefs[ line ].NumChars )
		{
			if ( LineDefs[ line ].NumChars > 0 )
			{
				cpos = LineDefs[ line ].NumChars - 1;
			}
			else
			{
				cpos = 0;
			}
		}

		SelEnd = LineDefs[ line ].StartPos + cpos;
	}
}

void EncodingTextArea::SetScrollbar( DWORD Max, DWORD Page )
{
	SCROLLINFO si;

	si.cbSize = sizeof( SCROLLINFO );
	si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;
	si.nMin   = 0;
	si.nMax   = Max;
	si.nPage  = Page;
	si.nPos   = 0;

	SetScrollInfo( hScrollBar, SB_CTL, &si, TRUE );

	if ( Page < Max )
	{
		EnableWindow( hScrollBar, TRUE );
	}
	else
	{
		EnableWindow( hScrollBar, FALSE );
	}
}
