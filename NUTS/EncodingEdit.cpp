#include "StdAfx.h"
#include "EncodingEdit.h"
#include "FontBitmap.h"
#include "libfuncs.h"

#include "resource.h"

#include <time.h>
#include <WindowsX.h>
#include <string>

#ifndef FONTBITMAP_PLUGIN
#include "EncodingClipboard.h"
#else
extern HINSTANCE hInstance;
#endif

bool EncodingEdit::_HasWindowClass = false;

std::map<HWND, EncodingEdit *> EncodingEdit::_EncodingEditClassMap;

const wchar_t EncodingEditClass[]  = L"Encoding Edit Control";

LRESULT CALLBACK EncodingEditWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::map<HWND, EncodingEdit *>::iterator iWindow = EncodingEdit::_EncodingEditClassMap.find( hWnd );

	if ( iWindow != EncodingEdit::_EncodingEditClassMap.end() )
	{
		return iWindow->second->WindowProc( uMsg, wParam, lParam );
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

#ifdef FONTBITMAP_PLUGIN
EncodingEdit::EncodingEdit( HWND hParent, int x, int y, int w, BYTE *pFontData )
#else
EncodingEdit::EncodingEdit( HWND hParent, int x, int y, int w, bool FontChanger )
#endif
{
#ifdef FONTBITMAP_PLUGIN
	bool FontChanger = false;
	HINSTANCE hInst  = hInstance;
#endif

	if ( !_HasWindowClass )
	{
		WNDCLASS wc = { };

		ZeroMemory( &wc, sizeof( wc ) );

		wc.lpfnWndProc   = EncodingEditWindowProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = EncodingEditClass;
		wc.hCursor       = LoadCursor( NULL, IDC_IBEAM );

		ATOM atom = RegisterClass(&wc);

		_HasWindowClass = true;
	}

	hWnd = CreateWindowEx(
		0,
		EncodingEditClass,
		L"Encoding Edit Control",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP,

		x, y, (FontChanger)?(w-24):w, 24,

		hParent, NULL, hInst, NULL
	);

	pValidator = nullptr;

#ifndef FONTBITMAP_PLUGIN
	pChanger   = nullptr;
	hChangeTip = NULL;

	if ( FontChanger )
	{
		pChanger = new IconButton( hParent, x + ( w - 24 ), y, LoadIcon( hInst, MAKEINTRESOURCE( IDI_FONTSWITCH ) ) );

		pChanger->SetTip( L"Change the font used for rendering the text" );
	}
#else
	pFont = pFontData;
#endif

	EncodingEdit::_EncodingEditClassMap[ hWnd ] = this;

	UINT BlinkRate = GetCaretBlinkTime();

	Blink = 0;

	SetTimer( hWnd, 0xCA7, BlinkRate, NULL );

	DWORD err = GetLastError();

	EditText[ 0 ] = 0;
	Length        = 0;
	Cursor        = 0;
	Encoding      = ENCODING_ASCII;
	HasFocus      = false;
	LDown         = false;
	CW            = w;

#ifndef FONTBITMAP_PLUGIN
	if ( ( pChanger != nullptr ) && ( CW > 24 ) ) { CW -= 24; }
	FontNum       = 0;
#endif

	DChars        = CW / 8;
	SChar         = 0;
	Shift         = false;
	Ctrl          = false;
	Select        = false;
	Disabled      = false;
	SoftDisable   = false;
	SCursor       = 0;
	ECursor       = 0;
	MouseXS       = 0;
	MaxLen        = 255;
	Parent        = hParent;
	Changes       = false;
	AllowNegative = false;
	AllowedChars  = textInputAny;
	hArea         = NULL;
	hAreaOld      = NULL;
	hAreaCanvas   = NULL;
	FontChanged   = true;

	if ( CW % 8 ) { DChars++; }

	LOGBRUSH brsh;

	brsh.lbColor = RGB( 0xCC, 0xCC, 0xCC );
	brsh.lbStyle = BS_SOLID;

	hDisBrush = CreateBrushIndirect( &brsh );

	pBuddyControl = false;
	AllowSpaces   = true;
}


EncodingEdit::~EncodingEdit(void)
{
	if ( hAreaOld )
	{
		SelectObject( hArea, hAreaOld );
	}

	NixObject( hAreaCanvas );

	if ( hArea )
	{
		DeleteDC( hArea );
	}

	NixObject( hDisBrush );

#ifndef FONTBITMAP_PLUGIN
	NixWindow( hChangeTip );

	if ( pChanger == nullptr )
	{
		delete pChanger;
	}
#endif
	
	NixWindow( hWnd );
}

LRESULT EncodingEdit::WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	bool invalidate = false;

	switch ( uMsg )
	{
	case WM_PAINT:
		PaintControl();

		break;

	case WM_ERASEBKGND:
		return FALSE;

	case WM_TIMER:
		if ( wParam == 0xCA7 )
		{
			Blink = 1 - Blink;

			Invalidate();
		}
		break;

	case WM_MOUSEACTIVATE:
		::SetFocus( hWnd );

		Invalidate();

		return MA_ACTIVATE;

	case WM_SETFOCUS:
		HasFocus = true;

		{
			Select   = true;
			SCursor  = 0;
			Cursor   = Length;

			Invalidate();
#ifndef FONTBITMAP_PLUGIN
			CharMap::SetFocusWindow( hWnd );
#endif
		}

		break;

	case WM_KILLFOCUS:
		HasFocus = false;

		{
			Select   = false;

			Invalidate();
		}

		break;

#ifndef FONTBITMAP_PLUGIN
	case WM_DESTROY:
		CharMap::RemoveFocus( hWnd );
		break;
#endif

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

	case WM_KEYDOWN:
		{
			const int keyboardScanCode = (lParam >> 16) & 0x00ff;
			const int virtualKey = wParam;

			if ( virtualKey == VK_BACK )
			{
				if ( !Disabled )
				{
					if ( Select )
					{
						BYTE LC, RC;

						if ( Cursor < SCursor ) { LC = Cursor; RC = SCursor; } else { LC = SCursor; RC = Cursor; }

						for ( int i = RC; i<Length; i++ )
						{
							if ( ( ( LC + ( i - RC ) ) <= MaxLen ) && ( i <= MaxLen ) )
							{
								EditText[ LC + ( i - RC ) ] = EditText[ i ];
							}
						}

						Length -= ( RC - LC );
						Cursor = LC;

						Changes = true;
					}
					else
					{
						if ( Cursor > 0 ) 
						{
							for ( int i = Cursor; i<Length; i++ )
							{
								if ( i <= MaxLen )
								{
									EditText[ i - 1 ] = EditText[ i ];
								}
							}

							Cursor--;
							Length--;

							Changes = true;
						}
					}
				}

				if ( !Shift ) { Select = false; }
			}
			else if ( virtualKey == VK_DELETE )
			{
				if ( !Disabled )
				{
					if ( Select )
					{
						BYTE LC, RC;

						if ( Cursor < SCursor ) { LC = Cursor; RC = SCursor; } else { LC = SCursor; RC = Cursor; }

						for ( int i = RC; i<Length; i++ )
						{
							if ( ( ( LC + ( i - RC ) ) <= MaxLen ) && ( i <= MaxLen ) )
							{
								EditText[ LC + ( i - RC ) ] = EditText[ i ];
							}
						}

						Length -= ( RC - LC );
						Cursor = LC;

						Changes = true;
					}
					else
					{
						if ( Cursor < Length )
						{
							for ( int i = Cursor; i<Length; i++ )
							{
								if ( i <= ( MaxLen - 1 ) )
								{
									EditText[ i ] = EditText[ i + 1 ];
								}
							}

							Length--;

							Changes = true;
						}
					}
				}

				if ( !Shift ) { Select = false; }
			}
			else if ( virtualKey == VK_LEFT )
			{
				if ( Cursor > 0 ) { Cursor--; }

				Select = Shift;
			}
			else if ( virtualKey == VK_RIGHT )
			{
				if ( Cursor < Length ) { Cursor++; }

				Select = Shift;
			}
			else if ( virtualKey == VK_HOME )
			{
				Cursor = 0;

				Select = Shift;
			}
			else if ( virtualKey == VK_END )
			{
				Cursor = Length;

				Select = Shift;
			}
			else if ( virtualKey == VK_SHIFT )
			{
				if ( ( !Shift ) && ( !Select ) )
				{
					SCursor = Cursor;
				}

				Shift   = true;
			}
			else if ( virtualKey == VK_CONTROL )
			{
				Ctrl  = true;
			}
			else
			{
				BYTE keyboardState[256];
				GetKeyboardState(keyboardState);

				WORD ascii = 0;
				const int len = ToAscii(virtualKey, keyboardScanCode, keyboardState, &ascii, 0);

				if ( ( ascii == 0x0020 ) && ( !AllowSpaces ) )
				{
					ascii = 0x0000;
				}

				if ( ( ascii & 0xFF ) == 22 /* CTRL+V */ )
				{
					::PostMessage( hWnd, WM_COMMAND, IDM_EE_PASTE, 0 );
				}
				else if ( ( ascii & 0xFF ) == 24 /* CTRL+X */ )
				{
					if ( Select )
					{
						::PostMessage( hWnd, WM_COMMAND, IDM_EE_CUT, 0 );
					}
				}
				else if ( ( ascii & 0xFF ) == 3 /* CTRL+C */ )
				{
					if ( Select )
					{
						::PostMessage( hWnd, WM_COMMAND, IDM_EE_COPY, 0 );
					}
				}
				else if ( ascii != 0x0000 )
				{
					ProcessASCII( ascii );
				}
			}

			while ( Cursor - SChar > ( DChars - 1 ) )
			{
				SChar++;
			}

			while ( Cursor - SChar < 0 )
			{
				SChar--;
			}
		
			Invalidate();
		}
		
		return 0;

	case WM_CHARMAPCHAR:
		{
			WORD ascii = LOWORD( wParam );

			ProcessASCII( ascii );

			Invalidate();
		}
		break;

	case WM_KEYUP:
		{
			if ( wParam == VK_SHIFT )
			{
				Shift = false;
			}

			if ( wParam == VK_CONTROL )
			{
				Ctrl = false;
			}
		}
		break;

	case WM_LBUTTONDOWN:
		{
			LDown = true;

			if ( !Shift ) { Select = false; }

			Cursor = SChar + GET_X_LPARAM( lParam ) / 8;

			if ( Cursor > Length )
			{
				Cursor = Length;
			}

			MouseXS = LOWORD( lParam );
		}

		break;

	case WM_MOUSEMOVE:
		{
			WORD NewMouseX = GET_X_LPARAM( lParam );

			BYTE NMP = SChar + NewMouseX / 8;
			BYTE OMP = SChar + MouseXS / 8;
			
			if ( NMP > Length )
			{
				NMP = Length;
			}

			if ( OMP > Length )
			{
				OMP = Length;
			}

			if ( abs( NewMouseX - MouseXS ) > 8 )
			{
				if ( LDown )
				{
					Cursor = NMP;
					SCursor = OMP;

					Select = true;

					Invalidate();
				}
			}
		}
		break;

	case WM_LBUTTONUP:
		{
			LDown = false;

			Invalidate();
		}
		break;

#ifndef FONTBITMAP_PLUGIN
	case WM_RBUTTONUP:
		{
			RECT rect;

			GetWindowRect(hWnd, &rect);

			HMENU hPopup   = LoadMenu(hInst, MAKEINTRESOURCE(IDR_EDIT_MENU));
			HMENU hSubMenu = GetSubMenu(hPopup, 0);

			EnableMenuItem( hSubMenu, IDM_EE_COPY, MF_BYCOMMAND | ( ( Select ) ? MF_ENABLED : MF_GRAYED ) );

			EnableMenuItem( hSubMenu, IDM_EE_CUT, MF_BYCOMMAND | ( ( Select ) ? MF_ENABLED : MF_GRAYED ) );
			EnableMenuItem( hSubMenu, IDM_EE_DELETE, MF_BYCOMMAND | ( ( Select ) ? MF_ENABLED : MF_GRAYED ) );

			EnableMenuItem( hSubMenu, IDM_EE_PASTE, MF_BYCOMMAND | ( ( IsClipboardFormatAvailable( CF_TEXT ) ) ? MF_ENABLED : MF_GRAYED ) );
			EnableMenuItem( hSubMenu, IDM_EE_PASTEE, MF_BYCOMMAND | ( ( IsClipboardFormatAvailable( CF_TEXT ) ) ? MF_ENABLED : MF_GRAYED ) );

			TrackPopupMenu( hSubMenu, 0, rect.left + LOWORD( lParam ), rect.top + HIWORD( lParam ), 0, hWnd, NULL );
		}
		break;

	case WM_FONTCHANGER:
		{
			FontNum++;

			FontChanged = true;

			Invalidate();
		}
		break;
#endif

	case WM_COMMAND:
#ifndef FONTBITMAP_PLUGIN
		if ( ( pChanger != nullptr) && ( lParam == (LPARAM) pChanger->hWnd ) )
		{
			FontNum++;

			Invalidate();

			FontChanged = true;

			if ( pBuddyControl != nullptr )
			{
				SendMessage( pBuddyControl->hWnd, WM_FONTCHANGER, 0, 0 );
			}

			break;
		}
#endif

		switch ( LOWORD(wParam) )
		{
#ifndef FONTBITMAP_PLUGIN
		case ID_EDITMENU_CHARACTERMAP:
			OpenCharacterMap();
			break;

		case IDM_EE_CUT:
		case IDM_EE_COPY:
			{
				OpenClipboard( hWnd );
				EmptyClipboard();

				EditText[ Length + 1 ] = 0;

				BYTE LC, RC;

				if ( Cursor < SCursor ) { LC = Cursor; RC = SCursor; } else { LC = SCursor; RC = Cursor; }

				HGLOBAL hg=GlobalAlloc( GMEM_MOVEABLE, ( RC - LC ) + 1 );

				rstrncpy( (BYTE *) GlobalLock( hg ), &EditText[ LC ] , RC - LC );

				SetEncodedClipboard( &EditText[ LC ], RC - LC );

				GlobalUnlock( hg );

				SetClipboardData( CF_TEXT, hg );

				CloseClipboard();
				
				GlobalFree( hg );

				if ( ( wParam == IDM_EE_CUT ) && ( !Disabled ) )
				{
					for ( int i = RC; i<Length; i++ )
					{
						if ( ( ( LC + ( i - RC ) ) <= MaxLen ) && ( i <= MaxLen ) )
						{
							EditText[ LC + ( i - RC ) ] = EditText[ i ];
						}
					}

					Length -= ( RC - LC );
					Cursor = LC;

					Select  = false;
					Changes = true;

					Invalidate();
				}
			}
			break;

		case IDM_EE_PASTE:
			{
				if ( ( IsClipboardFormatAvailable( CF_TEXT ) ) && ( !Disabled ) )
				{
					BYTE LC, RC;

					if ( Select )
					{
						if ( Cursor < SCursor ) { LC = Cursor; RC = SCursor; } else { LC = SCursor; RC = Cursor; }

						for ( int i = RC; i<Length; i++ )
						{
							if ( ( ( LC + ( i - RC ) ) <= MaxLen ) && ( i <= MaxLen ) )
							{
								EditText[ LC + ( i - RC ) ] = EditText[ i ];
							}
						}

						Length -= ( RC - LC );
						Cursor = LC;

						Changes = true;
					}

					OpenClipboard( hWnd );

					HGLOBAL hg = GetClipboardData( CF_TEXT );

					BYTE *src = (BYTE *) GlobalLock( hg );
					BYTE el;

					if ( src )
					{
						el = strlen( (char *) src );
					}

					if ( EClipStamp > ClipStamp )
					{
						src = pClipboard;
						el  = (BYTE) EClipLen;
					}

					for ( int i = Length - 1; i>=Cursor; i-- )
					{
						if ( ( i + el ) <= MaxLen )
						{
							EditText[ i + el ] = EditText[ i ];
						}
					}

					memcpy( (char *) &EditText[ Cursor ], (void *) src, el );

					Length += el;
					Cursor += el;

					GlobalUnlock( hg );

					CloseClipboard();

					Select  = false;
					Changes = true;
				}

				Invalidate();
			}
			break;
#endif

		case IDM_EE_DELETE:
			{
				if ( ( Select ) && ( !Disabled ) )
				{
					BYTE LC, RC;

					if ( Cursor < SCursor ) { LC = Cursor; RC = SCursor; } else { LC = SCursor; RC = Cursor; }

					for ( int i = RC; i<Length; i++ )
					{
						if ( ( ( LC + ( i - RC ) ) <= MaxLen ) && ( i <= MaxLen ) )
						{
							EditText[ LC + ( i - RC ) ] = EditText[ i ];
						}
					}

					Length -= ( RC - LC );
					Cursor = LC;

					Changes = true;
				}

				Select = false;

				Invalidate();
			}
			break;

		case IDM_EE_SELECT:
			{
				Select   = true;
				SCursor  = 0;
				Cursor   = Length;

				Invalidate();
			}
			break;
		}
		break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

void EncodingEdit::ProcessASCII( WORD ascii )
{
	if ( ( Select ) && ( !Disabled ) )
	{
		BYTE LC, RC;

		if ( Cursor < SCursor ) { LC = Cursor; RC = SCursor; } else { LC = SCursor; RC = Cursor; }

		for ( int i = RC; i<Length; i++ )
		{
			if ( ( ( LC + ( i - RC ) ) <= MaxLen ) && ( i <= MaxLen ) )
			{
				EditText[ LC + ( i - RC ) ] = EditText[ i ];
			}
		}

		Length -= ( RC - LC );
		Cursor = LC;						

		Changes = true;
	}

	Select = false;

	if ( !Disabled )
	{
		if ( Length == 0 ) 
		{
			EditText[ 0 ] = (BYTE) ( ascii & 0xFF );

			Length++;
			Cursor++;

			Changes = true;
		}
		else if ( Length < MaxLen )
		{
			for ( int i=Length; i>Cursor; i-- )
			{
				if ( i <= MaxLen )
				{
					EditText[ i ] = EditText[ i - 1 ];
				}
			}

			EditText[ Cursor ] = (BYTE) ( ascii & 0xFF );

			Length++;
			Cursor++;

			Changes = true;
		}
	}
}

void EncodingEdit::Invalidate( void )
{
	/* Validate the characters are what we accept */

	int i,j;

	if ( ( AllowedChars != textInputAny ) || ( pValidator != nullptr ) )
	{
		for ( i=0; i<Length; i++ )
		{
			if ( pValidator == nullptr )
			{
				if ( AllowNegative )
				{
					if ( ( EditText[i] == '-' ) && ( i == 0 ) )
					{
						continue;
					}
				}

				if ( AllowedChars == textInputBin )
				{
					if ( ( EditText[ i ] >= '0' ) && ( EditText[ i ] <= '1' ) )
					{
						continue;
					}
				}

				if ( AllowedChars == textInputOct )
				{
					if ( ( EditText[ i ] >= '0' ) && ( EditText[ i ] <= '7' ) )
					{
						continue;
					}
				}

				if ( AllowedChars == textInputDec )
				{
					if ( ( EditText[ i ] >= '0' ) && ( EditText[ i ] <= '9' ) )
					{
						continue;
					}
				}

				if ( AllowedChars == textInputHex )
				{
					if ( ( EditText[ i ] >= '0' ) && ( EditText[ i ] <= '9' ) )
					{
						continue;
					}

					if ( ( EditText[ i ] >= 'A' ) && ( EditText[ i ] <= 'F' ) )
					{
						continue;
					}

					if ( ( EditText[ i ] >= 'a' ) && ( EditText[ i ] <= 'f' ) )
					{
						continue;
					}
				}
			}
			else
			{
				if ( pValidator( EditText[ i ] ) )
				{
					continue;
				}
			}

			for ( j = i + 1; j<Length; j++ )
			{
				EditText[ i ] = EditText[ j ];
			}

			Length--;
			Cursor--;

			if ( Cursor > Length )
			{
				Cursor = Length;
			}

			i--;
		}
	}

	if ( Length > MaxLen )
	{
		Length = MaxLen;

		if ( Cursor > ( Length - 1 ) )
		{
			Cursor = Length;
		}
	}

	if ( Changes )
	{
		::PostMessage( Parent, WM_COMMAND, (WPARAM) ( EN_CHANGE << 16U ) | 0, (LPARAM) hWnd );

		Changes = false;
	}

	RECT rc;

	GetClientRect( hWnd, &rc );

	InvalidateRect( hWnd, &rc, FALSE );
}

void EncodingEdit::PaintControl( void )
{
	RECT rc;
	HDC hDC = GetDC( hWnd );

	GetClientRect( hWnd, &rc );

	if ( hArea == NULL )
	{
		hArea       = CreateCompatibleDC( hDC );
		hAreaCanvas = CreateCompatibleBitmap( hDC, rc.right - rc.left, rc.bottom - rc.top );
		hAreaOld    = SelectObject( hArea, hAreaCanvas );
	}

	if ( ( Disabled ) && ( !SoftDisable ) )
	{
		FillRect( hArea, &rc, hDisBrush );
	}
	else
	{
		FillRect( hArea, &rc, (HBRUSH) GetStockObject( WHITE_BRUSH ) );
	}

	BYTE LC, RC;

	if ( Cursor < SCursor ) { LC = Cursor; RC = SCursor; } else { LC = SCursor; RC = Cursor; }

#ifndef FONTBITMAP_PLUGIN
	FontSelection = FSPlugins.FontListForEncoding( Encoding );

	if ( FontNum >= FontSelection.size() )
	{
		FontNum = 0;
	}

	CurrentFontID = FontSelection[ FontNum ];

	if ( ( FontChanged ) && ( CharMap::TheCharMap != nullptr ) )
	{
		CharMap::TheCharMap->SetFont( CurrentFontID );

		FontChanged = false;
	}

	FontBitmap txt( FontSelection[ FontNum ], EditText, Length, false, false );
#else
	FontBitmap txt( pFont, EditText, Length, false, false );
#endif

	

	if ( ( Disabled ) && ( !SoftDisable ) )
	{
		txt.SetButtonColor( 0xCC, 0xCC, 0xCC );
	}

	txt.DrawText( hArea, 4 - ( SChar * 8 ), 4, DT_LEFT | DT_TOP );

	if ( Select )
	{
#ifndef FONTBITMAP_PLUGIN
		FontBitmap seltxt( FontSelection[ FontNum ], &EditText[ LC ], RC - LC, false, true );
#else
		FontBitmap seltxt( pFont, &EditText[ LC ], RC - LC, false, true );
#endif

		seltxt.DrawText( hArea, ( 4 - ( SChar * 8 ) ) + LC * 8, 4, DT_LEFT | DT_TOP );
	}

	if ( ( Blink ) && ( HasFocus ) )
	{
		SelectObject( hArea, GetStockObject( BLACK_PEN ) );

		MoveToEx( hArea, ( 4 - ( SChar * 8 ) ) + Cursor * 8, 4, NULL );
		LineTo( hArea, ( 4 - ( SChar * 8 ) ) + Cursor * 8, 20 );
	}

	DrawEdge( hArea, &rc, EDGE_SUNKEN, BF_RECT );

	BitBlt( hDC, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hArea, 0, 0, SRCCOPY );

	ReleaseDC( hWnd, hDC );
}

void EncodingEdit::SetText( BYTE *pText )
{
	rstrncpy( EditText, pText, MaxLen );

	Length = (BYTE) rstrnlen( EditText, MaxLen );

	if ( Length > MaxLen )
	{
		Length = MaxLen;
	}
	
	if ( Cursor >= Length ) { Cursor = Length - 1; }

	Invalidate();
}

long EncodingEdit::GetHexText( void )
{
	BYTE *pText = GetText();

	bool neg = false; if ( pText[0] == '-' ) { neg = true; pText++; }

	long r = 0;

	try {
		r = std::stoul( std::string( (char *) pText ), nullptr, 16 );
	}

	catch ( const std::invalid_argument &e )
	{
		e.what();

		r = 0;
	}

	if ( neg ) { r = 0 - r; }

	return r;
}

long EncodingEdit::GetOctText( void )
{
	BYTE *pText = GetText();

	bool neg = false; if ( pText[0] == '-' ) { neg = true; pText++; }

	long r = 0;

	try {
		r = std::stoul( std::string( (char *) pText ), nullptr, 8 );
	}

	catch ( const std::invalid_argument &e )
	{
		e.what();

		r = 0;
	}

	if ( neg ) { r = 0 - r; }

	return r;
}

long EncodingEdit::GetDecText( void )
{
	BYTE *pText = GetText();

	bool neg = false; if ( pText[0] == '-' ) { neg = true; pText++; }

	long r = 0;

	try {
		r = std::stoul( std::string( (char *) pText ), nullptr, 10 );
	}

	catch ( const std::invalid_argument &e )
	{
		e.what();

		r = 0;
	}

	if ( neg ) { r = 0 - r; }

	return r;
}

void EncodingEdit::SelectAll( void )
{
	SCursor = 0;
	Cursor  = Length;
	Select  = true;

	Invalidate();
}

void EncodingEdit::SetFocus( void )
{
	::SetFocus( hWnd );
}

void EncodingEdit::SetBuddy( EncodingEdit *pBuddy )
{
	pBuddyControl = pBuddy;
}

#ifndef FONTBITMAP_PLUGIN
void EncodingEdit::OpenCharacterMap( void )
{
	hCharmapFocusWnd = hWnd;
	CharmapFontID    = CurrentFontID;

	::PostMessage( hMainWnd, WM_OPENCHARMAP, 0, 0 );
}
#endif