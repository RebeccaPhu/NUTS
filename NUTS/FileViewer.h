#pragma once

#include "BitmapCache.h"
#include "FileSystem.h"
#include "TEXTContentViewer.h"
#include "SCREENContentViewer.h"
#include "EncodingEdit.h"
#include "Defs.h"

#include <vector>

typedef enum __DisplayType {
	DisplayLargeIcons = 0,
	DisplayDetails    = 1,
	DisplayList       = 2,
} DisplayType;

class CFileViewer
{
public:
	CFileViewer(void);
	~CFileViewer(void);

public:
	int Create(HWND Parent, HINSTANCE hInstance, int x, int w, int h);

	LRESULT	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	void Redraw();
	void Update();

	void StartDragging();
	void EndDragging();
	void SetDragType(int DType);
	
	std::vector<NativeFile> GetSelection( void );

public:
	static std::map<HWND, CFileViewer *> viewers;
	static LRESULT CALLBACK FileViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static bool WndClassReg;
	static INT_PTR CALLBACK RenameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BYTE *pRenameFile;
	static EncodingEdit *pRenameEdit;
	static INT_PTR CALLBACK NewDirDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BYTE *pNewDir;
	static EncodingEdit *pNewDirEdit;

public:
	FileSystem *FS;
	HWND       hWnd;
	bool       Updated;
	DWORD      CurrentFSID;
	BYTE       PaneIndex;

	std::vector<HWND> ControlButtons;

private:
	HWND    ParentWnd;
	HWND    hScrollBar;
	HWND    hProgress;

	HFONT   titleBarFont;
	HFONT   filenameFont;

	HBITMAP viewBuffer;
	HGDIOBJ viewObj;
	HDC     viewDC;
	long    KnownX,KnownY;

	int     FileEntries;

	std::vector<bool> FileSelections;

	bool    ParentSelected;
	bool    HasFocus;

	long    ScrollStartY;
	long    ScrollMax;

	DWORD   LastClick;

	DWORD   mouseX,mouseY;
	DWORD   dragX,dragY;
	bool    Dragging;
	bool    Bounding;
	int     DragType;
	bool    SelectionAtMouseDown;
	bool    MouseDown;
	WORD    DIndex;

	DWORD   ItemWidth;
	DWORD   ItemHeight;
	DWORD   IconsPerLine;
	DWORD   LinesPerWindow;
	DWORD   IconsPerWindow;
	DWORD   IconYOffset;
	DWORD   WindowHeight;
	DWORD   WindowWidth;

	CRITICAL_SECTION RedrawLock;

private:
	void  DrawBasicLayout();
	void  DrawFile(int i, NativeFile *pFile, DWORD Icon, bool Selected);
	void  ToggleItem(int x, int y);
	void  ActivateItem(int x, int y);
	void  ClearItems( bool DoUpdate );
	DWORD GetItem(DWORD x, DWORD y);
	bool  CheckClick();
	void  CheckDragType(long dragX, long dragY);
	void  DoScroll(WPARAM wParam, LPARAM lParam);
	void  CFileViewer::PopulateFSMenus( HMENU hPopup );
	void  CFileViewer::PopulateXlatorMenus( HMENU hPopup );
	void  RecalculateDimensions( RECT &wndRect );
	void  GetCliRect( RECT &client );
	void  DoSelections( UINT Msg, WPARAM wParam, LPARAM lParam );
	DWORD GetSelectionCount( void );
	void  DoSCREENContentViewer( DWORD PrefTUID  = NULL );
	void  DoTEXTContentViewer( DWORD PrefTUID = NULL );
	void  DoContentViewer( void );
	void  RenameFile( void );
	void  DoSwapFiles( BYTE UpDown );
	void  DoKeyControls( UINT message, WPARAM wParam, LPARAM lParam );
	void  DoContextMenu( void );
	void  DoStatusBar( void );
	void  NewDirectory( void );

	std::map<UINT, DWORD> MenuFSMap;
	std::map<UINT, DWORD> MenuXlatorMap;

	DisplayType Displaying;

	HPEN Pens[3];
	BYTE CurrentPen;
	HBRUSH hViewerBrush;

	std::vector<TitleComponent> TitleStack;

	long CalculatedY;

	bool IsSearching;
	int  LastItemIndex;

public:
	void Resize(int w, int h);
	void SetTitleStack( std::vector<TitleComponent> NewTitleStack );
	std::vector<TitleComponent> GetTitleStack( void );

	std::vector<int> SelectionStack;

	DWORD GetSelectedIndex( void );

	void  SetSearching( bool s );
};

#define FILESYS_MENU_BASE 43000
#define GFX_MENU_BASE     43600
#define TXT_MENU_BASE     43800

#define FILESYS_MENU_END  ( FILESYS_MENU_BASE + 499 )
#define GFX_MENU_END      ( GFX_MENU_BASE + 199 )
#define TXT_MENU_END      ( TXT_MENU_BASE + 199 )
