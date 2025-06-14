#pragma once

#include "BitmapCache.h"
#include "FileSystem.h"
#include "EncodingEdit.h"
#include "EncodingToolTip.h"
#include "FontBitmap.h"
#include "DropSite.h"
#include "SidePanel.h"
#include "NUTSUI.h"

#include <vector>

#define ROOT_OBJECT_HOOK_EXT 3

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
	FileSystem *DoRootHook();
	
	std::vector<NativeFile> GetSelection( void );

public:
	static std::map<HWND, CFileViewer *> viewers;
	static LRESULT CALLBACK FileViewerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static bool WndClassReg;
	static INT_PTR CALLBACK RenameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BYTE *pRenameFile;
	static EncodingEdit *pRenameEdit;
	static BYTE *pRenameExt;
	static EncodingEdit *pRenameEditX;
	static DWORD StaticFlags;
	static DWORD StaticFileFlags;
	static EncodingIdentifier StaticEncoding;
	static INT_PTR CALLBACK NewDirDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static BYTE *pNewDir;
	static EncodingEdit *pNewDirEdit;
	static BYTE *pNewDirX;
	static EncodingEdit *pNewDirEditX;
	static bool NewDirSupportsFOP;
	static INT_PTR CALLBACK DirTypeDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

public:
	FileSystem *FS;
	HWND       hWnd;
	bool       Updated;
	BYTE       PaneIndex;

	std::vector<FileSystem *> *pFSStack;

	std::vector<TitleComponent> *pTitleStack;

private:
	HWND    ParentWnd;
	HWND    hScrollBar;
	HWND    hProgress;

	SidePanel SideBar;

	HFONT   titleBarFont;
	HFONT   filenameFont;

	HBITMAP viewBuffer;
	HGDIOBJ viewObj;
	HDC     viewDC;
	HDC     hSourceDC;
	long    KnownX,KnownY;

	int     FileEntries;

	std::vector<bool> FileSelections;
	std::vector<FontBitmap *> FileLabels;
	std::vector<FontBitmap *> FileDescs;

	bool    ParentSelected;
	bool    HasFocus;

	long    ScrollStartY;
	long    ScrollMax;

	DWORD   LastClick;

	DWORD   mouseX,mouseY;
	DWORD   dragX,dragY;
	bool    Dragging;
	bool    Bounding;
	bool    Tracking;
	int     DragType;
	bool    SelectionAtMouseDown;
	bool    MouseDown;
	WORD    DIndex;

	QWORD   TooltipMoving;
	bool    TooltipPresent;
	long    tooltipx;
	long    tooltipy;
	QWORD   tooltipclock;
	
	EncodingToolTip *tooltip;

	DWORD   ItemWidth;
	DWORD   ItemHeight;
	DWORD   IconsPerLine;
	DWORD   LinesPerWindow;
	DWORD   IconsPerWindow;
	DWORD   IconYOffset;
	DWORD   WindowHeight;
	DWORD   WindowWidth;
	DWORD   InvokedHook;

	DropSite *pDropSite;

	int		IgnoreKeys;

	/* Caching */
	CRITICAL_SECTION CacheLock;

	std::vector<NativeFile> TheseFiles;
	ResolvedIconList        TheseIcons;

private:
	void  DrawBasicLayout();
	void  DrawFile(int i, NativeFile *pFile, DWORD Icon, bool Selected);
	void  ToggleItem(int x, int y);
	void  ActivateItem(int x, int y);
	void  ClearItems( bool DoUpdate );
	int   GetItem(DWORD x, DWORD y);
	bool  CheckClick();
	void  CheckDragType(long dragX, long dragY);
	void  DoScroll(WPARAM wParam, LPARAM lParam);
	void  CFileViewer::PopulateFSMenus( HMENU hPopup, bool Override );
	void  CFileViewer::PopulateXlatorMenus( HMENU hPopup );
	void  RecalculateDimensions( RECT &wndRect );
	void  GetCliRect( RECT &client );
	void  DoSelections( UINT Msg, WPARAM wParam, LPARAM lParam );
	DWORD GetSelectionCount( void );
	void  DoContentViewer( TXIdentifier PrefTUID = TX_Null );
	void  DoSwapFiles( BYTE UpDown );
	void  DoKeyControls( UINT message, WPARAM wParam, LPARAM lParam );
	void  DoContextMenu( void );
	void  DoLocalCommandMenu( HMENU hPopup );
	void  DoStatusBar( void );
	void  FreeLabels( void );
	void  DoPlayAudio( void );
	void  DoFileToolTip();
	void  DoTitleBarToolTip();


	std::map<UINT, FSIdentifier> MenuFSMap;
	std::map<UINT, TXIdentifier> MenuXlatorMap;

	DisplayType Displaying;

	HPEN Pens[3];
	BYTE CurrentPen;
	HBRUSH hViewerBrush;

	std::vector<TitleComponent> TitleStack;

	long CalculatedY;

	volatile bool IsSearching;
	int  LastItemIndex;

public:
	void Resize(int w, int h);
	void SetTitleStack( std::vector<TitleComponent> NewTitleStack );
	std::vector<TitleComponent> GetTitleStack( void );

	std::vector<int> SelectionStack;

	DWORD GetSelectedIndex( void );

	void  SetSearching( bool s );

	void  Refresh();

	void ReCalculateTitleStack( );

	void UpdateSidePanelFlags();

	void RenameFile( void );
	void NewDirectory( void );
	void SetDirType( void );

	void LockDisplay() { EnterCriticalSection( &CacheLock ); }
	void UnlockDisplay() { EnterCriticalSection( &CacheLock ); }
};

#define FILESYS_MENU_BASE 43000
#define GFX_MENU_BASE     43400
#define TXT_MENU_BASE     43600
#define LC_MENU_BASE      43800
#define AUD_MENU_BASE     44000
#define ROOT_HOOK_BASE    44200

#define FILESYS_MENU_END  ( FILESYS_MENU_BASE + 399 )
#define GFX_MENU_END      ( GFX_MENU_BASE + 199 )
#define TXT_MENU_END      ( TXT_MENU_BASE + 199 )
#define LC_MENU_END       ( LC_MENU_BASE + 199 )
#define AUD_MENU_END      ( AUD_MENU_BASE + 199 )
#define ROOT_HOOK_END     ( ROOT_HOOK_BASE + 199 )

