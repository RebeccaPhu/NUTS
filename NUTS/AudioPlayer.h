#pragma once

#include "DXAudio.h"
#include "TempFile.h"
#include "TapeKey.h"
#include "TapeBrowser.h"
#include "TapeDefs.h"
#include <map>

#define IDM_SAVEAUDIO 42001
#define IDM_CUEJUMP   42002

class AudioPlayer : DXAudioCallback
{
public:
	AudioPlayer( CTempFile &FileObj, TapeIndex &SourceIndexes );
	~AudioPlayer(void);

public:
	int Create( HWND Parent, HINSTANCE hInstance );

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	static std::map<HWND, AudioPlayer *> players;
	static bool WndClassReg;
	static LRESULT CALLBACK AudioPlayerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	int GetBuffer( BYTE *pBuffer, DWORD lBuffer );
	void SetReached( DWORD dwReached );

public:
	HWND hWnd;

public:

private:
	CTempFile *pStore;
	TapeIndex Indexes;

	HWND hParent;

	TapeKey *Keys[ 6 ];

	TapeBrowser *pBrowser;

	HDC hCanvas;
	HBITMAP hCanvasBitmap;
	HBRUSH  hTape;
	HPEN    VLtGPen;
	HPEN    LtGPen;
	HPEN    DkGPen;
	HPEN    RedPen;
	HBRUSH  GreenBrush;
	HFONT   TapeFont1;
	HFONT   TapeFont2;
	HFONT   IndexFont;
	HBITMAP hVolume;
	HWND    hVolumeControl;
	HWND    hOptions;

	double LReelAngle;
	double RReelAngle;
	double LReelSpeed;
	double RReelSpeed;

	BYTE TapeDither[ 50 ];

	std::map<BYTE, HPEN> DitherPens;

	DWORD TapePtr;
	DWORD TapeExt;

	DXAudio SoundPlayer;

	CRITICAL_SECTION cs;

	DWORD PlayCursor;
	DWORD BackCursor;
	DWORD TC;
	bool  WasPlaying;
	bool  Playing;

	DWORD TapeVolume;

private:
	void PaintPlayer( void );
	void SensiCircle( int x, int y, int r );
	void DrawTape( int x, int y, int Size );
	void DrawIndex( void );
	void DoOptionsPopup( void );
	void DoSaveAudio( void );
	void Refresh();
};

