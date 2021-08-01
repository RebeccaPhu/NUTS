#pragma once

#include "SidebarItem.h"
#include "Defs.h"

#include <map>

typedef enum _SidePanelFags
{
	SPF_ReOrderable = 0x00000001,
	SPF_RootFS      = 0x00000002,
	SPF_Playable    = 0x00000004,
	SPF_NewDirs     = 0x00000008,
	SPF_NewImages   = 0x00000010,
	SPF_Rename      = 0x00000020,
	SPF_Copy        = 0x00000040,
	SPF_Delete      = 0x00000080,
} SidePanelFlags;

class SidePanel
{
public:
	int Create( HWND Parent, HINSTANCE hInstance );

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	int Resize();
	int SetTopic( DWORD ft, std::wstring Descriptor );
	int SetFlags( DWORD f );

public:
	static std::map<HWND, SidePanel *> panels;
	static bool WndClassReg;
	static LRESULT CALLBACK SidePanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	HWND hWnd;

private:
	HWND hParent;

	HDC  hCorner;
	HDC  hTopic;

	HFONT   hTopicFont;
	HGDIOBJ hOldTopic;
	HPEN    SepPen;

	DWORD TopicIcon;

	std::wstring TopicDescriptor;

	CRITICAL_SECTION TopicLock;

	std::vector<SidebarItem *> items;

	int line1;
	int line2;

	DWORD PanelFlags;

	SidebarItem *backItem, *newdirItem, *newimageItem, *topItem, *upItem, *downItem, *renameItem, *deleteItem, *copyItem, *playItem;

private:
	void PaintPanel( void );

public:
	SidePanel(void);
	~SidePanel(void);

};

