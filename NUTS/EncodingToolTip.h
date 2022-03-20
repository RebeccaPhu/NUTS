#pragma once

#include "Plugins.h"

#include <vector>

typedef struct _TooltipRow
{
	FontIdentifier FontID;
	BYTEString Text;
} TooltipRow;

typedef std::vector<TooltipRow> TooltipDef;
typedef TooltipDef::iterator TooltipDef_iter;

class EncodingToolTip
{
public:
	int Create( HWND Parent, HINSTANCE hInstance, int x, int y, TooltipDef &tip );

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	static std::map<HWND, EncodingToolTip *> items;
	static bool WndClassReg;
	static LRESULT CALLBACK EncodingToolTipProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	HWND hWnd;

private:
	HWND hParent;

	HDC     hCanvas;
	HBITMAP hCanvasBitmap;
	HPEN    hEdgePen;
	HGDIOBJ hOld;

	int     ox;
	int     oy;

	bool    IgnoreEvents;

	TooltipDef _tip;

private:
	void PaintItem( void );

public:
	EncodingToolTip(void);
	~EncodingToolTip(void);
};

