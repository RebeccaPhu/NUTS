#pragma once

#include "stdafx.h"

#include "AUDIOTranslator.h"
#include <map>

typedef enum _AudioElementType
{
	AudioElementButton,
	AudioElementScrollBar,
	AudioElementListBox,
} AudioElementType;

#define AEWS_VERTICAL   0x00000001
#define AEWS_HORIZONTAL 0x00000002
#define AEWS_PLAY       0x00000004
#define AEWS_STOP       0x00000008
#define AEWS_PREV       0x00000010
#define AEWS_NEXT       0x00000020
#define AEWS_PAUSE      0x00000040
#define AEWS_EJECT      0x00000080
#define AEWS_SAVE       0x00000100
#define AEWS_LOOP       0x00000200

class AudioPlayerElement
{
public:
	AudioPlayerElement( HWND hParent, int x, int y, int w, int h, AudioElementType e, DWORD s );
	~AudioPlayerElement(void);

	void SetActive( bool f );
	void SetLimit( DWORD max );
	void SetPosition( DWORD pos );
	void SetDisabled( bool f );
	void SetActiveUpdate( bool f );
	void AddCuePoint( AudioCuePoint cue );
	void SetCuePoint( QWORD p );
	void AddTooltip( std::wstring Tip );

public:
	static std::map<HWND, AudioPlayerElement *> _AudioPlayerElementClassMap;
	static bool _HasWindowClass;

	HWND  hWnd;

	AudioElementType ElementType;

	static WORD    _RefCount;
	static HDC     _ButtonSource;
	static HBITMAP _ButtonCanvas;

public:
	LRESULT ButtonWindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );
	LRESULT ScrollBarWindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );
	LRESULT ListBoxWindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );

private:
	DWORD ElementStyle;
	DWORD maxpos;

	HWND    Parent;
	HDC     hCanvasDC;
	HBITMAP hCanvas;
	HGDIOBJ hOld;
	int     ew;
	int     eh;
	HPEN    hMainPen;
	HPEN    hSecPen;
	HBRUSH  hMainBrush;
	HBRUSH  hSecBrush;
	HBRUSH  hTextBrush;
	HFONT   hFont;
	HFONT   hBoldFont;
	HWND    hTip;

	bool    OverElement;
	bool    Active;
	bool    Scrolling;
	bool    ActiveUpdate;
	bool    Disabled;
	int     mo;
	int     se;
	int     fe;
	int     pe;
	int     maxe;
	int     nume;
	DWORD   LastClickTime;
	bool    FirstClick;

	AudioPlayerElement *pScrollBar;

	std::vector<AudioCuePoint> cues;

private:
	void PaintButton();
	void PaintScrollBar();
	void PaintListBox();

	void DrawBiText( AudioCuePoint *cue, int x, int y, int w, DWORD Displace, bool Invert, bool Bold );

	void BlitCanvas( HDC hDC );
	void Invalidate();
	void Track();
	void SetMO( LPARAM lParam );
	void DoListboxMouse( UINT uMsg, LPARAM lParam );
};
