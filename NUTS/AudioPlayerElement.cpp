#include "StdAfx.h"

#include "AudioPlayerElement.h"
#include "resource.h"
#include "Plugins.h"
#include "FontBitmap.h"
#include "libfuncs.h"

#include <WindowsX.h>

bool AudioPlayerElement::_HasWindowClass = false;

std::map<HWND, AudioPlayerElement *> AudioPlayerElement::_AudioPlayerElementClassMap;

const wchar_t AudioPlayerElementClass[]  = L"Audio Player Element";

WORD    AudioPlayerElement::_RefCount      = 0;
HDC     AudioPlayerElement::_ButtonSource  = NULL;
HBITMAP AudioPlayerElement::_ButtonCanvas  = NULL;

LRESULT CALLBACK AudioPlayerElementWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::map<HWND, AudioPlayerElement *>::iterator iWindow = AudioPlayerElement::_AudioPlayerElementClassMap.find( hWnd );

	if ( iWindow != AudioPlayerElement::_AudioPlayerElementClassMap.end() )
	{
		switch ( iWindow->second->ElementType )
		{
			case AudioElementButton:
				return iWindow->second->ButtonWindowProc( uMsg, wParam, lParam );

			case AudioElementScrollBar:
				return iWindow->second->ScrollBarWindowProc( uMsg, wParam, lParam );

			case AudioElementListBox:
				return iWindow->second->ListBoxWindowProc( uMsg, wParam, lParam );

			default:
				break;
		}
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

AudioPlayerElement::AudioPlayerElement( HWND hParent, int x, int y, int w, int h, AudioElementType e, DWORD s  )
{
	if ( !_HasWindowClass )
	{
		WNDCLASS wc = { };

		ZeroMemory( &wc, sizeof( wc ) );

		wc.lpfnWndProc   = AudioPlayerElementWindowProc;
		wc.hInstance     = hInst;
		wc.lpszClassName = AudioPlayerElementClass;

		ATOM atom = RegisterClass(&wc);

		_HasWindowClass = true;
	}

	ElementType  = e;
	ElementStyle = s;

	if ( e == AudioElementScrollBar )
	{
		if ( s & AEWS_HORIZONTAL )
		{
			h = 8;
		}
		else
		{
			w = 8;
		}
	}

	if ( e == AudioElementButton )
	{
		w = 16;
		h = 16;
	}

	hWnd = CreateWindowEx(
		0,
		AudioPlayerElementClass,
		L"Audio Player Element",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP,

		x, y, w, h,

		hParent, NULL, hInst, NULL
	);

	Parent = hParent;

	AudioPlayerElement::_AudioPlayerElementClassMap[ hWnd ] = this;

	HDC hDC = GetDC( hWnd );

	hCanvasDC = CreateCompatibleDC( hDC );
	hCanvas   = CreateCompatibleBitmap( hDC, w, h );

	hOld      = SelectObject( hCanvasDC, hCanvas );

	if ( AudioPlayerElement::_ButtonSource == NULL )
	{
		AudioPlayerElement::_ButtonSource = CreateCompatibleDC( hDC );

		AudioPlayerElement::_ButtonCanvas = LoadBitmap( hInst, MAKEINTRESOURCE( IDB_AUDIOBUTTONS ) );

		SelectObject( AudioPlayerElement::_ButtonSource, AudioPlayerElement::_ButtonCanvas );
	}

	ReleaseDC( hWnd, hDC );

	ew = w;
	eh = h;

	OverElement  = false;
	Active       = false;
	Scrolling    = false;
	Disabled     = false;
	ActiveUpdate = false;
	mo           = 0;

	hMainPen = CreatePen( PS_SOLID, 1, RGB( 0x00, 0xCC, 0xCC ) );
	hSecPen  = CreatePen( PS_SOLID, 1, RGB( 0x00, 0xAA, 0xAA ) );

	LOGBRUSH brsh;

	brsh.lbColor = RGB( 0x00, 0xAA, 0xAA );
	brsh.lbStyle = BS_SOLID;

	hMainBrush = CreateBrushIndirect( &brsh );

	brsh.lbColor = RGB( 0x00, 0xEE, 0xEE );

	hSecBrush = CreateBrushIndirect( &brsh );

	brsh.lbColor = RGB( 0x00, 0xCC, 0xCC );

	hTextBrush = CreateBrushIndirect( &brsh );

	AudioPlayerElement::_RefCount++;

	pScrollBar = nullptr;
	hFont      = NULL;
	hBoldFont  = NULL;
	se         = -1;
	fe         = 0;
	pe         = -1;
	maxe       = 0;
	nume       = 0;

	LastClickTime = 0;
	FirstClick    = false;

	if ( e == AudioElementListBox )
	{
		pScrollBar = new AudioPlayerElement( hWnd, ew - 10, 2, 8, eh - 4, AudioElementScrollBar, AEWS_VERTICAL );

		hFont    = CreateFont(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));

		hBoldFont = CreateFont(16,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
	}

	hTip = NULL;
}

AudioPlayerElement::~AudioPlayerElement(void)
{
	SelectObject( hCanvasDC, hOld );

	NixObject( hCanvasDC );
	NixObject( hCanvas );
	NixObject( hMainPen );
	NixObject( hSecPen );
	NixObject( hMainBrush );
	NixObject( hTextBrush );
	NixObject( hFont );
	NixObject( hBoldFont );

	NixWindow( hTip );
	NixWindow( hWnd );

	if ( pScrollBar != nullptr )
	{
		delete pScrollBar;
	}

	AudioPlayerElement::_RefCount--;

	if ( AudioPlayerElement::_RefCount == 0 )
	{
		NixObject( AudioPlayerElement::_ButtonSource );
		NixObject( AudioPlayerElement::_ButtonCanvas );

		AudioPlayerElement::_ButtonSource = NULL;
		AudioPlayerElement::_ButtonCanvas = NULL;
	}
}


LRESULT AudioPlayerElement::ButtonWindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
		case WM_PAINT:
			PaintButton();
			break;

		case WM_MOUSEMOVE:
			{
				Track();

				OverElement = true;

				Invalidate();
			}
			break;

		case WM_MOUSELEAVE:
			OverElement = false;

			Invalidate();

			break;

		case WM_LBUTTONUP:
			PostMessage( Parent, WM_AUDIOELEMENT, (WPARAM) this, (LPARAM) ElementStyle );
			
			break;

		default:
			break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

LRESULT AudioPlayerElement::ScrollBarWindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
		case WM_PAINT:
			PaintScrollBar();
			break;

		case WM_MOUSEMOVE:
			{
				if ( Scrolling )
				{
					SetMO( lParam );
				}
				
				if ( !OverElement )
				{
					Track();
				}
				
				OverElement = true;

				Invalidate();
			}
			break;

		case WM_MOUSELEAVE:
			OverElement = false;

			Invalidate();

			break;

		case WM_LBUTTONDOWN:
			{
				SetCapture( hWnd );

				Scrolling = true;
			}
			break;

		case WM_LBUTTONUP:
			Scrolling = false;

			SetMO( lParam );

			ReleaseCapture();

			PostMessage( Parent, WM_AUDIOELEMENT, (WPARAM) this, (LPARAM) mo );

			break;

		default:
			break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

LRESULT AudioPlayerElement::ListBoxWindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
		case WM_PAINT:
			PaintListBox();
			break;

		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			DoListboxMouse( uMsg, lParam );
			break;

		case WM_AUDIOELEMENT:
			{
				AudioPlayerElement *pE = (AudioPlayerElement *) wParam;

				if ( pE == pScrollBar )
				{
					fe = (int) lParam;
				}

				Invalidate();
			}
			break;

		default:
			break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

void AudioPlayerElement::PaintButton()
{
	RECT r;

	GetClientRect( hWnd, &r );

	HDC hDC = GetDC( hWnd );

	FillRect( hCanvasDC, &r, (HBRUSH) GetStockObject( BLACK_BRUSH ) );

	int y = 0;
	int x = 0;

	if ( OverElement ) { y = 16; }
	if ( Active )      { y = 32; }
	if ( Disabled )    { y = 0;  }

	if ( ElementStyle & AEWS_PLAY  ) { x = 0; }
	if ( ElementStyle & AEWS_STOP  ) { x = 16; }
	if ( ElementStyle & AEWS_PREV  ) { x = 32; }
	if ( ElementStyle & AEWS_NEXT  ) { x = 48; }
	if ( ElementStyle & AEWS_PAUSE ) { x = 64; }
	if ( ElementStyle & AEWS_EJECT ) { x = 80; }
	if ( ElementStyle & AEWS_SAVE  ) { x = 96; }
	if ( ElementStyle & AEWS_LOOP  ) { x = 112; }

	BitBlt( hCanvasDC, 0, 0, 16, 16, _ButtonSource, x, y, SRCCOPY );

	BlitCanvas( hDC );

	ReleaseDC( hWnd, hDC );
}

void AudioPlayerElement::PaintScrollBar()
{
	RECT r;

	GetClientRect( hWnd, &r );

	HDC hDC = GetDC( hWnd );

	FillRect( hCanvasDC, &r, (HBRUSH) GetStockObject( BLACK_BRUSH ) );

	SelectObject( hCanvasDC, hMainPen );

	/* Calculate the offset of the thumb */
	int sz = ew;

	if ( ElementStyle & AEWS_VERTICAL ) { sz = eh; }

	double mpos  = (double) maxpos;
	double pmpos = (double) ( sz - 16 );
	double prat  = pmpos / mpos;
	
	DWORD ppos = 0;

	if ( ( mo >= 0 ) && ( mo <= (int) mpos ) )
	{
		ppos = (DWORD) ( (double) mo * prat );
	}

	if ( !Active ) { ppos = 0; }

	if ( ElementStyle & AEWS_HORIZONTAL )
	{
		MoveToEx( hCanvasDC, 8, eh/2, NULL );
		LineTo( hCanvasDC, ew - 8, eh/2 );

		r.left = ppos;
		r.right = ppos + 16;
		r.top = 0;
		r.bottom = 8;

		FillRect( hCanvasDC, &r, (OverElement)?hSecBrush:hMainBrush );
	}

	if ( ElementStyle & AEWS_VERTICAL )
	{
		MoveToEx( hCanvasDC, ew/2, 8, NULL );
		LineTo( hCanvasDC, ew/2, eh - 8 );

		r.left = 0;
		r.right = 8;
		r.top = ppos;
		r.bottom = ppos + 16;

		FillRect( hCanvasDC, &r, (OverElement)?hSecBrush:hMainBrush );
	}

	BlitCanvas( hDC );

	ReleaseDC( hWnd, hDC );
}

void AudioPlayerElement::PaintListBox()
{
	RECT r;

	GetClientRect( hWnd, &r );

	HDC hDC = GetDC( hWnd );

	FillRect( hCanvasDC, &r, (HBRUSH) GetStockObject( BLACK_BRUSH ) );

	SelectObject( hCanvasDC, hMainPen );

	MoveToEx( hCanvasDC, 0, 0, NULL );
	LineTo( hCanvasDC, ew - 1, 0 );
	LineTo( hCanvasDC, ew - 1, eh - 1 );
	LineTo( hCanvasDC, 0, eh -1  );
	LineTo( hCanvasDC, 0, 0 );

	int y = 2;
	int i = 0;

	while ( ( y + 16 ) < ( eh - 4 ) )
	{
		if ( cues.size() <= i ) 
		{
			break;
		}

		int ii = fe + i;

		r.left   = 3;
		r.right  = ew -16;
		r.top    = y;
		r.bottom = y + 18;

		if ( ii == se ) 
		{
			FillRect( hCanvasDC, &r, hTextBrush );
		}
		else
		{
			FillRect( hCanvasDC, &r, (HBRUSH) GetStockObject( BLACK_BRUSH ) );
		}

		DrawBiText( &cues[ ii ], 3, y, ew - 16, DT_LEFT | DT_TOP, ii == se, ii == pe );

		y += 18;
		i++;
	}

	BlitCanvas( hDC );

	ReleaseDC( hWnd, hDC );
}

void AudioPlayerElement::BlitCanvas( HDC hDC )
{
	BitBlt( hDC, 0, 0, ew, eh, hCanvasDC, 0, 0, SRCCOPY );
}

void AudioPlayerElement::Invalidate()
{
	RECT r;
	GetClientRect( hWnd, &r );
	InvalidateRect( hWnd, &r, FALSE );
}

void AudioPlayerElement::Track()
{
	TRACKMOUSEEVENT e;

	e.cbSize    = sizeof( e );
	e.dwFlags   = TME_LEAVE;
	e.hwndTrack = hWnd;

	TrackMouseEvent( &e );
}

void AudioPlayerElement::SetActive( bool f )
{
	Active = f;

	Invalidate();
}

void AudioPlayerElement::SetLimit( DWORD max )
{
	maxpos = max;
}

void AudioPlayerElement::SetPosition( DWORD pos )
{
	if ( !Scrolling )
	{
		mo = (int) pos;

		Invalidate();
	}
}

void AudioPlayerElement::SetMO( LPARAM lParam )
{
	int sz = ew;

	if ( ElementStyle & AEWS_HORIZONTAL )
	{
		mo = GET_X_LPARAM( lParam );
		sz = ew;
	}

	if ( ElementStyle & AEWS_VERTICAL )
	{
		mo = GET_Y_LPARAM( lParam );
		sz = eh;
	}

	mo -= 8;

	/* Calculate the offset of the thumb */
	double mpos  = (double) maxpos;
	double pmpos = (double) ( sz - 16 );
	double prat  = mpos / pmpos;

	mo = (int) ( ( (double) mo * prat ) );

	mo = max( 0, mo );
	mo = min( mo, maxpos );

	if ( ActiveUpdate )
	{
		PostMessage( Parent, WM_AUDIOELEMENT, (WPARAM) this, (LPARAM) mo );
	}
}

void AudioPlayerElement::SetDisabled( bool f )
{
	Disabled = f;

	Invalidate();
}

void AudioPlayerElement::SetActiveUpdate( bool f )
{
	ActiveUpdate = f;
}

void AudioPlayerElement::AddCuePoint( AudioCuePoint cue )
{
	cues.push_back( cue );

	if ( pScrollBar )
	{
		nume = ( eh - 4 ) / 18;

		pScrollBar->SetActive( cues.size() > nume );
		pScrollBar->SetActiveUpdate( true );

		int icues = cues.size() - 1;

		maxe = icues - nume;

		if ( icues > nume )
		{
			pScrollBar->SetLimit( maxe );
		}
	}

	Invalidate();
}

void AudioPlayerElement::DrawBiText( AudioCuePoint *cue, int x, int y, int w, DWORD Displace, bool Invert, bool Bold )
{
	if ( cue->UseEncoding )
	{
		std::vector<FontIdentifier> fonts = FSPlugins.FontListForEncoding( cue->EncodingID );

		if ( fonts.size() > 0 )
		{
			FontBitmap t( fonts[ 0 ], cue->EncodingName, w / 8, true, false );

			if ( Invert )
			{
				t.SetButtonColor( 0x00, 0xCC, 0xCC );
				t.SetTextColor( 0x00, 0x00, 0x00 );
			}
			else
			{
				t.SetButtonColor( 0, 0, 0 );
				t.SetTextColor( 0x00, 0xCC, 0xCC );
			}

			if ( Bold )
			{
				t.SetTextColor( 0xFF, 0xFF, 0xFF );
			}

			t.DrawText( hCanvasDC, x, y, Displace );
		}
	}
	else
	{
		SelectObject( hCanvasDC, hFont );

		if ( Invert )
		{
			SetTextColor( hCanvasDC, 0 );
			SetBkColor( hCanvasDC, RGB( 0x00, 0xCC, 0xCC ) );
		}
		else
		{
			SetTextColor( hCanvasDC, RGB( 0x00, 0xCC, 0xCC ) );
			SetBkColor( hCanvasDC, 0 );
		}

		if ( Bold )
		{
			SetTextColor( hCanvasDC, RGB( 0xFF, 0xFF, 0xFF ) );
		}

		RECT r;

		r.top    = y;
		r.left   = x;
		r.right  = x + w;
		r.bottom = y + 16;

		DrawText( hCanvasDC, cue->Name.c_str(), (int) cue->Name.length(), &r, Displace );
	}
}

void AudioPlayerElement::DoListboxMouse( UINT uMsg, LPARAM lParam )
{
	if ( uMsg == WM_LBUTTONUP )
	{
		WORD x = LOWORD( lParam );
		WORD y = HIWORD( lParam );

		se = -1;

		if ( ( x >= 2 ) && ( x <= ( ew - 12 ) ) )
		{
			if ( ( y >= 2 ) && ( y <= ( eh - 4 ) ) )
			{
				se = ( y - 2 ) / 18;

				se += fe;
			}
		}

		DWORD now = GetTickCount();
		DWORD clk = now - LastClickTime;

		if ( now < LastClickTime )
		{
			clk = ( 0 - LastClickTime ) + now;
		}

		if ( ( clk < GetDoubleClickTime() ) && ( FirstClick ) )
		{
			::PostMessage( Parent, WM_AUDIOELEMENT, (WPARAM) this, (LPARAM) se );

			FirstClick = false;

			LastClickTime = 0;
		}
		else
		{
			FirstClick = true;

			LastClickTime = now;
		}
	}

	Invalidate();
}

void AudioPlayerElement::SetCuePoint( QWORD p )
{
	int newi = -1;

	for ( int i=0; i<cues.size(); i++ )
	{
		if ( p >= cues[ i ].Position )
		{
			newi = i;
		}
	}

	if ( newi != pe )
	{
		pe = newi;

		if ( !FirstClick )
		{
			if ( pe < fe )
			{
				fe = pe;
			}
			else
			{
				while ( ( fe + ( nume - 1 ) ) < pe )
				{
					fe++;
				}
			}

			pScrollBar->SetPosition( fe );
		}

		Invalidate();
	}
}

void AudioPlayerElement::AddTooltip( std::wstring Tip )
{
	hTip = CreateToolTip( hWnd, Parent, (WCHAR *) Tip.c_str(), hInst );
}
