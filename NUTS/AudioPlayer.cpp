#include "StdAfx.h"
#include "AudioPlayer.h"
#include "Preference.h"
#include "FileDialogs.h"
#include "TapeKey.h"
#include "resource.h"

#include <math.h>
#include <CommCtrl.h>

#define APW 500
#define APH 250

std::map<HWND, AudioPlayer *> AudioPlayer::players;

bool AudioPlayer::WndClassReg = false;

LRESULT CALLBACK AudioPlayer::AudioPlayerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( players.find( hWnd ) != players.end() )
	{
		return players[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

AudioPlayer::AudioPlayer( CTempFile &FileObj, TapeIndex &SourceIndexes ) {
	if ( !WndClassReg )
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= AudioPlayer::AudioPlayerProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInst;
		wcex.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(IDI_SPRITE));
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
		wcex.lpszMenuName	= NULL;
		wcex.lpszClassName	= L"NUTS Cassette Image Player";
		wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SPRITE));

		RegisterClassEx(&wcex);

		WndClassReg = true;
	}

	pStore = new CTempFile( FileObj );

	pStore->Keep();

	Indexes = SourceIndexes;

	for ( int i = 0; i<6; i++ )
	{
		Keys[ i ] = nullptr;
	}

	hTape     = NULL;
	VLtGPen   = NULL;
	LtGPen    = NULL;
	DkGPen    = NULL;
	TapeFont1 = NULL;
	TapeFont2 = NULL;

	hCanvasBitmap = NULL;

	LReelAngle = 0.0;
	RReelAngle = 0.0;
	LReelSpeed = 2.0;
	RReelSpeed = 8.0;

	/* Precalculate this so that it has consistency */
	for ( int i=0; i<50; i++ )
	{
		BYTE Dither = (BYTE) ( ( rand() % 5 ) * 5 );

		TapeDither[ i ] = Dither;

		if ( DitherPens.find( Dither ) == DitherPens.end() )
		{
			DitherPens[ Dither ] = CreatePen( PS_SOLID, 1, RGB( 0x3B + Dither, 0x25 + Dither, 0x17 + Dither ) );
		}
	}

	TapePtr = 0;
	TapeExt = pStore->Ext();

	hTape = NULL;

	VLtGPen = CreatePen( PS_SOLID, 2, 0xF0F0F0 );
	LtGPen  = CreatePen( PS_SOLID, 2, 0xC0C0C0 );
	DkGPen  = CreatePen( PS_SOLID, 2, 0x808080 );
	RedPen  = CreatePen( PS_SOLID, 1, RGB(255,0,0) );
	
	LOGBRUSH lb;
	lb.lbStyle = BS_SOLID;
	lb.lbColor = RGB( 0, 0xA0, 0 );
	GreenBrush = CreateBrushIndirect( &lb );

	
	IndexFont = CreateFont(12,6, 0,0,FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"Arial");

	InitializeCriticalSection( &cs );

	PlayCursor = 0;
	BackCursor = 0;
	TC         = GetTickCount();
	WasPlaying = false;
	Playing    = false;
	pBrowser   = nullptr;
}


AudioPlayer::~AudioPlayer(void)
{
	DeleteCriticalSection( &cs );

	SoundPlayer.Stop();

	if ( pBrowser != nullptr )
	{
		delete pBrowser;

		pBrowser = nullptr;
	}

	for ( int i=0;i<6;i++ )
	{
		if ( Keys[i] != nullptr )
		{
			delete Keys[i];
		}
	}

	NixObject( hCanvasBitmap );
	NixObject( hTape );
	NixObject( VLtGPen );
	NixObject( LtGPen );
	NixObject( DkGPen );
	NixObject( RedPen );
	NixObject( GreenBrush );
	NixObject( TapeFont1 );
	NixObject( TapeFont2 );
	
	DeleteDC( hCanvas );

	NixWindow( hVolumeControl );
	NixWindow( hOptions );
	NixWindow( hWnd );

	/* This deletes the temporary store */
	CTempFile erase( *pStore );
}

int AudioPlayer::Create( HWND Parent, HINSTANCE hInstance )
{
	hWnd = CreateWindowEx(
		NULL,
		L"NUTS Cassette Image Player",
		L"NUTS Cassette Image Player",
		WS_SYSMENU | WS_CLIPCHILDREN | WS_OVERLAPPED | WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED,
		CW_USEDEFAULT, CW_USEDEFAULT, APW, APH,
		Parent, NULL, hInstance, NULL
	);

	players[ hWnd ] = this;

	hParent = Parent;

	hVolume   = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_VOLUME ) );

	HDC hDC = GetDC( hWnd );

	hCanvas = CreateCompatibleDC( hDC );

	hCanvasBitmap = CreateCompatibleBitmap( hDC, APW, APH );

	SelectObject( hCanvas, hCanvasBitmap );

	SetTimer( hWnd, 0xA110, 80, NULL );

	RECT r;

	GetClientRect( hWnd, &r );

	SoundPlayer.Init( hWnd, this );

	hVolumeControl = CreateWindowEx(
		NULL,
		TRACKBAR_CLASS,
		L"Volume Control",
		WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_NOTICKS | TBS_TRANSPARENTBKGND | WS_TABSTOP | WS_GROUP,
		r.right - 16, r.top + 16, 16, ( r.bottom - r.top ) - ( 50 + 16 + 16 ),
		hWnd, NULL, hInst, NULL
	);

	hOptions = CreateWindowEx(
		NULL,
		L"BUTTON",
		L"Volume Control",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP,
		r.right - 16, r.bottom - 50 - 16, 16, 16,
		hWnd, NULL, hInst, NULL
	);

	DWORD APKT = r.bottom - 48;

	Keys[ 0 ] = new TapeKey( hWnd, TapeKeyStart,   1,   APKT, true );
	Keys[ 1 ] = new TapeKey( hWnd, TapeKeyBack,    51,  APKT, false );
	Keys[ 2 ] = new TapeKey( hWnd, TapeKeyPlay,    101, APKT, false );
	Keys[ 3 ] = new TapeKey( hWnd, TapeKeyForward, 151, APKT, false );
	Keys[ 4 ] = new TapeKey( hWnd, TapeKeyStop,    201, APKT, true );
	Keys[ 5 ] = new TapeKey( hWnd, TapeKeyEject,   251, APKT, true );

	TapeVolume = Preference( L"TapePlayerVolume", (DWORD) 64 );

	::SendMessage( hOptions, WM_SETTEXT, 0, (LPARAM) L"▼" );
	::SendMessage( hVolumeControl, TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG( 0, 127 ) );
	::SendMessage( hVolumeControl, TBM_SETPAGESIZE, (WPARAM) 0, (LPARAM) 10 );
	::SendMessage( hVolumeControl, TBM_SETPOS, (WPARAM) TRUE, 127 - TapeVolume );

	SoundPlayer.SetVolume( TapeVolume );

	return 0;
}

LRESULT	AudioPlayer::WndProc(HWND hWindow, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_ACTIVATE:
		if ( wParam == 0 )
		{
			hActiveWnd = NULL;
		}
		else
		{
			hActiveWnd = hWindow;
		}
		break;

	case WM_PAINT:
		{
			PaintPlayer();
		}
		break;

	case WM_CLOSE:
		{
			delete this;
		}
		break;

	case WM_VSCROLL:
		{
			TapeVolume = 127 - ::SendMessage( hVolumeControl, TBM_GETPOS, 0, 0 );

			Preference(L"TapePlayerVolume") = TapeVolume;

			SoundPlayer.SetVolume( TapeVolume );
		}
		break;

	case WM_TAPEKEY_DOWN:
	case WM_TAPEKEY_UP:
		{
			TapeKeyID b = (TapeKeyID) wParam;

			if ( message == WM_TAPEKEY_DOWN ) {
				if ( b == 2 ) {
					WasPlaying = Playing;
				}

				for ( int i=0; i<6; i++ )
				{
					if ( i != (int) b )
					{
						Keys[i]->Reset();
					}
				}
			}

			if ( message == WM_TAPEKEY_UP )   {
				if ( (int) b >= 4 ) /* Stop and eject buttons */
				{
					Keys[b]->Reset();;
				}

				if ( b == TapeKeyStart ) /* First button */
				{
					Keys[b]->Reset();
				}

				if ( b == TapeKeyEject ) /* Eject button */
				{
					PostMessage( hWnd, WM_CLOSE, 0, 0 );
				}

				if ( Keys[2]->IsPressed() )
				{
					if ( !WasPlaying )
					{
						TapePtr    = PlayCursor;
						BackCursor = PlayCursor;

						SoundPlayer.PositionReset();
						SoundPlayer.Play();

						TC = GetTickCount();
					}

					Playing = true;
				}
				else
				{
					WasPlaying = false;
					Playing    = false;

					SoundPlayer.Pause();
				}

				if ( b == 0 )
				{
					EnterCriticalSection( &cs );

					TapePtr    = 0;
					PlayCursor = 0;
					BackCursor = 0;

					LeaveCriticalSection( &cs );

					SoundPlayer.PositionReset();
				}
			}

			Refresh();
		}
		return 0;

	case WM_TIMER:
		{
			if ( wParam == 0xA110 )
			{
				if ( ( Playing ) || ( Keys[1]->IsPressed() ) || ( Keys[3]->IsPressed() ) )
				{
					EnterCriticalSection( &cs );
					
					/* Actually do the wind here */
					if ( Keys[1]->IsPressed() )
					{
						TapePtr -= min( TapePtr, 88200 );

						SoundPlayer.PositionReset();

						if ( TapePtr == 0 )
						{
							Keys[1]->Reset();
						}

						PlayCursor = TapePtr;
						BackCursor = TapePtr;
					}

					if ( Keys[3]->IsPressed() )
					{
						TapePtr += min( (TapeExt - TapePtr), 88200 );

						SoundPlayer.PositionReset();

						if ( TapePtr >= TapeExt )
						{
							Keys[3]->Reset();
						}

						PlayCursor = TapePtr;
						BackCursor = TapePtr;
					}

					if ( Playing ) {
						DWORD now = GetTickCount();
						DWORD elapse = now - TC;

						if ( TC > now ) {
							/* Wraparound */
							elapse = TC - now;
						}

						PlayCursor += (int) ( (double) elapse * 44.1 );

						if ( PlayCursor > TapeExt )
						{
							Keys[2]->Reset();

							Playing = false;
						}

						TC = now;
					}

					double LSpd = ( (double ) TapePtr / (double) TapeExt );
					double RSpd = ( (double ) (TapeExt - TapePtr ) / (double) TapeExt );

					LeaveCriticalSection( &cs );

					LReelSpeed = 2.0 + ( 6.0 * LSpd );
					RReelSpeed = 2.0 + ( 6.0 * RSpd );

					if ( ( Keys[ 1 ]->IsPressed() ) || ( Keys[ 3 ]->IsPressed() ) )
					{
						LReelSpeed *= 10.0;
						RReelSpeed *= 10.0;
					}

					if ( Keys[ 1 ]->IsPressed() )
					{
						LReelSpeed = 0.0 - LReelSpeed;
						RReelSpeed = 0.0 - RReelSpeed;
					}

					LReelAngle += LReelSpeed;
					RReelAngle += RReelSpeed;

					if ( LReelAngle >=  360.0 ) { LReelAngle -= 360.0; }
					if ( RReelAngle >=  360.0 ) { RReelAngle -= 360.0; }
					if ( LReelAngle <= -360.0 ) { LReelAngle += 360.0; }
					if ( RReelAngle <= -360.0 ) { RReelAngle += 360.0; }

					RECT r;

					GetClientRect( hWnd, &r );
					InvalidateRect( hWnd, &r, FALSE );
				}
			}
		}
		break;

	case WM_TBCLOSED:
		if ( pBrowser != nullptr )
		{
			// Browser has already deleted itself
			pBrowser = nullptr;
		}
		break;

	case WM_COMMAND:
		if ( lParam == (LPARAM) hOptions )
		{
			DoOptionsPopup();
		}

		{
			WORD i = LOWORD( wParam );

			if ( i == IDM_SAVEAUDIO )
			{
				DoSaveAudio();
			}
			else if ( i == IDM_CUEJUMP )
			{
				if ( pBrowser == nullptr )
				{
					pBrowser = new TapeBrowser();

					RECT r;

					GetWindowRect( hWnd, &r );

					pBrowser->Create( hWnd, r.left + APW + 16, r.top, &Indexes.Cues );
				}
			}
		}		

		break;

	case WM_CUEINDEX_JUMP:
		{
			::SendMessage( hWnd, WM_TAPEKEY_DOWN, (WPARAM) TapeKeyStop, 0 );
			::SendMessage( hWnd, WM_TAPEKEY_UP, (WPARAM) TapeKeyStop, 0 );

			std::vector<TapeCue>::iterator iCue = Indexes.Cues.begin() + lParam;

			EnterCriticalSection( &cs );

			TapePtr    = iCue->IndexPtr;
			PlayCursor = iCue->IndexPtr;
			BackCursor = iCue->IndexPtr;

			LeaveCriticalSection( &cs );

			Keys[ 2 ]->Press();

			::SendMessage( hWnd, WM_TAPEKEY_DOWN, (WPARAM) TapeKeyPlay, 0 );
			::SendMessage( hWnd, WM_TAPEKEY_UP, (WPARAM) TapeKeyPlay, 0 );

			Refresh();
		}
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

inline void AudioPlayer::SensiCircle( int x, int y, int r )
{
	Ellipse( hCanvas, x - r, y - r, x + r, y + r );
}

void AudioPlayer::DrawTape( int x, int y, int Size )
{
	if ( hTape == NULL )
	{
		LOGBRUSH lb;

		lb.lbColor = RGB( 0x3B, 0x25, 0x17 );
		lb.lbStyle = BS_SOLID;

		hTape = CreateBrushIndirect( &lb );
	}

	SelectObject( hCanvas, hTape );

	double dSize = (double) Size;

	dSize = 26 + 24 * ( dSize / 100 );

	/* This draws the bulk of the tape */
	SelectObject( hCanvas, DitherPens[ 0 ] );

	SensiCircle( x, y, (int) dSize );

	/* This draws the detail */
	for ( int r=0; r<(int) dSize; r+= 2 )
	{
		BYTE DitherNum = TapeDither[ r ];

		if ( DitherPens.find( DitherNum ) != DitherPens.end() )
		{
			SelectObject( hCanvas, DitherPens[ DitherNum ] );
		}
		else
		{
			SelectObject( hCanvas, DitherPens[ 0 ] );
		}

		MoveToEx( hCanvas, x, y + (cos(0.0) * r), NULL );

		for ( double a=0.0; a<2*3.141592654; a += ( 3.141592654 / 90 ) )
		{
			LineTo( hCanvas, x + sin(a) * r, y + cos(a) * r );
		}
	}
}

void AudioPlayer::PaintPlayer( void )
{
	EnterCriticalSection( &cs );

	DWORD tp = TapePtr;

	LeaveCriticalSection( &cs );

	RECT r;

	GetClientRect( hWnd, &r );

	/* Start blank */
	RECT r2 = r;

	r2.right = r2.left + 300;

	FillRect( hCanvas, &r2, (HBRUSH) GetStockObject( BLACK_BRUSH ) );

	/* Draw the deck */
	r2 = r;

	int cx1 = r.left + 85;
	int cx2 = r.left + 215;
	int cy  = r.top + 75;

	r2.top += (cy - 30 );
	r2.left += (cx1 - 40);
	r2.right = r.left + (cx2 + 40); /* These don't need to be precise */
	r2.bottom = r2.top + 60;

	FillRect( hCanvas, &r2, (HBRUSH) GetStockObject( GRAY_BRUSH ) );

	/* Draw the tape */
	int LSize = (int) ( ( (double ) (TapeExt - tp ) / (double) TapeExt ) * 100.0 );
	int RSize = (int) ( ( (double ) tp / (double) TapeExt ) * 100.0 );

	DrawTape( cx1, cy, LSize );
	DrawTape( cx2, cy, RSize );

	/* Draw the reels */
	HGDIOBJ hEmpty = SelectObject( hCanvas, GetStockObject( WHITE_BRUSH) );

	SensiCircle( cx1, cy, 25 );
	SensiCircle( cx2, cy, 25 );

	SelectObject( hCanvas, GetStockObject( GRAY_BRUSH) );

	SensiCircle( cx1, cy, 15 );
	SensiCircle( cx2, cy, 15 );

	/* Draw the reel grabs */
	SelectObject( hCanvas, GetStockObject( WHITE_BRUSH) );
	SelectObject( hCanvas, GetStockObject( WHITE_PEN ) );

	for ( double a = LReelAngle; a<(LReelAngle+360.0); a+=(360.0/6.0) )
	{
		double aa = (a / 180) * 3.141592654;

		double xx = cx1 + 13 * sin( aa );
		double yy = cy + 13 * cos( aa );

		SensiCircle( xx, yy, 3 );
	}

	for ( double a = RReelAngle; a<(RReelAngle+360.0); a+=(360.0/6.0) )
	{
		double aa = (a / 180) * 3.141592654;

		double xx = cx2 + 13 * sin( aa );
		double yy = cy + 13 * cos( aa );

		SensiCircle( xx, yy, 3 );
	}

	SelectObject( hCanvas, GetStockObject( BLACK_BRUSH) );
	SelectObject( hCanvas, GetStockObject( BLACK_PEN ) );

	SensiCircle( cx1, cy, 10 );
	SensiCircle( cx2, cy, 10 );

	for ( double a = LReelAngle + 30.0; a<(LReelAngle+390.0); a+=(360.0/3.0) )
	{
		double aa = (a / 180) * 3.141592654;

		double xx = cx1 + 12 * sin( aa );
		double yy = cy + 12 * cos( aa );

		SensiCircle( xx, yy, 3 );
	}

	for ( double a = RReelAngle + 30.0; a<(RReelAngle+390.0); a+=(360.0/3.0) )
	{
		double aa = (a / 180) * 3.141592654;

		double xx = cx2 + 12 * sin( aa );
		double yy = cy + 12 * cos( aa );

		SensiCircle( xx, yy, 3 );
	}

	SelectObject( hCanvas, GetStockObject( GRAY_BRUSH) );

	SensiCircle( cx1, cy, 2 );
	SensiCircle( cx2, cy, 2 );

	/* Draw the label */

	SelectObject( hCanvas, GetStockObject( WHITE_PEN ) );

	for (int ly = -28; ly < 28; ly++ )
	{
		/* Use pythagoras to get line extents */
		double dy = (double) ly;

		double dx = sqrt( ( 28.0 * 28.0 ) - (dy * dy) );

		MoveToEx( hCanvas, (int) ( cx1 - dx), cy + ly, NULL );
		LineTo( hCanvas, r.left - 1, cy + ly );

		MoveToEx( hCanvas, (int) ( cx2 + dx), cy + ly, NULL );
		LineTo( hCanvas, r.left + 300, cy + ly );
	}

	r2 = r;
	r2.right = r2.left + 300;
	r2.bottom = cy - 28;
	FillRect( hCanvas, &r2, (HBRUSH) GetStockObject( WHITE_BRUSH ) );

	r2 = r;
	r2.right = r2.left + 300;
	r2.top = cy + 28;
	r2.bottom -= 50;
	FillRect( hCanvas, &r2, (HBRUSH) GetStockObject( WHITE_BRUSH ) );

	SelectObject( hCanvas, GetStockObject( BLACK_PEN ) );

	MoveToEx( hCanvas, cx1, cy - 28, NULL );

	for (int ly = -28; ly < 28; ly++ )
	{
		/* Use pythagoras to get line extents */
		double dy = (double) ly;

		double dx = sqrt( ( 28.0 * 28.0 ) - (dy * dy) );

		LineTo( hCanvas, (int) ( cx1 - dx), cy + ly );
	}

	for (int ly = 28; ly > -28; ly-- )
	{
		/* Use pythagoras to get line extents */
		double dy = (double) ly;

		double dx = sqrt( ( 28.0 * 28.0 ) - (dy * dy) );

		LineTo( hCanvas, (int) ( cx2 + dx), cy + ly );
	}

	MoveToEx( hCanvas, cx1, cy - 28, NULL );
	LineTo( hCanvas, cx2, cy - 28 );
	MoveToEx( hCanvas, cx1, cy + 28, NULL );
	LineTo( hCanvas, cx2, cy + 28 );

	/* Draw the head casing */
	SelectObject( hCanvas, DkGPen );
	MoveToEx( hCanvas, cx1 - 15, r.bottom - 50, NULL );
	LineTo( hCanvas, cx1 - 5, r.bottom - 85 );
	SelectObject( hCanvas, LtGPen );
	LineTo( hCanvas, cx2 + 5, r.bottom - 85 );
	LineTo( hCanvas, cx2 + 15, r.bottom - 50 );

	/* Draw the lid outline */
	RECT r3 = r;
	r3.right = r3.left + 300;
	r3.bottom -= 50;
	r2 = r3;

	r2.top += 15;
	r2.left += 15;
	r2.bottom -= 15;
	r2.right -= 15;
	
	SelectObject( hCanvas, GetStockObject( LTGRAY_BRUSH ) );
	SelectObject( hCanvas, LtGPen );
	Rectangle( hCanvas, r3.left, r3.top, r3.right, r2.top );
	Rectangle( hCanvas, r3.left, r2.top, r2.left, r2.bottom );
	Rectangle( hCanvas, r2.right, r2.top, r3.right, r2.bottom );
	Rectangle( hCanvas, r3.left, r2.bottom, r3.right, r3.bottom );

	SelectObject( hCanvas, VLtGPen );
	MoveToEx( hCanvas, r2.left, r2.bottom, NULL );
	LineTo( hCanvas, r2.left, r2.top );
	LineTo( hCanvas, r2.right, r2.top );
	SelectObject( hCanvas, DkGPen );
	LineTo( hCanvas, r2.right, r2.bottom );
	LineTo( hCanvas, r2.left, r2.bottom );

	/* Finally the title and publisher */
	SetTextColor( hCanvas, RGB( 0, 0, 0 ) );

	r2.bottom = r2.top + 30;
	r2.left  += 4;
	r2.right -= 4;

	if ( TapeFont1 == NULL )
	{
		int FontSize = 28;

		bool Done = false;

		while ( !Done )
		{
			RECT r3 = r2;

			if ( TapeFont1 != NULL )
			{
				NixObject( TapeFont1 );
			}

			TapeFont1  = CreateFont(FontSize,FontSize/2,0,0,FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"Arial");

			SelectObject( hCanvas, TapeFont1 );

			DrawText( hCanvas, Indexes.Title.c_str(), -1, &r3, DT_CENTER | DT_VCENTER | DT_CALCRECT );

			if ( r3.right > r2.right )
			{
				FontSize -= 4;
			}
			else
			{
				Done = true;
			}

			if ( FontSize < 8 )
			{
				Done = true;
			}

			SelectObject( hCanvas, hEmpty );
		}
	}

	if ( TapeFont2 == NULL )
	{
		int FontSize = 28;

		bool Done = false;

		while ( !Done )
		{
			RECT r3 = r2;

			if ( TapeFont2 != NULL )
			{
				NixObject( TapeFont2 );
			}

			TapeFont2  = CreateFont(FontSize,FontSize/2,0,0,FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"Arial");

			SelectObject( hCanvas, TapeFont2 );

			DrawText( hCanvas, Indexes.Publisher.c_str(), -1, &r3, DT_CENTER | DT_VCENTER | DT_CALCRECT );

			if ( r3.right > r2.right )
			{
				FontSize -= 4;
			}
			else
			{
				Done = true;
			}

			if ( FontSize < 8 )
			{
				Done = true;
			}

			SelectObject( hCanvas, hEmpty );
		}
	}

	SelectObject( hCanvas, TapeFont1 );

	DrawText( hCanvas, Indexes.Title.c_str(), -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE );

	r2.bottom = r3.bottom - 15 - 20;
	r2.top    = r2.bottom - 30;

	SelectObject( hCanvas, TapeFont2 );
	
	DrawText( hCanvas, Indexes.Publisher.c_str(), -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE );

	/* Draw the extended controls */
	HDC hDC    = GetDC( hWnd );
	HDC hMemDC = CreateCompatibleDC( hDC );

	SelectObject( hMemDC, hVolume );

	StretchBlt( hCanvas, r.right - 16, r.top, 16, 16, hMemDC, 0, 0, 32, 32, SRCCOPY );

	DrawIndex();

	/* Blit the canvas to the real DC */

	BitBlt( hDC, r.left, r.top, r.right - r.left - 16, r.bottom - r.top, hCanvas, 0, 0, SRCCOPY );
	BitBlt( hDC, r.left + ( r.right - r.left - 16 ), r.bottom - 50, 16, 50, hCanvas, r.right - r.left - 16, r.bottom - 50, SRCCOPY );
	BitBlt( hDC, r.right - 16, r.top, 16, 16, hCanvas, r.right - 16, r.top, SRCCOPY );

	ValidateRect( hWnd, &r );

	DeleteDC( hMemDC );
	ReleaseDC( hWnd, hDC );
}

int AudioPlayer::GetBuffer( BYTE *pBuffer, DWORD lBuffer )
{
	EnterCriticalSection( &cs );

	if ( ( TapePtr == TapeExt ) || ( !Keys[2]->IsPressed() ) )
	{
		memset( pBuffer, 127, lBuffer );

		LeaveCriticalSection( &cs );

		return 0;
	}

	pStore->Seek( TapePtr );

	pStore->Read( pBuffer, lBuffer );

	TapePtr += lBuffer;

	if ( TapePtr > TapeExt )
	{
		TapePtr = TapeExt;
	}

	LeaveCriticalSection( &cs );

	return 0;
}

void AudioPlayer::SetReached( DWORD dwReached )
{
	BackCursor += 44100;
	PlayCursor  = BackCursor;
}

void AudioPlayer::DrawIndex( void )
{
	RECT r;

	GetClientRect( hWnd, &r );

	DWORD ix = r.left + 300;
	DWORD iy = r.top;
	DWORD iw = ( r.right - r.left ) - 300 - 16;
	DWORD ih = ( r.bottom - r.top ) - 50;

	SelectObject( hCanvas, GetStockObject( BLACK_PEN ) );
	SelectObject( hCanvas, GetStockObject( BLACK_BRUSH ) );

	Rectangle( hCanvas, ix, iy, ix + iw, r.top + ih );

	const DWORD MaxIndexes = 12;

	/* Let's find out what index we are currently on relative to PlayCursor */
	std::vector<TapeCue>::iterator iCue;

	int CIndex = 0;

	for ( iCue = Indexes.Cues.begin(); iCue != Indexes.Cues.end(); iCue++ )
	{
		if ( PlayCursor < iCue->IndexPtr )
		{
			break;
		}

		CIndex++;
	}

	CIndex--;

	int Index1, Index2;

	/* Now calculate the start and end points of our list */
	if ( Indexes.Cues.size() <= MaxIndexes )
	{
		/* Show the lot */
		Index1 = 0;
		Index2 = Indexes.Cues.size() - 1;
	}
	else
	{
		/* Set the indexes so that the current index is in the middle */
		Index1 = CIndex - (MaxIndexes / 2 );
		Index2 = CIndex + (MaxIndexes / 2 );

		/* Fixup rounding */
		if ( ( Index2 - Index1 ) > ( MaxIndexes - 1 ) ) { Index1++; }

		/* Now fix up positioning */
		while ( Index1 < 0 ) { Index1++; Index2++; }
		while ( Index2 >= Indexes.Cues.size() ) { Index2--; Index1--; }
	}

	SelectObject( hCanvas, IndexFont );
	SetBkMode( hCanvas, TRANSPARENT );
	SetTextColor( hCanvas, RGB( 255, 255, 255 ) );

	/* Now draw */
	for ( int i=Index1; i<=Index2; i++ )
	{
		RECT tr;

		tr.left   = ix + 2;
		tr.right  = ( ix + iw ) - 4;
		tr.top    = iy + ( 14 * ( i - Index1 ) );
		tr.bottom = tr.top + 14;

		tr.top++;
		tr.bottom--;

		if ( i < CIndex )
		{
			FillRect( hCanvas, &tr, GreenBrush );
		}
		else if ( i > CIndex )
		{
			FillRect( hCanvas, &tr, (HBRUSH) GetStockObject( BLACK_BRUSH ) );
		}
		else
		{
			std::vector<TapeCue>::iterator iNext = Indexes.Cues.begin() + i;

			iNext++;

			double TapeNext = TapeExt;

			if ( iNext != Indexes.Cues.end() )
			{
				TapeNext = iNext->IndexPtr;
			}

			double TT = TapeNext   - Indexes.Cues[ i ].IndexPtr;
			double TI = PlayCursor - Indexes.Cues[ i ].IndexPtr;
			double p;

			if ( TT == 0 )
			{
				p = 1.0;
			}
			else
			{
				p = TI / TT;
			}

			RECT tr1 = tr;
			RECT tr2 = tr;

			tr1.right = tr1.left + (int) ( (double) (iw - 6) * p );
			tr2.left  = tr1.right + 1;

			FillRect( hCanvas, &tr1, GreenBrush );

			if ( tr2.left < tr2.right )
			{
				FillRect( hCanvas, &tr2, (HBRUSH) GetStockObject( BLACK_BRUSH ) );
			}
		}

		tr.left += 2;
		tr.right -= 4;

		DrawText( hCanvas, Indexes.Cues[ i ].IndexName.c_str(), Indexes.Cues[ i ].IndexName.length(), &tr, DT_LEFT | DT_VCENTER );
	}

	SetTextColor( hCanvas, RGB( 0xc0, 0xc0, 0x00 ) );

	std::wstring Extra1,Extra2,Extra3;

	for (int i=0; i<=CIndex; i++ )
	{
		DWORD f = Indexes.Cues[i].Flags;

		if ( f & TF_ExtraValid )
		{
			if ( f & TF_Extra1 )
			{
				Extra1 = Indexes.Cues[i].Extra;
			}
			if ( f & TF_Extra2 )
			{
				Extra2 = Indexes.Cues[i].Extra;
			}
			if ( f & TF_Extra3 )
			{
				Extra3 = Indexes.Cues[i].Extra;
			}
		}
	}

	SelectObject( hCanvas, GetStockObject( BLACK_PEN ) );
	SelectObject( hCanvas, GetStockObject( BLACK_BRUSH ) );

	Rectangle( hCanvas, ix, r.bottom - 50, ix + iw, r.bottom );

	RECT tr;

	tr.left   = ix + 2 + 2;
	tr.right  = ( ix + iw ) - 4;
	tr.top    = r.bottom - 46;
	tr.bottom = tr.top + 14;

	DrawText( hCanvas, Extra1.c_str(), Extra1.length(), &tr, DT_LEFT | DT_VCENTER );

	tr.top += 16;
	tr.bottom += 16;

	DrawText( hCanvas, Extra2.c_str(), Extra2.length(), &tr, DT_LEFT | DT_VCENTER );

	tr.top += 16;
	tr.bottom += 16;

	DrawText( hCanvas, Extra3.c_str(), Extra3.length(), &tr, DT_LEFT | DT_VCENTER );

	RECT er = r;

	er.left  = r.left + 300;
	er.right = er.left + 2;

	DrawEdge( hCanvas, &er, EDGE_RAISED, BF_RECT );

	er = r;

	er.top    = r.bottom - 50;
	er.bottom = er.top + 2;

	DrawEdge( hCanvas, &er, EDGE_RAISED, BF_RECT );
}

void AudioPlayer::DoOptionsPopup( void )
{
	HMENU hOpts = CreatePopupMenu();

	AppendMenu( hOpts, MF_STRING, IDM_SAVEAUDIO, L"&Save Audio" );
	AppendMenu( hOpts, MF_SEPARATOR, 0, L"" );
	AppendMenu( hOpts, MF_STRING, IDM_CUEJUMP, L"Tape &Browser" );

	RECT r;

	GetWindowRect( hOptions, & r);

	TrackPopupMenu( hOpts, 0, r.left, r.top + 16, NULL, hWnd, NULL );

	DestroyMenu( hOpts );
}

void AudioPlayer::DoSaveAudio( void )
{
	std::wstring filename;

	if ( SaveFileDialog( hWnd, filename, L"Wave Audio File", L"WAV", L"Save Tape Audio as WAVE file" ) )
	{
		FILE *wavefile = nullptr;

		_wfopen_s( &wavefile, filename.c_str(), L"wb" );

		if ( wavefile == nullptr )
		{
			MessageBox( hWnd, std::wstring( L"Could not save tape audio to " + filename ).c_str(), L"NUTS Tape Image Player", MB_ICONERROR | MB_OK );

			return;
		}

		DWORD RIFF = 0x46464952;
		DWORD fmt  = 0x20746d66;
		DWORD WAVE = 0x45564157;
		DWORD data = 0x61746164;

		DWORD SubChunk1Size = 16;
		DWORD SubChunk2Size = TapeExt;
		DWORD ChunkSize     = 4 + ( 8 + SubChunk1Size ) + ( 8 + SubChunk2Size );

		fwrite( &RIFF, 1, 4, wavefile );          // RIFF header
		fwrite( &ChunkSize, 1, 4, wavefile );     // Chunk Size
		fwrite( &WAVE, 1, 4, wavefile );          // WAVE Chunk
		fwrite( &fmt,  1, 4, wavefile );          // fmt SubChunk
		fwrite( &SubChunk1Size, 1, 4, wavefile ); // fmt Chunk Size

		DWORD waveVar;

		waveVar = 0x00000001; fwrite( &waveVar, 1, 2, wavefile ); // AudioForamat
		waveVar = 0x00000001; fwrite( &waveVar, 1, 2, wavefile ); // NumChannels
		waveVar = 0x0000AC44; fwrite( &waveVar, 1, 4, wavefile ); // SampleRate
		waveVar = 0x0000AC44; fwrite( &waveVar, 1, 4, wavefile ); // ByteRate
		waveVar = 0x00000001; fwrite( &waveVar, 1, 2, wavefile ); // BlockAlign
		waveVar = 0x00000008; fwrite( &waveVar, 1, 2, wavefile ); // BitsPerSample

		fwrite( &data, 1, 4, wavefile ) ;         // data SubChunk
		fwrite( &SubChunk2Size, 1, 4, wavefile ); // data Chunk Size

		DWORD BytesToGo = TapeExt;
		BYTE  Buffer[ 16384 ];

		pStore->Seek( 0 );

		while ( BytesToGo > 0 )
		{
			DWORD BytesRead = min( 16384, BytesToGo );

			pStore->Read( Buffer, BytesRead );

			fwrite( Buffer, 1, BytesRead, wavefile );

			BytesToGo -= BytesRead;
		}

		pStore->Seek( TapePtr );

		fclose( wavefile );
	}
}

void AudioPlayer::Refresh()
{
	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );
}