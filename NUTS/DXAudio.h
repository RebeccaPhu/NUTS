#pragma once

#define INITGUID

#include <MMSystem.h>
#include <dsound.h>

class DXAudioCallback
{
public:
	virtual int GetBuffer( BYTE *pBuffer, DWORD lBuffer ) = 0;
	virtual void SetReached( DWORD dwReached ) = 0;
};

class DXAudio
{
public:
	DXAudio(void);
	~DXAudio(void);

public:
	static unsigned int __stdcall _PlayerThread(void *param);
	unsigned int PlayerThread();

public:
	void Init( HWND hWnd, DXAudioCallback *pSource );
	void Stop( void );
	void Pause( void );
	void Play( void );
	void PositionReset( void );
	void SetVolume( BYTE Volume );
	void SetFormat( BYTE Chans, bool Wide );

private:
	HWND SoundWnd;

	LPDIRECTSOUND               lpds;
	DSBUFFERDESC                dsbdesc;
	LPDIRECTSOUNDBUFFER         lpdsb;
	LPDIRECTSOUNDBUFFER         lpdsbPrimary;
	LPDIRECTSOUNDNOTIFY         lpdsNotify;
	DSBPOSITIONNOTIFY           rgdsbpn[2];
	HANDLE                      rghEvent[2];

	HANDLE hStopPlayer;
	HANDLE hPlayerThread;

	DXAudioCallback *pCallback;

	bool ResetBuffers;
	bool IgnoreFirstReach;

	BYTE PlayVolume;

	bool WideBits;
	BYTE NumChans;
};

