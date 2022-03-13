#pragma once

#include "TempFile.h"
#include "AUDIOTranslator.h"
#include "AudioPlayerElement.h"
#include "DXAudio.h"

#include "Defs.h"

class AUDIOContentViewer : DXAudioCallback
{
public:
	AUDIOContentViewer( CTempFile &fileObj, TXIdentifier TUID );
	~AUDIOContentViewer(void);

public:
	int Create(HWND Parent, HINSTANCE hInstance, int x, int y );

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	unsigned int TranslateThread();

public:
	static std::map<HWND, AUDIOContentViewer *> viewers;
	static bool WndClassReg;
	static LRESULT CALLBACK AUViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static unsigned int __stdcall _TranslateThread(void *param);

public:
	HWND hWnd;

private:
	HWND ParentWnd;

	HGDIOBJ hOld;
	HDC     hCanvasDC;
	HBITMAP hCanvas;
	HPEN    hDigitPen1;
	HPEN    hDigitPen2;
	HFONT   hFont;

	AUDIOTranslator *pXlator;

	TXIdentifier TID;

	DWORD TimeIndex;
	DWORD TimeTotal;

	AudioPlayerElement *pSongPos;
	AudioPlayerElement *pVolume;
	AudioPlayerElement *pPlaylist;

	std::vector<AudioPlayerElement *> Buttons;

	AudioTranslateOptions TXOptions;

	CTempFile AudioSource;
	CTempFile *sourceObject;

	bool Translating;
	BYTE TransSeg;

	HANDLE hTranslateThread;
	HANDLE hExitThread;

	DXAudio SoundPlayer;

	QWORD   AudioPtr;
	bool    Playing;
	bool    Paused;
	bool    DoLoop;
	BYTE    Volume;

	CRITICAL_SECTION cs;

private:
	void PaintPlayer( void );

	void DrawTime( DWORD TIndex, int x, int y, bool large );
	void DrawDigit( BYTE Digit, int x, int y, bool large );
	void DrawSeg( int x, int y, int l, bool horiz, bool cent, bool lefttop );

	void DrawBiText( AudioCuePoint *cue, int x, int y, int w, DWORD Displace );

	void SaveData();

	void ProcessButton( DWORD Button );
	void Invalidate();

/* DXAudioCallback */
public:
	int GetBuffer( BYTE *pBuffer, DWORD lBuffer );
	void SetReached( DWORD dwReached );
};

