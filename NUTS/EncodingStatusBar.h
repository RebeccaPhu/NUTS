#pragma once

#include "Defs.h"

#include <map>

class EncodingStatusBar
{
public:
	EncodingStatusBar( HWND hParent );
	~EncodingStatusBar( void );

	static std::map<HWND, EncodingStatusBar *> StatusBars;
	static bool _HasWindowClass;

	LRESULT WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );

	HWND hWnd;

	int AddPanel( DWORD PanelID, DWORD Width, BYTE *Text, FontIdentifier FontID, DWORD Flags );
	int SetPanelWidth( DWORD PanelID, DWORD NewWidth );
	int SetPanelText( DWORD PanelID, FontIdentifier FontID, BYTE *Text );
	int SetPanelFont( DWORD PanelID, FontIdentifier FontID );

	int NotifyWindowSizeChanged( void );

private:
	HWND ParentWnd;

	PanelList Panels;

private:
	int DoPaint( void );
};

