#pragma once

#include <map>

class IconButton
{
public:
	IconButton( HWND hParent, int x, int y, HICON icon );
	~IconButton(void);

public:
	static LRESULT CALLBACK IconButtonWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static std::map<HWND, IconButton *> _IconButtons;
	static bool _HasWindowClass;

	HWND  hWnd;

public:
	LRESULT WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );

	void SetTip( std::wstring Tip );

private:
	HWND  Parent;
	HICON hIcon;
	HWND  hTip;
	HPEN  hDotted;

	bool  MPressed;
	bool  KPressed;
	bool  Focus;
	bool  IgnoreKey;

	HDC     hArea;
	HBITMAP hCanvas;
	HGDIOBJ hAreaOld;

private:
	void Refresh();
	void PaintButton();
};

