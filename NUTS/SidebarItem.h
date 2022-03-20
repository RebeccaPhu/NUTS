#pragma once

#include <map>

class SidebarItem
{
public:
	int Create( HWND Parent, HINSTANCE hInstance, int Icon, std::wstring &text, int y, HWND hTarget, DWORD param, bool bGroup );

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	void Refresh();
	void SetState( bool bDisabled )
	{
		Disabled = bDisabled;

		Refresh();
	}

public:
	static std::map<HWND, SidebarItem *> items;
	static bool WndClassReg;
	static LRESULT CALLBACK SidebarItemProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	HWND hWnd;

private:
	HWND hParent;

	HFONT   hFont;

	HICON hIcon;
	std::wstring Text;
	HWND hTargetWindow;
	DWORD SourceCommand;

	HBRUSH HoverBrush;
	HBRUSH ClickBrush;
	HPEN   FocusPen;

	bool Hover;
	bool Click;
	bool Disabled;
	bool Tracking;
	bool Focus;

private:
	void PaintItem( void );

public:
	SidebarItem(void);
	~SidebarItem(void);

};

