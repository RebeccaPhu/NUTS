#include "StdAfx.h"
#include "AUDIOContentViewer.h"
#include "AUDIOTranslator.h"
#include "Plugins.h"
#include "FontBitmap.h"
#include "BuiltIns.h"
#include "Preference.h"
#include "FileDialogs.h"
#include "resource.h"
#include "NUTSMacros.h"

#include <process.h>

std::map<HWND, AUDIOContentViewer *> AUDIOContentViewer::viewers;

bool AUDIOContentViewer::WndClassReg = false;

static DWORD dwthreadid;

#define PlayerW 300
#define PlayerH 212

LRESULT CALLBACK AUDIOContentViewer::AUViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if ( viewers.find( hWnd ) != viewers.end() )
	{
		return viewers[ hWnd ]->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

AUDIOContentViewer::AUDIOContentViewer( CTempFile &fileObj, TXIdentifier TUID )
{
	hWnd           = NULL;
	ParentWnd      = NULL;
	hOld           = NULL;
	hCanvasDC      = NULL;
	hCanvas        = NULL;
	hDigitPen1     = NULL;
	hDigitPen2     = NULL;
	hFont          = NULL;

	hTranslateThread = NULL;
	hExitThread      = NULL;

	if ( !WndClassReg )
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= AUDIOContentViewer::AUViewerProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInst;
		wcex.hIcon			= LoadIcon(hInst, MAKEINTRESOURCE(IDI_AUDIO));
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
		wcex.lpszMenuName	= NULL;
		wcex.lpszClassName	= L"NUTS Audio Player";
		wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_AUDIO));

		RegisterClassEx(&wcex);

		WndClassReg = true;
	}

	pXlator = nullptr;
	TID     = TUID;

	fileObj.Keep();

	/* Clone this so that NUTS can dispose of the original */
	sourceObject = new CTempFile( fileObj.Name() );

	sourceObject->Seek( 0 );

	TimeIndex = 0;
	TimeTotal = 0;

	pSongPos        = nullptr;
	pVolume         = nullptr;
	pPlaylist       = nullptr;
	Translating     = true;
	TransSeg        = 0;
	AudioPtr        = 0;
	Playing         = false;
	Paused          = false;

	DoLoop = Preference( L"AudioPlayerLoop", false );

	InitializeCriticalSection( &cs );
}

AUDIOContentViewer::~AUDIOContentViewer(void)
{
	if ( hTranslateThread )
	{
		SetEvent( hExitThread );

		if ( WaitForSingleObject( hTranslateThread, 500 ) == WAIT_TIMEOUT )
		{
			TerminateThread( hTranslateThread, 500 );
		}
	}

	CloseHandle( hTranslateThread );
	CloseHandle( hExitThread );

	SoundPlayer.Stop();

	if ( pPlaylist != nullptr )
	{
		delete pPlaylist;
	}

	if ( pSongPos != nullptr )
	{
		delete pSongPos;
	}

	if ( pVolume != nullptr )
	{
		delete pVolume;
	}

	std::vector<AudioPlayerElement *>::iterator iElement;

	for ( iElement = Buttons.begin(); iElement != Buttons.end(); iElement++ )
	{
		delete *iElement;
	}

	Buttons.clear();
	Buttons.shrink_to_fit();

	if ( hOld != nullptr )
	{
		SelectObject( hCanvasDC, hOld );
	}

	NixObject( hCanvasDC );
	NixObject( hCanvas );
	NixObject( hDigitPen1 );
	NixObject( hDigitPen2 );
	NixObject( hFont );

	NixWindow( hWnd );

	if ( pXlator != nullptr )
	{
		delete pXlator;
	}

	delete sourceObject;

	DeleteCriticalSection( &cs );
}

int AUDIOContentViewer::Create(HWND Parent, HINSTANCE hInstance, int x, int y ) {
	hWnd = CreateWindowEx(
		NULL,
		L"NUTS Audio Player",
		L"NUTS Audio Player",
		WS_SYSMENU | WS_CLIPCHILDREN | WS_OVERLAPPED | WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED,
		x, y, PlayerW, PlayerH,
		GetDesktopWindow(), NULL, hInstance, NULL
	);

	AUDIOTranslator *pXlator = (AUDIOTranslator *) FSPlugins.LoadTranslator( TID );

	viewers[ hWnd ] = this;

	ParentWnd	= Parent;

	HDC hDC = GetDC( hWnd );

	hCanvas   = CreateCompatibleBitmap( hDC, PlayerW, PlayerH );
	hCanvasDC = CreateCompatibleDC( hDC );
	hOld      = SelectObject( hCanvasDC, hCanvas );

	ReleaseDC( hWnd, hDC );
	
	RECT r;

	GetClientRect( hWnd, &r );

	r.right = r.left + PlayerW;
	r.bottom = r.top + 96;

	AdjustWindowRect( &r, WS_SYSMENU | WS_CLIPCHILDREN | WS_OVERLAPPED | WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED, FALSE );

	SetWindowPos( hWnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOMOVE );

	hDigitPen1 = CreatePen( PS_SOLID, 1, RGB( 0x00, 0xCC, 0xCC ) );
	hDigitPen2 = CreatePen( PS_SOLID, 1, RGB( 0x00, 0x33, 0x33 ) );

	pSongPos        = new AudioPlayerElement( hWnd, 0, 84, PlayerW, 8, AudioElementScrollBar, AEWS_HORIZONTAL );
	pVolume         = new AudioPlayerElement( hWnd, 138, 28, 78, 8, AudioElementScrollBar, AEWS_HORIZONTAL );
	pPlaylist       = new AudioPlayerElement( hWnd, 2, 96, PlayerW - 4, PlayerH - 98, AudioElementListBox, NULL );

	pSongPos->SetActive( false );
	pVolume->SetLimit( 127 );
	pVolume->SetActive( true );
	pVolume->SetActiveUpdate( true );

	DWORD v = Preference( L"AudioPlayerVolume", (DWORD) 64 );

	pVolume->SetPosition( v );

	Volume = (BYTE) v;

	hFont	= CreateFont(16,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));

	int xd    = 20;
	int basex = PlayerW - ( xd * 8 ) - 2;
	int basey = 42;

	Buttons.push_back( new AudioPlayerElement( hWnd, basex + ( 0 * xd ), basey, 16, 16, AudioElementButton, AEWS_PREV  ) );
	Buttons.push_back( new AudioPlayerElement( hWnd, basex + ( 1 * xd ), basey, 16, 16, AudioElementButton, AEWS_PLAY  ) );
	Buttons.push_back( new AudioPlayerElement( hWnd, basex + ( 2 * xd ), basey, 16, 16, AudioElementButton, AEWS_PAUSE ) );
	Buttons.push_back( new AudioPlayerElement( hWnd, basex + ( 3 * xd ), basey, 16, 16, AudioElementButton, AEWS_STOP  ) );
	Buttons.push_back( new AudioPlayerElement( hWnd, basex + ( 4 * xd ), basey, 16, 16, AudioElementButton, AEWS_NEXT  ) );
	Buttons.push_back( new AudioPlayerElement( hWnd, basex + ( 5 * xd ), basey, 16, 16, AudioElementButton, AEWS_EJECT ) );
	Buttons.push_back( new AudioPlayerElement( hWnd, basex + ( 6 * xd ), basey, 16, 16, AudioElementButton, AEWS_LOOP  ) );
	Buttons.push_back( new AudioPlayerElement( hWnd, basex + ( 7 * xd ), basey, 16, 16, AudioElementButton, AEWS_SAVE  ) );

	Buttons[ 0 ]->AddTooltip( L"Jump to previous cue point" );
	Buttons[ 1 ]->AddTooltip( L"Play audio" );
	Buttons[ 2 ]->AddTooltip( L"Pause audio" );
	Buttons[ 3 ]->AddTooltip( L"Stop audio" );
	Buttons[ 4 ]->AddTooltip( L"Jump to next cue point" );
	Buttons[ 5 ]->AddTooltip( L"Close audio player" );
	Buttons[ 6 ]->AddTooltip( L"Enable/disable looping (audio restarts once finished)" );
	Buttons[ 7 ]->AddTooltip( L"Save audio as 16-bit 44kHz Stereo WAV file (uncompressed) ");

	for ( BYTE i=0; i<8; i++ ) { Buttons[i]->SetDisabled( true ); }

	Buttons[6]->SetActive( DoLoop );

	hExitThread = CreateEvent( NULL, TRUE, FALSE, NULL );

	DWORD dwthreadid;

	hTranslateThread = (HANDLE) _beginthreadex(NULL, NULL, _TranslateThread, this, NULL, (unsigned int *) &dwthreadid);

	SetTimer( hWnd, 0x7777, 100, NULL );

	SetFocus( Buttons[ 1 ]->hWnd );

	SetActiveWindow( hWnd );
	SetWindowPos( hWnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE );

	return 0;
}

unsigned int AUDIOContentViewer::TranslateThread()
{
	void *pX = NUTSBuiltIns.LoadTranslator( TID );

	if ( pX == nullptr )
	{
		pX = FSPlugins.LoadTranslator( TID );
	}

	if ( pX != nullptr )
	{
		pXlator = (AUDIOTranslator *) pX;

		TXOptions.hStopTranslating = hExitThread;
		TXOptions.hParentWnd       = hWnd;

		if ( pXlator->Translate( *sourceObject, AudioSource, &TXOptions ) != 0 )
		{
			NUTSError::Report( L"Translate Audio", hWnd );
		}
	}

	return 0;
}

LRESULT	AUDIOContentViewer::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	switch (message )
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

	case WM_DESTROY:
		viewers.erase( hWnd );

		delete this;

		break;

	case WM_PAINT:
		{
			PaintPlayer();
		}
		break;

	case WM_TIMER:
		{
			TransSeg = ++TransSeg%6;

			Invalidate();

			if ( WaitForSingleObject( hTranslateThread, 0 ) == WAIT_OBJECT_0 )
			{
				CloseHandle( hTranslateThread );
				CloseHandle( hExitThread );

				hTranslateThread = NULL;
				hExitThread      = NULL;

				if ( TXOptions.CuePoints.size() > 0 )
				{
					RECT r;

					GetClientRect( hWnd, &r );

					r.right = r.left + PlayerW;
					r.bottom = r.top + PlayerH;

					AdjustWindowRect( &r, WS_SYSMENU | WS_CLIPCHILDREN | WS_OVERLAPPED | WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED, FALSE );

					SetWindowPos( hWnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOMOVE );
				}

				TimeIndex = 0;
				TimeTotal = TXOptions.AudioSize / 44100;

				if ( TXOptions.WideBits )
				{
					TimeTotal /= 2;
				}

				if ( TXOptions.Stereo )
				{
					TimeTotal /= 2;
				}

				Translating = false;

				pSongPos->SetActive( true );
				pSongPos->SetLimit( TimeTotal );

				KillTimer( hWnd, 0x7777 );

				AudioSource.Seek( 0 );

				SoundPlayer.SetFormat( (TXOptions.Stereo)?2:1, TXOptions.WideBits );
				SoundPlayer.Init( hWnd, this );
				SoundPlayer.SetVolume( Volume );
				SoundPlayer.Play();
				SoundPlayer.Pause();

				for ( BYTE i=0; i<8; i++ ) { Buttons[i]->SetDisabled( false ); }

				if ( TXOptions.CuePoints.size() == 0 )
				{
					Buttons[0]->SetDisabled( true );
					Buttons[4]->SetDisabled( true );
				}
				else
				{
					for ( int i=0; i<TXOptions.CuePoints.size(); i++ )
					{
						pPlaylist->AddCuePoint( TXOptions.CuePoints[ i ] );
					}
				}

				pPlaylist->SetCuePoint( 0 );

				Invalidate();
			}
		}

	case WM_AUDIOELEMENT:
		{
			void *o = (void *) wParam;

			if ( o == pSongPos )
			{
				if ( ( Playing ) && ( !Paused ) )
				{
					SoundPlayer.Pause();
					SoundPlayer.PositionReset();
				}

				EnterCriticalSection( &cs );

				TimeIndex = (DWORD) lParam;

				AudioPtr = (QWORD) lParam;
				AudioPtr *= 44100;

				if ( TXOptions.WideBits ) { AudioPtr *= 2; }
				if ( TXOptions.Stereo )   { AudioPtr *= 2; }

				AudioSource.Seek( AudioPtr );

				pPlaylist->SetCuePoint( AudioPtr );

				LeaveCriticalSection( &cs );

				if ( ( Playing ) && ( !Paused ) )
				{
					SoundPlayer.Play();
				}

				if ( ( !Playing ) || ( Paused ) )
				{
					pSongPos->SetPosition( TimeIndex );
				}

				Invalidate();
			}
			else if ( o == pPlaylist )
			{
				int se = (int) lParam;

				if ( ( Playing ) && ( !Paused ) )
				{
					SoundPlayer.Pause();
					SoundPlayer.PositionReset();
				}

				EnterCriticalSection( &cs );

				AudioPtr = TXOptions.CuePoints[ se ].Position;

				QWORD TimePtr = AudioPtr;

				TimePtr /= 44100;

				if ( TXOptions.WideBits ) { TimePtr /= 2; }
				if ( TXOptions.Stereo )   { TimePtr /= 2; }

				TimeIndex = (DWORD) TimePtr;

				AudioSource.Seek( AudioPtr );

				pPlaylist->SetCuePoint( AudioPtr );

				LeaveCriticalSection( &cs );

				if ( ( Playing ) && ( !Paused ) )
				{
					SoundPlayer.Play();
				}

				if ( ( !Playing ) || ( Paused ) )
				{
					pSongPos->SetPosition( TimeIndex );
				}

				Invalidate();
			}
			else if ( o == pVolume )
			{
				Preference( L"AudioPlayerVolume" ) = (DWORD) lParam;

				Volume = (BYTE) Volume;

				SoundPlayer.SetVolume( (BYTE) lParam );

				pVolume->SetPosition( (DWORD) lParam );
			}
			else
			{
				ProcessButton( (DWORD) lParam );
			}
		}
		break;
	}

	return DefWindowProc( hWnd, message, wParam, lParam);
}

void AUDIOContentViewer::PaintPlayer( void )
{
	HDC hDC = GetDC( hWnd );

	RECT r;

	GetClientRect( hWnd, &r );

	FillRect( hCanvasDC, &r, (HBRUSH) GetStockObject( BLACK_BRUSH ) );

	DrawTime( TimeIndex, 8,   8,  true );
	DrawTime( TimeTotal, 220, 8,  false );


	DrawBiText( &TXOptions.Title, 8, 64, PlayerW, DT_TOP | DT_LEFT );

	/* Bit hackish, but keeps everything in check */
	AudioCuePoint cue;

	cue.Name        = TXOptions.FormatName;
	cue.UseEncoding = false;

	DrawBiText( &cue, 140, 8, 80, DT_VCENTER | DT_CENTER );

	SelectObject( hCanvasDC, hDigitPen1 );

	MoveToEx( hCanvasDC, 0, 62, NULL );
	LineTo( hCanvasDC, PlayerW, 62 );
	MoveToEx( hCanvasDC, 0, 82, NULL );
	LineTo( hCanvasDC, PlayerW, 82 );


	BitBlt( hDC, 0, 0, PlayerW, PlayerH, hCanvasDC, 0, 0, SRCCOPY );

	ReleaseDC( hWnd, hDC );
}

void AUDIOContentViewer::DrawTime( DWORD TIndex, int x, int y, bool large )
{
	int HGap = 28;
	int VSize = 24;

	if ( !large )
	{
		HGap  = 18;
		VSize = 12;
	}

	DWORD Digits1 = TIndex / 60;
	DWORD Digits2 = TIndex % 60;

	DWORD Digit1 = Digits1 / 10;
	DWORD Digit2 = Digits1 % 10;
	DWORD Digit3 = Digits2 / 10;
	DWORD Digit4 = Digits2 % 10;

	DrawDigit( (BYTE) Digit1, 0 * HGap + x, y, large );
	DrawDigit( (BYTE) Digit2, 1 * HGap + x, y, large );
	DrawDigit( (BYTE) Digit3, 2 * HGap + x + 6, y, large );
	DrawDigit( (BYTE) Digit4, 3 * HGap + x + 6, y, large );

	Ellipse( hCanvasDC, x + HGap * 2 - 1, y + VSize - (VSize / 2 ), x + HGap * 2 + 3, y + VSize - (VSize / 2 ) + 4 );
	Ellipse( hCanvasDC, x + HGap * 2 - 1, y + VSize + (VSize / 2 ), x + HGap * 2 + 3, y + VSize + (VSize / 2 ) + 4 );
}

void AUDIOContentViewer::DrawDigit( BYTE Digit, int x, int y, bool large )
{
	int HSize = 24;
	int VSize = 24;
	
	if ( !large )
	{
		HSize = 14;
		VSize = 12;
	}

	/* Digit table bits: 6 top, 5 top left, 4 top right, 3 middle, 2 bottom left, 1 bottom right, 0 bottom */
	const BYTE Segs[10] = {
		/* 0b01110111, */ 0x77,
		/* 0b00010010, */ 0x12,
		/* 0b01011101, */ 0x5D,
		/* 0b01011011, */ 0x5B,
		/* 0b00111010, */ 0x3A,
		/* 0b01101011, */ 0x6B,
		/* 0b01101111, */ 0x6F,
		/* 0b01010010, */ 0x52,
		/* 0b01111111, */ 0x7F,
		/* 0b01111011  */ 0x7B
	};

	BYTE Seg = Segs[ Digit ];

	if ( Translating )
	{
		const BYTE XSeg[6] = { 0x40, 0x10, 0x02, 0x01, 0x04, 0x20 };

		Seg = XSeg[ TransSeg ];
	}

	SelectObject( hCanvasDC, hDigitPen1 );

	if ( Seg & 0x20 ) { DrawSeg( x, y + 1, VSize - 1, false, false, true ); }
	if ( Seg & 0x04 ) { DrawSeg( x, y + 1 + VSize, VSize - 1, false, false, true ); }

	if ( Seg & 0x10 ) { DrawSeg( x + HSize - 1, y + 1, VSize - 1, false, false, false ); }
	if ( Seg & 0x02 ) { DrawSeg( x + HSize - 1, y + 1 + VSize, VSize - 1, false, false, false ); }

	if ( Seg & 0x40 ) { DrawSeg( x + 2, y, HSize - 4, true, false, true ); }
	if ( Seg & 0x08 ) { DrawSeg( x + 2, y + VSize, HSize - 4, true, true, false ); }
	if ( Seg & 0x01 ) { DrawSeg( x + 2, y + VSize + VSize, HSize - 4, true, false, false ); }

	Seg ^= 0xFF;

	SelectObject( hCanvasDC, hDigitPen2 );

	if ( Seg & 0x20 ) { DrawSeg( x, y + 1, VSize - 1, false, false, true ); }
	if ( Seg & 0x04 ) { DrawSeg( x, y + 1 + VSize, VSize - 1, false, false, true ); }

	if ( Seg & 0x10 ) { DrawSeg( x + HSize - 1, y + 1, VSize - 1, false, false, false ); }
	if ( Seg & 0x02 ) { DrawSeg( x + HSize - 1, y + 1 + VSize, VSize - 1, false, false, false ); }

	if ( Seg & 0x40 ) { DrawSeg( x + 2, y, HSize - 4, true, false, true ); }
	if ( Seg & 0x08 ) { DrawSeg( x + 2, y + VSize, HSize - 4, true, true, false ); }
	if ( Seg & 0x01 ) { DrawSeg( x + 2, y + VSize + VSize, HSize - 4, true, false, false ); }
}

void AUDIOContentViewer::DrawSeg( int x, int y, int l, bool horiz, bool cent, bool lefttop )
{
	for ( int o = 0; o<3; o++ )
	{
		int o2 = o;

		if ( !lefttop ) { o2 = 0 - o; }

		if ( horiz )
		{
			int y2 = y + o2;
			int l2 = l - ( o * 2 );
			int x2 = x + o;

			if ( ( cent ) && ( o == 2 ) )
			{
				y2 = y + 1;
				l2 = l - 2;
				x2 = x + 1;
			}

			MoveToEx( hCanvasDC, x2, y2, NULL );
			LineTo( hCanvasDC, x2 + l2, y2 );
		}
		else
		{
			MoveToEx( hCanvasDC, x + o2, y + o, NULL );
			LineTo( hCanvasDC, x + o2, y + ( l - o ) );
		}
	}
}

void AUDIOContentViewer::DrawBiText( AudioCuePoint *cue, int x, int y, int w, DWORD Displace )
{
	if ( cue->UseEncoding )
	{
		std::vector<FontIdentifier> fonts = FSPlugins.FontListForEncoding( cue->EncodingID );

		if ( fonts.size() > 0 )
		{
			FontBitmap t( fonts[ 0 ], cue->EncodingName, w / 8, true, false );

			t.SetButtonColor( 0, 0, 0 );
			t.SetTextColor( 0x00, 0xCC, 0xCC );

			t.DrawText( hCanvasDC, x, y, Displace );
		}
	}
	else
	{
		SelectObject( hCanvasDC, hFont );
		SetTextColor( hCanvasDC, RGB( 0x00, 0xCC, 0xCC ) );
		SetBkColor( hCanvasDC, 0 );

		RECT r;

		r.top    = y;
		r.left   = x;
		r.right  = x + w;
		r.bottom = y + 16;

		DrawText( hCanvasDC, cue->Name.c_str(), (int) cue->Name.length(), &r, Displace );
	}
}

unsigned int __stdcall AUDIOContentViewer::_TranslateThread(void *param)
{
	AUDIOContentViewer *pClass = (AUDIOContentViewer *) param;

	return pClass->TranslateThread();
}

int AUDIOContentViewer::GetBuffer( BYTE *pBuffer, DWORD lBuffer )
{
	memset( pBuffer, 0, lBuffer );

	EnterCriticalSection( &cs );

	if ( ( Playing ) && ( !Paused ) )
	{
		AudioSource.Read( pBuffer, lBuffer );
	}

	LeaveCriticalSection( &cs );

	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );

	return 0;
}

void AUDIOContentViewer::SetReached( DWORD dwReached )
{
	DWORD mo = 0;

	EnterCriticalSection( &cs );

	if ( ( Playing ) && ( !Paused ) )
	{
		AudioPtr += 44100;
	
		DWORD ti = (DWORD ) AudioPtr;

		ti /= 44100;

		if ( TXOptions.Stereo ) { ti /= 2; }
		if ( TXOptions.WideBits ) { ti /= 2; }

		TimeIndex = ti;

		mo = ti;

		if ( AudioPtr >= TXOptions.AudioSize )
		{
			SoundPlayer.Pause();

			AudioPtr  = 0;
			TimeIndex = 0;

			AudioSource.Seek( 0 );

			SoundPlayer.PositionReset();

			if ( DoLoop )
			{
				SoundPlayer.Play();
			}
			else
			{
				Playing = false;
				Paused  = false;

				Buttons[1]->SetActive( false );
				Buttons[2]->SetActive( false );
			}
		}
		else
		{
			pPlaylist->SetCuePoint( AudioPtr );
		}
	}

	LeaveCriticalSection( &cs );

	pSongPos->SetPosition( mo );
}

void AUDIOContentViewer::ProcessButton( DWORD Button )
{
	if ( Buttons.size() != 8 )
	{
		return;
	}

	if ( Button & AEWS_PLAY )
	{
		Playing = true;
		Paused  = false;
		
		Buttons[1]->SetActive( true );
		Buttons[2]->SetActive( false );

		SoundPlayer.PositionReset();
		SoundPlayer.Play();
	}

	if ( Button & AEWS_STOP )
	{
		Playing = false;
		Paused  = false;

		Buttons[1]->SetActive( false );
		Buttons[2]->SetActive( false );

		SoundPlayer.Pause();
	}

	if ( Button & AEWS_PREV )
	{
	}

	if ( Button & AEWS_NEXT )
	{
	}

	if ( Button & AEWS_PAUSE )
	{
		if ( Playing )
		{
			Paused = !Paused;

			Buttons[2]->SetActive( Paused );

			if ( Paused )
			{
				SoundPlayer.Pause();
			}
			else
			{
				SoundPlayer.Play();
			}
		}
	}

	if ( Button & AEWS_EJECT )
	{
		viewers.erase( hWnd );

		delete this;

		return;
	}

	if ( Button & AEWS_LOOP )
	{
		DoLoop = !DoLoop;

		Preference( L"AudioPlayerLoop" ) = DoLoop;

		Buttons[6]->SetActive( DoLoop );
	}

	if ( ( Button & AEWS_SAVE ) && ( !Playing ) )
	{
		SaveData();
	}

	Buttons[7]->SetDisabled( Playing );
}

void AUDIOContentViewer::Invalidate()
{
	RECT r;

	GetClientRect( hWnd, &r );

	InvalidateRect( hWnd, &r, FALSE );
}

void AUDIOContentViewer::SaveData()
{
	std::wstring filename;

	if ( SaveFileDialog( hWnd, filename, L"Wave Audio File", L"WAV", L"Save Translated Audio as WAVE file" ) )
	{
		FILE *wavefile = nullptr;

		_wfopen_s( &wavefile, filename.c_str(), L"wb" );

		if ( wavefile == nullptr )
		{
			MessageBox( hWnd, std::wstring( L"Could not save translated audio to " + filename ).c_str(), L"NUTS Audio Translator", MB_ICONERROR | MB_OK );

			return;
		}

		DWORD RIFF = 0x46464952;
		DWORD fmt  = 0x20746d66;
		DWORD WAVE = 0x45564157;
		DWORD data = 0x61746164;

		DWORD SubChunk1Size = 16;
		DWORD SubChunk2Size = AudioSource.Ext();
		DWORD ChunkSize     = 4 + ( 8 + SubChunk1Size ) + ( 8 + SubChunk2Size );

		fwrite( &RIFF, 1, 4, wavefile );          // RIFF header
		fwrite( &ChunkSize, 1, 4, wavefile );     // Chunk Size
		fwrite( &WAVE, 1, 4, wavefile );          // WAVE Chunk
		fwrite( &fmt,  1, 4, wavefile );          // fmt SubChunk
		fwrite( &SubChunk1Size, 1, 4, wavefile ); // fmt Chunk Size

		DWORD waveVar;
		DWORD ByteRate = 44100;
		DWORD Chans    = 1;
		DWORD Bits     = 8;

		if ( TXOptions.WideBits ) { ByteRate *= 2; Bits  = 16; }
		if ( TXOptions.Stereo   ) { ByteRate *= 2; Chans = 2;  }

		waveVar = 0x00000001; fwrite( &waveVar, 1, 2, wavefile ); // AudioForamat
		waveVar = Chans;      fwrite( &waveVar, 1, 2, wavefile ); // NumChannels
		waveVar = 0x0000AC44; fwrite( &waveVar, 1, 4, wavefile ); // SampleRate
		waveVar = ByteRate;   fwrite( &waveVar, 1, 4, wavefile ); // ByteRate
		waveVar = 0x00000001; fwrite( &waveVar, 1, 2, wavefile ); // BlockAlign
		waveVar = Bits;       fwrite( &waveVar, 1, 2, wavefile ); // BitsPerSample

		fwrite( &data, 1, 4, wavefile ) ;         // data SubChunk
		fwrite( &SubChunk2Size, 1, 4, wavefile ); // data Chunk Size

		DWORD BytesToGo = AudioSource.Ext();
		BYTE  Buffer[ 16384 ];

		AudioSource.Seek( 0 );

		while ( BytesToGo > 0 )
		{
			DWORD BytesRead = min( 16384, BytesToGo );

			AudioSource.Read( Buffer, BytesRead );

			fwrite( Buffer, 1, BytesRead, wavefile );

			BytesToGo -= BytesRead;
		}

		AudioSource.Seek( AudioPtr );

		fclose( wavefile );
	}
}