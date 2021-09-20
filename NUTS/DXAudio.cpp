#include "StdAfx.h"
#include "DXAudio.h"

#define INITGUID
#include <dsound.h>
#include <process.h>

/* Mostly taken from here:

   http://idlebeaver.ninja/dx7/devdoc/live/directx/dstut_8ktg.htm
*/

#define DX_SIZE 88200
#define DX_ONEBUF ( DX_SIZE / 2 )
#define DX_OFFSET DX_ONEBUF

DXAudio::DXAudio()
{
	lpds         = nullptr;
	lpdsb        = nullptr;
	lpdsbPrimary = nullptr;
	lpdsNotify   = nullptr;

	hPlayerThread = NULL;
	hStopPlayer   = NULL;

	ResetBuffers  = true;

	IgnoreFirstReach = true;

	PlayVolume = 127;
	NumChans   = 1;
	WideBits   = false;
}

DXAudio::~DXAudio(void)
{
	Stop();

	if ( lpdsNotify != nullptr )
	{
		lpdsNotify->Release();
	}

	if ( lpdsb != nullptr )
	{
		lpdsb->Release();
	}

	if ( lpdsbPrimary != nullptr )
	{
		lpdsbPrimary->Release();
	}

	if ( lpds != nullptr )
	{
		lpds->Release();
	}
}

void DXAudio::Init( HWND hWnd, DXAudioCallback *pSource )
{
	SoundWnd  = hWnd;
	pCallback = pSource;

    if ( DirectSoundCreate( NULL, &lpds, NULL) != S_OK )
	{
        return;
	}
 
    if ( IDirectSound_SetCooperativeLevel( lpds, hWnd, DSSCL_PRIORITY ) != S_OK )
	{
        return;
	}
    
	DSBUFFERDESC bsddesc;
 
    ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));

    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;

    if ( lpds->CreateSoundBuffer(&dsbdesc, &lpdsbPrimary, NULL) != S_OK )
	{
        return;
	}
 
    WAVEFORMATEX wfx;
    memset( &wfx, 0, sizeof( WAVEFORMATEX ) ); 

    wfx.wFormatTag      = WAVE_FORMAT_PCM; 
    wfx.nChannels       = NumChans; 
    wfx.nSamplesPerSec  = 44100; 
    wfx.wBitsPerSample  = ((WideBits)?2:1)*8; 
    wfx.nBlockAlign     = wfx.wBitsPerSample / 8 * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
 
    HRESULT hr = lpdsbPrimary->SetFormat(&wfx); 

    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC)); 

    dsbdesc.dwSize = sizeof(DSBUFFERDESC); 
    dsbdesc.dwFlags = 
            DSBCAPS_GETCURRENTPOSITION2   // Always a good idea
            | DSBCAPS_GLOBALFOCUS         // Allows background playing
            | DSBCAPS_CTRLPOSITIONNOTIFY  // Needed for notification
			| DSBCAPS_CTRLVOLUME;         // For the volume slider.
 
    // The size of the buffer is arbitrary, but should be at least
    // two seconds, to keep data writes well ahead of the play
    // position.

    dsbdesc.dwBufferBytes = DX_SIZE;  
    dsbdesc.lpwfxFormat   = &wfx; 
 
    if ( IDirectSound_CreateSoundBuffer( lpds, &dsbdesc, &lpdsb, NULL ) != S_OK )
    {
        return; 
    }

	for (int i = 0; i < 2; i++)
	{
		rghEvent[i] = CreateEvent( NULL, FALSE, FALSE, NULL );

		if ( NULL == rghEvent[i] )
		{
			return;
		}
    }
 
	rgdsbpn[0].dwOffset     = 0;
	rgdsbpn[0].hEventNotify = rghEvent[0];
	rgdsbpn[1].dwOffset     = DX_ONEBUF;
	rgdsbpn[1].hEventNotify = rghEvent[1];
 
    if ( IDirectSoundBuffer_QueryInterface( lpdsb, IID_IDirectSoundNotify8, (VOID **) &lpdsNotify ) != S_OK )
	{
        return;
	}
 
    if ( IDirectSoundNotify_SetNotificationPositions( lpdsNotify, 2, rgdsbpn ) != S_OK )
    {
        IDirectSoundNotify_Release( lpdsNotify );

        return;
    }

	DWORD dwthreadid;

	hStopPlayer   = CreateEvent( NULL, TRUE, FALSE, NULL );
	hPlayerThread = (HANDLE) _beginthreadex(NULL, NULL, _PlayerThread, this, NULL, (unsigned int *) &dwthreadid);
}

unsigned int DXAudio::_PlayerThread(void *param)
{
	DXAudio *pClass = (DXAudio *) param;

	return pClass->PlayerThread();
}

unsigned int DXAudio::PlayerThread()
{
	HANDLE evts[3];

	while ( 1 )
	{
		evts[ 0 ] = rghEvent[ 0 ];
		evts[ 1 ] = rghEvent[ 1 ];
		evts[ 2 ] = hStopPlayer;

		DWORD evt = WaitForMultipleObjects( 3, evts, FALSE, 1000 );

		if ( evt != WAIT_TIMEOUT )
		{
			if ( WaitForSingleObject( hStopPlayer, 0 ) == WAIT_OBJECT_0 )
			{
				return 0;
			}

			DWORD dwStartOfs = 0;
			VOID  *lpvPtr1, *lpvPtr2;
			DWORD dwBytes1, dwBytes2;

			if ( !IgnoreFirstReach )
			{
				if ( evt == WAIT_OBJECT_0 )
				{
					dwStartOfs = DX_OFFSET;

					pCallback->SetReached( 0 );
				}

				if ( evt == WAIT_OBJECT_0 + 1 )
				{
					dwStartOfs = 0;

					pCallback->SetReached( DX_OFFSET );
				}

				IDirectSoundBuffer_Lock(lpdsb,
						 dwStartOfs,       // Offset of lock start
						 DX_ONEBUF,        // Number of bytes to lock
						 &lpvPtr1,         // Address of lock start
						 &dwBytes1,        // Count of bytes locked
						 &lpvPtr2,         // Address of wrap around
						 &dwBytes2,        // Count of wrap around bytes
						 0);               // Flags
			
				pCallback->GetBuffer( (BYTE *) lpvPtr1, dwBytes1 );

				if ( dwBytes2 != 0 )
				{
					pCallback->GetBuffer( (BYTE *) lpvPtr2, dwBytes2 );
				}

				IDirectSoundBuffer_Unlock( lpdsb, lpvPtr1, dwBytes1, lpvPtr2, dwBytes2 );
			}

			IgnoreFirstReach = false;
		}
	}

	return 0;
}

void DXAudio::Stop( void )
{
	if ( hPlayerThread == NULL )
	{
		return;
	}

	SetEvent( hStopPlayer );

	if ( WaitForSingleObject( hPlayerThread, 500 ) == WAIT_TIMEOUT )
	{
		TerminateThread( hPlayerThread, 500 );
	}

	hPlayerThread = NULL;
}

void DXAudio::Pause( void )
{
	if ( lpdsb != nullptr )
	{
		IDirectSoundBuffer_Stop(lpdsb);
	}
}

void DXAudio::Play( void )
{
	if ( lpdsb != nullptr )
	{
		SetVolume( PlayVolume );

		if ( ResetBuffers )
		{
			IDirectSoundBuffer_SetCurrentPosition( lpdsb, 0 );

			VOID  *lpvPtr1, *lpvPtr2;
			DWORD dwBytes1, dwBytes2;

			IDirectSoundBuffer_Lock(lpdsb,
                     0,                // Offset of lock start
                     DX_SIZE,          // Number of bytes to lock
                     &lpvPtr1,         // Address of lock start
                     &dwBytes1,        // Count of bytes locked
                     &lpvPtr2,         // Address of wrap around
                     &dwBytes2,        // Count of wrap around bytes
                     0);               // Flags
			
			pCallback->GetBuffer( (BYTE *) lpvPtr1, dwBytes1 );

			if ( dwBytes2 != 0 )
			{
				pCallback->GetBuffer( (BYTE *) lpvPtr2, dwBytes2 );
			}

			IDirectSoundBuffer_Unlock( lpdsb, lpvPtr1, dwBytes1, lpvPtr2, dwBytes2 );
		}

		IgnoreFirstReach = true;

		ResetBuffers = false;

		IDirectSoundBuffer_Play(lpdsb, 0, 0, DSBPLAY_LOOPING);
	}
}

void DXAudio::PositionReset( void )
{
	ResetBuffers = true;
}

void DXAudio::SetVolume( BYTE Volume )
{
	PlayVolume = Volume;

	const DWORD VolRange = ( DSBVOLUME_MAX - DSBVOLUME_MIN )  / 2;

	double v = (double) VolRange / 127.0;

	v *= (double) PlayVolume;
	v += DSBVOLUME_MIN + ( ( 0 - DSBVOLUME_MIN) / 2 );
	
	if ( lpdsb != NULL )
	{
		HRESULT hr = lpdsb->SetVolume( (LONG) v );
	}
}

void DXAudio::SetFormat( BYTE Chans, bool Wide )
{
	NumChans = Chans;
	WideBits = Wide;
}
