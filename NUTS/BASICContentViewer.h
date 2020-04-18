#pragma once

#include "TempFile.h"

#include <map>

class CBASICContentViewer
{
public:
	CBASICContentViewer( CTempFile FileObj );
	~CBASICContentViewer(void);

public:
	int Create(HWND Parent, HINSTANCE hInstance, int x, int w, int h);

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	HWND   hWnd;

	BYTE  *pContent;
	QWORD lContent;

public:
	static LRESULT CALLBACK BCViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static std::map<HWND,CBASICContentViewer *> viewers;
	static bool WndClassReg;

private:
	HWND	ParentWnd;
	HWND	hText;

	char	*pTextBuffer;
	long	lTextBuffer;

private:
	void	DeTokenise();
};
