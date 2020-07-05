// NUTS.cpp : Defines the entry point for the application.
//

//#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "stdafx.h"
#include "NUTS.h"
#include "FormatWizard.h"
#include "Plugins.h"
#include "AppAction.h"
#include "AboutBox.h"
#include "resource.h"
#include "ExtensionRegistry.h"
#include "EncodingClipboard.h"
#include "EncodingStatusBar.h"
#include "Preference.h"
#include "DataSourceCollector.h"
#include "NUTSError.h"

#include <winioctl.h>
#include <process.h>
#include <time.h>

#include <vector>
#include <deque>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                     // current instance
bool      FSChangeLock = false;

TCHAR szTitle[MAX_LOADSTRING];       // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING]; // the main window class name

HWND hMainWnd;	  // Main window handle
HWND hNextClip;   // Next Clipboard Window

EncodingStatusBar *pStatusBar;

#define PANELID_LEFT_STATUS  1
#define PANELID_LEFT_FONT    2
#define PANELID_RIGHT_STATUS 3
#define PANELID_RIGHT_FONT   4

bool Tracking = false;
bool UseResolvedIcons = Preference( L"UseResolvedIcons" );
bool HideSidecars     = Preference( L"HideSidecars" );

//	File viewer window classes:
CFileViewer	leftPane,rightPane;

// FileSystem Breadcrumbs:
std::vector<FileSystem *> leftFS;
std::vector<FileSystem *> rightFS;

std::vector<TitleComponent> leftTitles;
std::vector<TitleComponent> rightTitles;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

unsigned int __stdcall ActionThread(void *param);

HANDLE leftThread  = NULL;
HANDLE rightThread = NULL;

HWND   FocusPane   = NULL;
HWND   DragSource  = NULL;

std::map< DWORD, GlobalCommand> GlobalCommandMap;

typedef struct _EnterVars
{
	CFileViewer *pane;
	std::vector<FileSystem *> *pStack;
	std::vector<TitleComponent> *pTitleStack;
	int EnterIndex;
	FileSystem *FS;
	DWORD FSID;
} EnterVars;

DataSourceCollector *pCollector = new DataSourceCollector();

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	pGlobalError = new NUTSError( 0, L"" );

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MSG msg;
	HACCEL hAccelTable;

	leftPane.PaneIndex  = 0;
	rightPane.PaneIndex = 1;

	/* This is used by the drop target mechanism */
	OleInitialize( NULL );

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_NUTS, szWindowClass, MAX_LOADSTRING);

	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) {
		return FALSE;
	}

	InitAppActions();

	hAccelTable	= LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NUTS));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	StopAppActions();

	return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));
	wcex.hCursor		= NULL;
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE( IDC_NUTS );
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= NULL; // LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

LRESULT CALLBACK DummyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void ReCalculateTitleStack( std::vector<FileSystem *> *pFS, std::vector<TitleComponent> *pTitleStack, CFileViewer *pPane )
{
	std::vector< FileSystem *>::iterator iStack;
	
	pTitleStack->clear();

	iStack = pFS->begin();

	/* Skip over the root if we have more than 1 FS deep */
	if ( pFS->size() > 1U )
	{
		iStack++;
	}

	while ( iStack != pFS->end() )
	{
		TitleComponent t;

		if ( (*iStack)->EnterIndex == 0xFFFFFFFF )
		{
			strncpy_s( (char *) t.String, 512, (char *) (*iStack)->GetTitleString( nullptr ), 511 );
		}
		else
		{
			strncpy_s( (char *) t.String, 512, (char *) (*iStack)->GetTitleString( &(*iStack)->pDirectory->Files[ (*iStack)->EnterIndex ] ), 511 );
		}

		t.Encoding = (*iStack)->GetEncoding();

		pTitleStack->push_back( t );

		iStack++;
	}

	pPane->SetTitleStack( *pTitleStack );
}

void ConfigureExtrasMenu( void )
{
	HMENU hMenuBar = GetMenu( hMainWnd );

	/* Windows is a bit silly here. Unless you create a menu for the menu item, Windows assumes it is a command in itself.
	   So create a submenu here */

	HMENU hExtraMenu = CreatePopupMenu();

	MENUITEMINFO mii = { 0 };
	mii.cbSize = sizeof( MENUITEMINFO );
	mii.fMask = MIIM_ID | MIIM_STRING | MIIM_SUBMENU ;
	mii.wID = 42999;
	mii.hSubMenu = hExtraMenu;
	mii.dwTypeData = _T( "Extras" );

	InsertMenuItem( hMenuBar, (UINT) 2, TRUE, &mii );

	/* Now add the items */
	GlobalCommandSet menus = FSPlugins.GetGlobalCommands();
	GlobalCommandSet::iterator iter;

	if ( menus.size() == 0 )
	{
		AppendMenu( hExtraMenu, MF_STRING, (UINT) EXTRA_MENU_BASE, L"None available" );

		EnableMenuItem( hExtraMenu, 0, MF_DISABLED | MF_BYPOSITION );
	}

	UINT index = EXTRA_MENU_BASE;

	for (iter = menus.begin(); iter != menus.end(); iter++ )
	{
		AppendMenu( hExtraMenu, MF_STRING, (UINT) index, iter->Text.c_str() );

		GlobalCommandMap[ index ] = *iter;
	}
}

void SetUpBaseMappings( void )
{
	ExtReg.RegisterExtension( L"BIN", FT_Binary,    FT_Binary  );
	ExtReg.RegisterExtension( L"BAT", FT_Script,    FT_Script  );
	ExtReg.RegisterExtension( L"EXE", FT_App,       FT_App     );
	ExtReg.RegisterExtension( L"DAT", FT_Data,      FT_Data    );
	ExtReg.RegisterExtension( L"BMP", FT_Graphic,   FT_Graphic );
	ExtReg.RegisterExtension( L"JPG", FT_Graphic,   FT_Graphic );
	ExtReg.RegisterExtension( L"PNG", FT_Graphic,   FT_Graphic );
	ExtReg.RegisterExtension( L"GIF", FT_Graphic,   FT_Graphic );
	ExtReg.RegisterExtension( L"WAV", FT_Sound,     FT_Sound   );
	ExtReg.RegisterExtension( L"MP3", FT_Sound,     FT_Sound   );
	ExtReg.RegisterExtension( L"VOC", FT_Sound,     FT_Sound   );
	ExtReg.RegisterExtension( L"INI", FT_Pref,      FT_Pref    );
	ExtReg.RegisterExtension( L"TXT", FT_Text,      FT_Text    );
	ExtReg.RegisterExtension( L"ZIP", FT_Archive,   FT_Archive );
	ExtReg.RegisterExtension( L"LHA", FT_Archive,   FT_Archive );
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) { 
	HWND hWnd;

	hInst = hInstance; // Store instance handle in our global variable

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

	BitmapCache.LoadBitmaps();

	SetUpBaseMappings();

	DWORD WindowWidth  = Preference( L"WindowWidth",  (DWORD) 800 );
	DWORD WindowHeight = Preference( L"WindowHeight", (DWORD) 500 );

	hWnd = CreateWindow(
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS,
		CW_USEDEFAULT, 0, WindowWidth, WindowHeight,
		NULL, NULL, hInstance, NULL
	);

	if (!hWnd)
		return FALSE;

	hMainWnd	= hWnd;

	FSPlugins.LoadPlugins();

	ConfigureExtrasMenu();

	leftPane.FS  = new RootFileSystem(); 
	rightPane.FS = new RootFileSystem(); 

	leftFS.push_back( leftPane.FS );
	rightFS.push_back( rightPane.FS );

	ReCalculateTitleStack( &leftFS, &leftTitles, &leftPane );
	ReCalculateTitleStack( &rightFS, &rightTitles, &rightPane );

	ShowWindow(hWnd, nCmdShow);

	UpdateWindow(hWnd);

	SetFocus( leftPane.hWnd );

	FocusPane = leftPane.hWnd;

	return TRUE;
}

unsigned int __stdcall DoParentThread(void *param);

int DoParent(CFileViewer *pane) {
	DWORD dwthreadid;

	EnterVars *pVars = new EnterVars;

	pVars->pane = pane;

	if ( pane == &leftPane )
	{
		leftThread = (HANDLE) _beginthreadex(NULL, NULL, DoParentThread, pVars, NULL, (unsigned int *) &dwthreadid);
	}
	else
	{
		rightThread = (HANDLE) _beginthreadex(NULL, NULL, DoParentThread, pVars, NULL, (unsigned int *) &dwthreadid);
	}

	return 0;
}

unsigned int __stdcall DoParentThread( void *param )
{
	EnterVars *pVars = (EnterVars *) param;

	pVars->pane->SetSearching( true );

	FileSystem *FS = pVars->pane->FS;

	std::vector<TitleComponent> *pTitleStack = &leftTitles;
	std::vector<FileSystem *> *pFS = &leftFS;

	if ( pVars->pane == &rightPane )
	{
		pFS = &rightFS;
		pTitleStack = &rightTitles;
	}

	if (FS->IsRoot()) {
		delete FS;

		pFS->pop_back();

		FS = pFS->back();

		pVars->pane->FS          = FS;
		pVars->pane->CurrentFSID = FS->FSID;
	} else {
		if ( FS->Parent() != NUTS_SUCCESS )
		{
			NUTSError::Report( L"Parent Directory", hMainWnd );
		}
	}

	if ( pVars->pane->SelectionStack.size() > 0 )
	{
		pVars->pane->SelectionStack.pop_back();
	}

	pVars->pane->FS->EnterIndex = 0xFFFFFFFF;

	ReCalculateTitleStack( pFS, pTitleStack, pVars->pane );

	if ( pVars->pane == &leftPane )  { CloseHandle( leftThread );  leftThread  = NULL; }
	if ( pVars->pane == &rightPane ) { CloseHandle( rightThread ); rightThread = NULL; }

	pVars->pane->SetSearching( false );
	pVars->pane->Updated = true;
	pVars->pane->Update();

	delete pVars;

	return 0;
}

unsigned int __stdcall DoEnterThread(void *param);

int DoEnter(CFileViewer *pane, std::vector<FileSystem *> *pStack, std::vector<TitleComponent> *pTitleStack, int EnterIndex) {
	DWORD dwthreadid;

	EnterVars *pVars = new EnterVars;

	pVars->EnterIndex  = EnterIndex;
	pVars->pane        = pane;
	pVars->pStack      = pStack;
	pVars->pTitleStack = pTitleStack;

	if ( pane == &leftPane )
	{
		leftThread = (HANDLE) _beginthreadex(NULL, NULL, DoEnterThread, pVars, NULL, (unsigned int *) &dwthreadid);
	}
	else
	{
		rightThread = (HANDLE) _beginthreadex(NULL, NULL, DoEnterThread, pVars, NULL, (unsigned int *) &dwthreadid);
	}

	return 0;
}

unsigned int __stdcall DoEnterThread(void *param)
{
	EnterVars *pVars = (EnterVars *) param;

	pVars->pane->SetSearching( true );

	FileSystem *pCurrentFS = pVars->pStack->back();

	if ( pCurrentFS->pDirectory->Files[pVars->EnterIndex].Flags & FF_Directory )
	{
		if ( pCurrentFS->ChangeDirectory( pVars->EnterIndex ) )
		{
			NUTSError::Report( L"Change Directory", hMainWnd );

		}
		else
		{
			pVars->pane->SelectionStack.push_back( -1 );
			pCurrentFS->EnterIndex = 0xFFFFFFFF;

			ReCalculateTitleStack( pVars->pStack, pVars->pTitleStack, pVars->pane );
		}

		if ( pVars->pane == &leftPane )  { CloseHandle( leftThread );  leftThread  = NULL; }
		if ( pVars->pane == &rightPane ) { CloseHandle( rightThread ); rightThread = NULL; }

		pVars->pane->SetSearching( false );
		pVars->pane->Updated	= true;
		pVars->pane->Update();

		delete pVars;

		return 0;
	}

	if (
		( pCurrentFS->pDirectory->Files[pVars->EnterIndex].Type != FT_MiscImage ) &&
		( pCurrentFS->pDirectory->Files[pVars->EnterIndex].Type != FT_Archive ) )
	{
		if ( pVars->pane == &leftPane )  { CloseHandle( leftThread );  leftThread  = NULL; }
		if ( pVars->pane == &rightPane ) { CloseHandle( rightThread ); rightThread = NULL; }

		pVars->pane->SetSearching( false );

		delete pVars;

		return 0;
	}

	FileSystem *pNewFS = pCurrentFS->FileFilesystem( pVars->EnterIndex );

	if ( pNewFS != nullptr )
	{
		pNewFS->EnterIndex       = 0xFFFFFFFF;
		pNewFS->pParentFS        = pCurrentFS;
		pNewFS->UseResolvedIcons = UseResolvedIcons;
		pNewFS->HideSidecars     = HideSidecars;

		if ( pNewFS->Init() != NUTS_SUCCESS )
		{
			NUTSError::Report( L"Initialise File System", hMainWnd );
		}

		pNewFS->hMainWindow = hMainWnd;
		pNewFS->hPaneWindow = pVars->pane->hWnd;

		if ( pNewFS ) {
			pVars->pStack->push_back( pNewFS );

			pVars->pane->FS	      = pNewFS;
			pVars->pane->CurrentFSID = pNewFS->FSID;

			pVars->pane->SelectionStack.push_back( -1 );
		}
	}
	else
	{
		pGlobalError->GlobalCode = NUTS_SUCCESS;

		DataSource *pSource = pCurrentFS->FileDataSource( pVars->EnterIndex );

		if ( pSource != nullptr )
		{
			pNewFS = FSPlugins.FindAndLoadFS( pSource, &pCurrentFS->pDirectory->Files[ pVars->EnterIndex ] );

			pSource->Release();

			if ( pNewFS != nullptr )
			{
				pNewFS->EnterIndex       = 0xFFFFFFFF;
				pNewFS->pParentFS        = pCurrentFS;
				pNewFS->UseResolvedIcons = UseResolvedIcons;
				pNewFS->HideSidecars     = HideSidecars;

				if ( pNewFS->Init() != NUTS_SUCCESS )
				{
					NUTSError::Report( L"Initialise File System", hMainWnd );
				}

				pNewFS->IsRaw = false;

				pVars->pStack->push_back( pNewFS );

				pVars->pane->FS	           = pNewFS;
				pVars->pane->CurrentFSID   = pNewFS->FSID;
				pNewFS->hMainWindow        = hMainWnd;
				pNewFS->hPaneWindow        = pVars->pane->hWnd;

				pVars->pane->SelectionStack.push_back( -1 );
			}
			else
			{
				if ( pGlobalError->GlobalCode != NUTS_SUCCESS )
				{
					NUTSError::Report( L"Load Data Source", hMainWnd );
				}
				else
				{
					MessageBox(hMainWnd,
						L"The drive or image contains an unrecognised file system.\n\nDo you need to run NUTS as Administrator?",
						L"NUTS FileSystem Probe", MB_OK|MB_ICONSTOP
					);
				}

				if ( pVars->pane == &leftPane )  { CloseHandle( leftThread );  leftThread  = NULL; }
				if ( pVars->pane == &rightPane ) { CloseHandle( rightThread ); rightThread = NULL; }

				pVars->pane->SetSearching( false );

				delete pVars;

				return NUTSError( 0x00000022, L"Unrecognised file system" );
			}
		}
		else
		{

			if ( pVars->pane == &leftPane )  { CloseHandle( leftThread );  leftThread  = NULL; }
			if ( pVars->pane == &rightPane ) { CloseHandle( rightThread ); rightThread = NULL; }

			pVars->pane->SetSearching( false );

			delete pVars;

			NUTSError::Report( L"Load Data Source", hMainWnd );

			return 0;
		}
	}

	if ( pNewFS->Flags & FSF_Reorderable )
	{
		EnableWindow( pVars->pane->ControlButtons[ 2 ], TRUE );
		EnableWindow( pVars->pane->ControlButtons[ 3 ], TRUE );
	}
	else
	{
		EnableWindow( pVars->pane->ControlButtons[ 2 ], FALSE );
		EnableWindow( pVars->pane->ControlButtons[ 3 ], FALSE );
	}

	pCurrentFS->EnterIndex = pVars->EnterIndex;

	ReCalculateTitleStack( pVars->pStack, pVars->pTitleStack, pVars->pane );

	if ( pVars->pane == &leftPane )  { CloseHandle( leftThread );  leftThread  = NULL; }
	if ( pVars->pane == &rightPane ) { CloseHandle( rightThread ); rightThread = NULL; }

	pVars->pane->SetSearching( false );
	pVars->pane->Updated = true;
	pVars->pane->Update();

	delete pVars;

	return 0; 
}

void DoResizeWindow(HWND hWnd) {
	WINDOWPLACEMENT	placement;
	RECT			rect;

	GetWindowPlacement(hWnd, &placement);

	if (placement.showCmd == SW_MINIMIZE)
		return;

	GetClientRect(hWnd, &rect);

	int	paneWidth	= (rect.right - rect.left) / 2;
	int	paneHeight	= (rect.bottom - rect.top) - 24;

	paneWidth -= 8;
	paneHeight -= 2;

	SetWindowPos(leftPane.hWnd, NULL, 4, 0, paneWidth, paneHeight, NULL);
	SetWindowPos(rightPane.hWnd, NULL, paneWidth + 12, 0, paneWidth, paneHeight, NULL);

	leftPane.Resize( paneWidth, paneHeight );
	rightPane.Resize( paneWidth, paneHeight );

	pStatusBar->NotifyWindowSizeChanged();

	pStatusBar->SetPanelWidth( PANELID_LEFT_STATUS,  paneWidth - 75 );
	pStatusBar->SetPanelWidth( PANELID_LEFT_FONT,    80  );
	pStatusBar->SetPanelWidth( PANELID_RIGHT_STATUS, paneWidth - 80 );
	pStatusBar->SetPanelWidth( PANELID_RIGHT_FONT,   75 );

	GetWindowRect( hWnd, &rect );

	Preference( L"WindowWidth" )  = (DWORD) rect.right - rect.left;
	Preference( L"WindowHeight" ) = (DWORD) rect.bottom - rect.top;
}

void CreateStatusBar(HWND hWnd) {
	pStatusBar = new EncodingStatusBar( hWnd );

	pStatusBar->AddPanel( PANELID_LEFT_STATUS,  50, (BYTE *) "",       FONTID_PC437, PF_LEFT   );
	pStatusBar->AddPanel( PANELID_LEFT_FONT,    50, (BYTE *) "PC437",  FONTID_PC437, PF_CENTER );
	pStatusBar->AddPanel( PANELID_RIGHT_STATUS, 50, (BYTE *) "",       FONTID_PC437, PF_LEFT   );
	pStatusBar->AddPanel( PANELID_RIGHT_FONT,   50, (BYTE *) "PC437",  FONTID_PC437, PF_CENTER | PF_SIZER | PF_SPARE );
}

void TrackMouse( void )
{
	if ( !Tracking )
	{
		TRACKMOUSEEVENT tme;

		tme.cbSize      = sizeof( TRACKMOUSEEVENT );
		tme.dwFlags     = TME_NONCLIENT;
		tme.dwHoverTime = HOVER_DEFAULT;
		tme.hwndTrack   = hMainWnd;

		TrackMouseEvent( &tme );

		Tracking = true;
	}
}

unsigned int __stdcall DoEnterAsThread(void *param);

void DoEnterAs( std::vector<FileSystem *> *pStack, CFileViewer *pPane, FileSystem *pCurrentFS, std::vector<TitleComponent> *pTitleStack, DWORD FSID )
{
	DWORD dwthreadid;

	EnterVars *pVars = new EnterVars;

	pVars->pane        = pPane;
	pVars->pStack      = pStack;
	pVars->pTitleStack = pTitleStack;
	pVars->FSID        = FSID;
	pVars->FS          = pCurrentFS;

	if ( pPane == &leftPane )
	{
		leftThread = (HANDLE) _beginthreadex(NULL, NULL, DoEnterAsThread, pVars, NULL, (unsigned int *) &dwthreadid);
	}
	else
	{
		rightThread = (HANDLE) _beginthreadex(NULL, NULL, DoEnterAsThread, pVars, NULL, (unsigned int *) &dwthreadid);
	}
}

unsigned int __stdcall DoEnterAsThread( void *param )
{
	EnterVars *pVars = (EnterVars *) param;

	DWORD Index = pVars->pane->GetSelectedIndex();

	DataSource *pSource    = pVars->FS->FileDataSource( Index );
	NativeFile file        = pVars->FS->pDirectory->Files[ Index ];

	pVars->FS->EnterIndex = Index;

	if ( pSource == nullptr )
	{
		if ( pGlobalError->GlobalCode == 0 )
		{
			MessageBox( hMainWnd, L"Unable to load data source", L"NUTS", MB_ICONEXCLAMATION | MB_OK );
		}
		else
		{
			NUTSError::Report( L"Load Data Source", hMainWnd );
		}

		if ( pVars->pane == &leftPane )  { CloseHandle( leftThread );  leftThread  = NULL; }
		if ( pVars->pane == &rightPane ) { CloseHandle( rightThread ); rightThread = NULL; }

		delete pVars;

		return 0;
	}

	bool IsRaw = IsRawFS( UString( (char *) pVars->FS->pDirectory->Files[ Index ].Filename ) );

	if (!IsRaw) {
		if ( MessageBox( hMainWnd,
			L"The drive you have selected is recognised by Windows as containing a valid volume.\n\n"
			L"Entering this filesystem with an explicit handler will almost certainly cause untold misery, and should "
			L"only be attempted if you are absolutely certain you know what you are doing.\n\n"
			L"Are you sure you want to proceed?",
			L"NUTS File System Probe", MB_YESNO | MB_ICONEXCLAMATION ) != IDYES )
		{
			if ( pVars->pane == &leftPane )  { CloseHandle( leftThread );  leftThread  = NULL; }
			if ( pVars->pane == &rightPane ) { CloseHandle( rightThread ); rightThread = NULL; }

			delete pVars;

			return 0;
		}
	}

	pVars->pane->SetSearching( true );

	pGlobalError->GlobalCode = NUTS_SUCCESS;

	if ( pVars->FSID != FS_Null )
	{
		FileSystem	*newFS = FSPlugins.LoadFS( pVars->FSID, pSource, false );

		pSource->Release();

		if (newFS) {
			newFS->EnterIndex       = 0xFFFFFFFF;
			newFS->pParentFS        = pVars->FS;
			newFS->UseResolvedIcons = UseResolvedIcons;
			newFS->HideSidecars     = HideSidecars;
			newFS->hMainWindow      = hMainWnd;
			newFS->hPaneWindow      = pVars->pane->hWnd;

			if ( newFS->Init() != NUTS_SUCCESS )
			{
				NUTSError::Report( L"Initialise File System", hMainWnd );
			}

			pVars->pane->FS    = newFS;

			pVars->pStack->push_back( newFS );

			pVars->pane->CurrentFSID = newFS->FSID;

			pVars->pane->SelectionStack.push_back( -1 );

			if ( newFS->Flags & FSF_Reorderable )
			{
				EnableWindow( pVars->pane->ControlButtons[ 2 ], TRUE );
				EnableWindow( pVars->pane->ControlButtons[ 3 ], TRUE );
			}
			else
			{
				EnableWindow( pVars->pane->ControlButtons[ 2 ], FALSE );
				EnableWindow( pVars->pane->ControlButtons[ 3 ], FALSE );
			}
		}
		else
		{
			if ( pGlobalError->GlobalCode != NUTS_SUCCESS )
			{
				NUTSError::Report( L"Load Data Source", hMainWnd );
			}
			else
			{
				MessageBox( hMainWnd, L"Unable to load data source", L"NUTS", MB_ICONERROR | MB_OK );
			}
		}
	}

	ReCalculateTitleStack( pVars->pStack, pVars->pTitleStack, pVars->pane );

	if ( pVars->pane == &leftPane )  { CloseHandle( leftThread );  leftThread  = NULL; }
	if ( pVars->pane == &rightPane ) { CloseHandle( rightThread ); rightThread = NULL; }

	pVars->pane->SetSearching( false );

	pVars->pane->Updated     = true;

	pVars->pane->Update();

	delete pVars;

	return 0;
}

void DoExternalDrop( HWND hDroppedWindow, void *pPaths )
{
	std::vector<std::wstring> *Paths = (std::vector<std::wstring> *) pPaths;

	/* Prepare some vars for the AppAction */
	AppAction action;

	CFileViewer *pane = &leftPane;

	if ( hDroppedWindow == rightPane.hWnd )
	{
		pane = &rightPane;
	}

	action.Action = AA_COPY;
	action.hWnd   = hMainWnd;
	action.Pane   = nullptr; /* Source pane, which does not exist - FileOps will ignore this */
	action.pData  = pane;
	
	/* Validate the selection - all the paths should start the same */
	std::vector<std::wstring>::iterator iPath;

	std::wstring root = L"";

	std::vector<std::wstring> Trailers;

	for ( iPath = Paths->begin(); iPath != Paths->end(); iPath++ )
	{
		if ( iPath->length() == 3 )
		{
			/* Someone tried to drop a drive on us - no way josè */
			MessageBox( hMainWnd, L"Can't drop a whole drive onto NUTS.", L"NUTS", MB_ICONEXCLAMATION | MB_OK );

			delete pPaths;

			return;
		}

		if ( iPath->substr(0, 2) == L"\\\\" )
		{
			/* UNC paths? nope nope noep nope */
			MessageBox( hMainWnd, L"NUTS received an unsupported UNC path. Try dropping from a mapped network drive.", L"NUTS", MB_ICONEXCLAMATION | MB_OK );

			delete pPaths;

			return;
		}

		size_t p = iPath->find_last_of( L'\\' );

		if ( ( p == std::wstring::npos ) || ( p == iPath->length() - 1 ) )
		{
			/* Uh.... */
			MessageBox( hMainWnd, L"NUTS received an unsupported path.", L"NUTS", MB_ICONEXCLAMATION | MB_OK );

			delete pPaths;

			return;
		}

		std::wstring prefix  = iPath->substr( 0, p );
		std::wstring trailer = iPath->substr( p + 1 );

		if ( prefix != root )
		{
			if ( root == L"" )
			{
				root = prefix;
			}
			else
			{
				MessageBox( hMainWnd, L"NUTS received multiple paths from different locations which is not supported.", L"NUTS", MB_ICONEXCLAMATION | MB_OK );

				delete pPaths;

				return;
			}
		}

		Trailers.push_back( trailer );
	}

	/* Delete this now as we don't want to leak memory */
	delete pPaths;

	/* Create a Windows File System object to represent the source */
	WindowsFileSystem *pWFS = new WindowsFileSystem( root );

	action.FS = pWFS;

	/* Search the directory for the files we're looking for. We don't need case matching as the right case
	   should already have been provided */
	std::vector<std::wstring>::iterator iDir;

	DWORD iIndex = 0;

	for ( iDir = pWFS->pWindowsDirectory->WindowsFiles.begin(); iDir != pWFS->pWindowsDirectory->WindowsFiles.end(); iDir++ )
	{
		for ( iPath = Trailers.begin(); iPath != Trailers.end(); iPath++ )
		{
			if ( *iDir == *iPath )
			{
				action.Selection.push_back( pWFS->pDirectory->Files[ iIndex ] );
			}
		}

		iIndex++;
	}

	/* And finally, let this be AppAction's problem */
	QueueAction( action );

	/* This is a special action to ask AppAction to delete the FS object after the copy action is done */
	action.Action = AA_DELETE_FS;

	QueueAction( action );
}

void DoTitleStrings( std::vector<FileSystem *> *pStack, CFileViewer *pPane, int PanelIndex, DWORD TitleSize );

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_UPDATESTATUS:
		if (wParam == (WPARAM) &leftPane)
		{
			pStatusBar->SetPanelText( PANELID_LEFT_STATUS, FSPlugins.FindFont( leftPane.FS->GetEncoding(), 0 ), (BYTE *) lParam );
		}
		else
		{
			pStatusBar->SetPanelText( PANELID_RIGHT_STATUS, FSPlugins.FindFont( rightPane.FS->GetEncoding(), 1 ), (BYTE *) lParam );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_CREATE:
		{
			leftPane.Create(hWnd, hInst, 0, 480, 640);
			rightPane.Create(hWnd, hInst, 480, 520, 640);

			CreateStatusBar(hWnd);

			TrackMouse();

			hNextClip = SetClipboardViewer( hWnd );

			HMENU hMainMenu = GetMenu( hWnd );

			CheckMenuItem( hMainMenu, ID_OPTIONS_RESOLVEDICONS, (UseResolvedIcons)?MF_CHECKED:MF_UNCHECKED );
			CheckMenuItem( hMainMenu, IDM_HIDESIDECARS, (HideSidecars)?MF_CHECKED:MF_UNCHECKED );
			CheckMenuItem( hMainMenu, IDM_CONFIRM, ((bool)Preference( L"Confirm", true ))?MF_CHECKED:MF_UNCHECKED );
			CheckMenuItem( hMainMenu, IDM_SIDECAR_PATHS, ((bool)Preference( L"SidecarPaths", true ))?MF_CHECKED:MF_UNCHECKED );
			CheckMenuItem( hMainMenu, IDM_SLOWENHANCE, ((bool)Preference( L"SlowEnhance", true ))?MF_CHECKED:MF_UNCHECKED );

			SetTimer( hWnd, 0x5016CE, 5000, NULL );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_IDENTIFYFONT:
		{
			DWORD Panel  = PANELID_LEFT_FONT;
			DWORD SPanel = PANELID_LEFT_STATUS;
			BYTE  PIndex = 0;

			CFileViewer *pane = (CFileViewer *) wParam;

			if ( pane == &rightPane )
			{
				Panel  = PANELID_RIGHT_FONT;
				SPanel = PANELID_RIGHT_STATUS;
				PIndex = 1;
			}

			WCHAR *FontName = FSPlugins.FontName( (DWORD) lParam );

			if ( FontName != nullptr )
			{
				pStatusBar->SetPanelText( Panel, FONTID_PC437, (BYTE *) AString( (WCHAR *) FontName ) );
				pStatusBar->SetPanelFont( SPanel, FSPlugins.FindFont( pane->FS->GetEncoding(), PIndex ) );
			}
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_ROOTFS:
		{
			std::vector<FileSystem *> *pStack = &leftFS;
			CFileViewer *pPane = (CFileViewer *) wParam;
			std::vector<TitleComponent> *pTitleStack = &leftTitles;

			if ( pPane == &rightPane )
			{
				pStack      = &rightFS;
				pTitleStack = &rightTitles;
			}

			/* Remove items from the stack until we get back to the root FS */
			while ( 1 )
			{
				FileSystem *pFS = pStack->back();

				if ( pFS->FSID != FS_Root )
				{
					pStack->pop_back();

					delete pFS;
				}
				else
				{
					break;
				}
			}

			pPane->FS = pStack->front();
			pPane->CurrentFSID = pPane->FS->FSID;
			pPane->SelectionStack.clear();
			pPane->SelectionStack.push_back( -1 );
			pPane->Updated = true;

			ReCalculateTitleStack( pStack, pTitleStack, pPane );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:

		if ( ( wmId >= EXTRA_MENU_BASE ) && ( wmId <= EXTRA_MENU_END ) )
		{
			if ( GlobalCommandMap.find( wmId ) != GlobalCommandMap.end() )
			{
				GlobalCommandResult r = (GlobalCommandResult) FSPlugins.PerformGlobalCommand( hMainWnd, GlobalCommandMap[ wmId ].PUID, GlobalCommandMap[ wmId ].CmdIndex );

				if ( r == GC_ResultRefresh )
				{
					PostMessage( leftPane.hWnd, WM_COMMAND, IDM_REFRESH, 0 );
					PostMessage( rightPane.hWnd, WM_COMMAND, IDM_REFRESH, 0 );
				}

				if ( r == GC_ResultRootRefresh )
				{
					if ( leftPane.FS->FSID == FS_Root )
					{
						PostMessage( leftPane.hWnd, WM_COMMAND, IDM_REFRESH, 0 );
					}

					if ( rightPane.FS->FSID == FS_Root )
					{
						PostMessage( rightPane.hWnd, WM_COMMAND, IDM_REFRESH, 0 );
					}
				}
			}

			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		switch (wmId)
		{
		case IDM_ABOUT:
			DoAboutBox( hMainWnd );

			break;

		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case ID_OPTIONS_RESOLVEDICONS:
			{
				UseResolvedIcons = !UseResolvedIcons;

				Preference( L"UseResolvedIcons" ) = UseResolvedIcons;

				std::vector< FileSystem *>::iterator iStack;

				for ( iStack = leftFS.begin(); iStack != leftFS.end(); iStack++ )
				{
					(*iStack)->UseResolvedIcons = UseResolvedIcons;

					(*iStack)->Refresh();
				}

				leftPane.Updated = true;
				leftPane.Redraw();

				for ( iStack = rightFS.begin(); iStack != rightFS.end(); iStack++ )
				{
					(*iStack)->UseResolvedIcons = UseResolvedIcons;

					(*iStack)->Refresh();
				}

				rightPane.Updated = true;
				rightPane.Redraw();

				HMENU hMainMenu = GetMenu( hWnd );

				CheckMenuItem( hMainMenu, ID_OPTIONS_RESOLVEDICONS, (UseResolvedIcons)?MF_CHECKED:MF_UNCHECKED );
			}
			break;

		case IDM_HIDESIDECARS:
			{
				HideSidecars = !HideSidecars;

				Preference( L"HideSidecars" ) = HideSidecars;

				std::vector< FileSystem *>::iterator iStack;

				for ( iStack = leftFS.begin(); iStack != leftFS.end(); iStack++ )
				{
					(*iStack)->HideSidecars = HideSidecars;

					(*iStack)->Refresh();
				}

				leftPane.Updated = true;
				leftPane.Redraw();

				for ( iStack = rightFS.begin(); iStack != rightFS.end(); iStack++ )
				{
					(*iStack)->HideSidecars = HideSidecars;

					(*iStack)->Refresh();
				}

				rightPane.Updated = true;
				rightPane.Redraw();

				HMENU hMainMenu = GetMenu( hWnd );

				CheckMenuItem( hMainMenu, IDM_HIDESIDECARS, (HideSidecars)?MF_CHECKED:MF_UNCHECKED );
			}
			break;

		case IDM_CONFIRM:
			{
				bool Confirm = Preference( L"Confirm", true );

				Confirm = !Confirm;

				Preference( L"Confirm" ) = Confirm;

				HMENU hMainMenu = GetMenu( hWnd );

				CheckMenuItem( hMainMenu, IDM_CONFIRM, (Confirm)?MF_CHECKED:MF_UNCHECKED );
			}
			break;

		case IDM_SLOWENHANCE:
			{
				bool SlowEnhance = Preference( L"SlowEnhance", true );

				SlowEnhance = !SlowEnhance;

				Preference( L"SlowEnhance" ) = SlowEnhance;

				HMENU hMainMenu = GetMenu( hWnd );

				CheckMenuItem( hMainMenu, IDM_SLOWENHANCE, (SlowEnhance)?MF_CHECKED:MF_UNCHECKED );
			}
			break;

		case IDM_SIDECAR_PATHS:
			{
				bool Respect = Preference( L"SidecarPaths", true );

				Respect = !Respect;

				Preference( L"SidecarPaths" ) = Respect;

				HMENU hMainMenu = GetMenu( hWnd );

				CheckMenuItem( hMainMenu, IDM_SIDECAR_PATHS, (Respect)?MF_CHECKED:MF_UNCHECKED );
			}
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);

		EndPaint(hWnd, &ps);

		break;

	case WM_DESTROY:
		ChangeClipboardChain( hWnd, hNextClip );

		PostQuitMessage(0);
		break;

	case WM_CHANGECBCHAIN:
		{
			if ( wParam != (WPARAM) hNextClip )
			{
				::SendMessage( hNextClip, message, wParam, lParam );
			}
			else
			{
				if ( hNextClip != NULL )
				{
					hNextClip = (HWND) lParam;
				}
			}
		}
		break;

	case WM_DRAWCLIPBOARD:
		{
			OutputDebugStringA( "Clipboard change\n" );

			ClipStamp = time(NULL) - 3; // Fudge so that our internal clipboard has priority

			::SendMessage( hNextClip, message, wParam, lParam );
		}
		break;

	case WM_DOENTERAS:
		{
			CFileViewer *Pane = &leftPane;
			std::vector<FileSystem *> *pStack = &leftFS;
			std::vector<TitleComponent> *pTitleStack = &leftTitles;

			if ( wParam == (WPARAM) rightPane.hWnd )
			{
				Pane = &rightPane;
				pStack = &rightFS;
				pTitleStack = &rightTitles;
			}

			DoEnterAs( pStack, Pane, Pane->FS, pTitleStack, lParam );
		}
		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_ENTERICON:
		{
			CFileViewer *Pane = &leftPane;
			std::vector<FileSystem *> *pStack = &leftFS;
			std::vector<TitleComponent> *pTitleStack = &leftTitles;

			OutputDebugString(L"Double click\n");

			if (wParam == (WPARAM) rightPane.hWnd)
			{
				Pane = &rightPane;
				pStack = &rightFS;
				pTitleStack = &rightTitles;
			}

			DoEnter(Pane, pStack, pTitleStack, lParam);
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_COPYOBJECT:
		{
			CFileViewer *pane   = &leftPane;
			CFileViewer *target = &rightPane;

			if ( wParam == (WPARAM) rightPane.hWnd )
			{
				pane   = &rightPane;
				target = &leftPane;
			}

			AppAction Action;

			Action.Action = AA_COPY;
			Action.FS     = pane->FS;
			Action.hWnd   = pane->hWnd;
			Action.Pane   = pane;
			Action.pData  = (void *) target;

			Action.Selection = pane->GetSelection();

			QueueAction( Action );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_INSTALLOBJECT:
		{
			CFileViewer *pane   = &leftPane;
			CFileViewer *target = &rightPane;

			if ( wParam == (WPARAM) rightPane.hWnd )
			{
				pane   = &rightPane;
				target = &leftPane;
			}

			if ( ! (target->FS->Flags & FSF_Supports_Dirs ) )
			{
				MessageBox( pane->hWnd, L"The target file system does not support directories. The 'Install' function cannot be used.", L"NUTS", MB_ICONEXCLAMATION | MB_OK );

				break;
			}

			AppAction Action;

			Action.Action = AA_INSTALL;
			Action.FS     = pane->FS;
			Action.hWnd   = pane->hWnd;
			Action.Pane   = pane;
			Action.pData  = (void *) target;

			Action.Selection = pane->GetSelection();

			QueueAction( Action );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_GOTOPARENT:
		if (wParam == (WPARAM) leftPane.hWnd)
			DoParent( &leftPane );
		else if (wParam == (WPARAM) rightPane.hWnd)
			DoParent( &rightPane );

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_SIZE:
		DoResizeWindow(hWnd);

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_FVSTARTDRAG:
		if ( DragSource == NULL )
		{
			leftPane.StartDragging();
			rightPane.StartDragging();

			char smeg[256];
			sprintf(smeg, "Start drag from %08X, left is %08X, right is %08X\n", wParam, leftPane.hWnd, rightPane.hWnd );
			OutputDebugStringA( smeg );

			SetCursor(LoadCursor(hInst, MAKEINTRESOURCE(IDI_SINGLEFILE)));

			DragSource = (HWND) wParam;
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_MOUSEMOVE:
		TrackMouse();
		break;

	case WM_NCMOUSELEAVE:
	case WM_MOUSELEAVE:
		Tracking = false;
		OutputDebugStringA( "Left!\n" );
	case WM_FVENDDRAG:
		leftPane.EndDragging();
		rightPane.EndDragging();

		char smeg[256];
		sprintf(smeg, "End drag from %08X, left is %08X, right is %08X\n", wParam, leftPane.hWnd, rightPane.hWnd );
		OutputDebugStringA( smeg );

		SetCursor(LoadCursor(NULL, IDC_ARROW));

		/* Post a copy object to ourselves to trigger the action of dragging */
		if ( message == WM_FVENDDRAG )
		{
			if ( wParam != (WPARAM) DragSource )
			{
				::PostMessage( hWnd, WM_COPYOBJECT, (WPARAM) DragSource, 0 );
			}
		}

		DragSource = NULL;

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_SETDRAGTYPE:
		leftPane.SetDragType(wParam);
		rightPane.SetDragType(wParam);

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_FV_FOCUS:
		FocusPane = (HWND) wParam;
		return 0;

	case WM_ACTIVATEAPP:
	case WM_ACTIVATE:
		{
			BOOL fActivate = (BOOL) wParam;

			if ( fActivate )
			{
				char a[256];
				sprintf(a, "Activating window %08X\n", FocusPane );
				OutputDebugStringA( a );
				SetFocus( FocusPane );
			}
		}
		return 0;

	case WM_TIMER:
		if ( wParam == (WPARAM) 0x5016CE )
		{
			pCollector->ReleaseSources();
		}

		{
			OutputDebugStringW( L"Last error: " );
			OutputDebugStringW( pGlobalError->GlobalString.c_str() );
			OutputDebugStringW( L"\n" );
		}
		return 0;

	case WM_EXTERNALDROP:
		{
			DoExternalDrop( (HWND) wParam, (void *) lParam );
		}
		return 0;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
