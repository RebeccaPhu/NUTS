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
#include "FileOps.h"
#include "CharMap.h"
#include "License.h"
#include "ISOECC.h"
#include "PortManager.h"

#include <winioctl.h>
#include <process.h>
#include <time.h>

#include <vector>
#include <deque>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                     // current instance
HWND      hActiveWnd   = NULL;
bool      FSChangeLock = false;

TCHAR szTitle[MAX_LOADSTRING];       // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING]; // the main window class name

HWND hMainWnd;	  // Main window handle
HWND hNextClip;   // Next Clipboard Window
HWND hSplashWnd;  // Splash Screen

EncodingStatusBar *pStatusBar;

#define PANELID_LEFT_STATUS  1
#define PANELID_LEFT_FONT    2
#define PANELID_RIGHT_STATUS 3
#define PANELID_RIGHT_FONT   4

bool Tracking = false;
bool UseResolvedIcons = Preference( L"UseResolvedIcons" );
bool HideSidecars     = Preference( L"HideSidecars" );

//	File viewer window classes:
CFileViewer	*leftPane;
CFileViewer *rightPane;

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
BOOL                LoadMainWindow( HINSTANCE hInstance );
BOOL                ShowMainWindow( HINSTANCE hInstance );

HWND   FocusPane   = NULL;
HWND   DragSource  = NULL;

std::map< DWORD, RootCommand> RootCommandMap;

DataSourceCollector *pCollector = new DataSourceCollector();

/* Action Thread */
CRITICAL_SECTION FSActionLock;

std::deque<FSAction> ActionQueue;

HANDLE hActionEvent, hActionShutdown, hFSActionThread;

HWND  hCharmapFocusWnd;

FontIdentifier CharmapFontID;

void InitFSActions( void )
{
	unsigned int	dwthreadid;

	hActionShutdown = CreateEvent(NULL, TRUE, FALSE, NULL);
	hActionEvent    = CreateEvent(NULL, FALSE, FALSE, NULL);

	ActionQueue.clear();

	InitializeCriticalSection( &FSActionLock );

	hFSActionThread	= (HANDLE) _beginthreadex(NULL, NULL, FSActionThread, NULL, NULL, &dwthreadid);
}

void StopFSActions( void )
{
	SetEvent( hActionShutdown );

	if ( WaitForSingleObject( hFSActionThread, 10000 ) == WAIT_TIMEOUT ) {
		TerminateThread( hFSActionThread, 500 );
	}

	CloseHandle( hFSActionThread );
	CloseHandle( hActionShutdown );
	CloseHandle( hActionEvent );

	DeleteCriticalSection( &FSActionLock );

	ActionQueue.clear();
	ActionQueue.shrink_to_fit();
}

void UnloadStacks( void )
{
	while ( leftFS.size() > 0 )
	{
		FileSystem *fs = leftFS.back();

		delete fs;

		leftFS.pop_back();
	}

	while ( rightFS.size() > 0 )
	{
		FileSystem *fs = rightFS.back();

		delete fs;

		rightFS.pop_back();
	}

	leftFS.shrink_to_fit();
	rightFS.shrink_to_fit();

	leftTitles.clear();
	leftTitles.shrink_to_fit();
	rightTitles.clear();
	rightTitles.shrink_to_fit();

	RootCommandMap.clear();

	BitmapCache.Unload();
	ExtReg.UnloadExtensions();
}

LRESULT CALLBACK SplashProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch ( message )
	{
	case WM_PAINT:
		{
			HDC hDC = GetDC( hWnd );

			HDC hSplash = CreateCompatibleDC( hDC );
			HDC hNutsLogo = CreateCompatibleDC( hDC );

			HBITMAP hCanvas = CreateCompatibleBitmap( hDC, 300, 200 );

			HGDIOBJ hOld = SelectObject( hSplash, hCanvas );
			HGDIOBJ hOLDBM = SelectObject( hNutsLogo, LoadBitmap( hInst, MAKEINTRESOURCE( IDB_NUTSACORN ) ) );

			HFONT h1Font = CreateFont(52,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Times New Roman"));

			HFONT h2Font = CreateFont(16,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Arial"));

			HFONT h3Font = CreateFont(12,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Arial"));

			RECT r; GetClientRect( hWnd, &r );

			FillRect( hSplash, &r, (HBRUSH) GetStockObject( WHITE_BRUSH ) );

			SetStretchBltMode( hSplash, 3 );

			StretchBlt( hSplash, 20, 20, 64, 64, hNutsLogo, 0, 0, 64, 64, SRCCOPY );

			RECT r2 = { 110, 25, 280, 80 };

			SelectObject( hSplash, h1Font );

			DrawText( hSplash, L"N.U.T.S.", 8, &r2, DT_CENTER | DT_VCENTER );

			r2.top += 45;
			r2.bottom += 45;

			SelectObject( hSplash, h3Font );

			DrawText( hSplash, L"The Native Universal Transfer System", 36, &r2, DT_CENTER | DT_VCENTER );

			SelectObject( hSplash, h2Font );

			RECT r3 = { 4, 100, 296, 120 };

			DrawText( hSplash, L"Written by Rebecca Gellman - (C) 2021", 37, &r3, DT_CENTER | DT_VCENTER );

			r3.top += 60;
			r3.bottom += 60;

			std::wstring t = FSPlugins.GetSplash();

			if ( t == L"" ) { t= L"Loading Plugins..."; }

			FillRect( hSplash, &r3, (HBRUSH) GetStockObject( WHITE_BRUSH ) );

			DrawText( hSplash, t.c_str(), t.length(), &r3, DT_CENTER | DT_VCENTER );

			BitBlt( hDC, 0, 0, 300, 200, hSplash, 0, 0, SRCCOPY );

			ReleaseDC( hWnd, hDC );

			SelectObject( hSplash, hOld );
			SelectObject( hNutsLogo, hOLDBM );

			DeleteObject( hCanvas );
			DeleteObject( h1Font );
			DeleteObject( h2Font );
			DeleteObject( h3Font );
			DeleteObject( hSplash );
			DeleteObject( hNutsLogo );
		}
		break;

	case WM_ENDSPLASH:
		SetTimer( hSplashWnd, 0x4040, 1000, NULL );
		break;

	case WM_TIMER:
		KillTimer( hSplashWnd, 0x4040 );

		DestroyWindow( hSplashWnd );

		if ( !DoAcceptLicense() )
		{
			exit( 0 );
		}

		ShowMainWindow( hInst );

		break;
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}

void SetupSplashScreen( HINSTANCE hInstance )
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= SplashProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));
	wcex.hCursor		= LoadCursor( NULL, IDC_ARROW );
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= L"NUTSSplashScreen";
	wcex.hIconSm		= NULL; // LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	RegisterClassEx(&wcex);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
//	_CrtSetBreakAlloc( 17209 );

	SetupSplashScreen( hInstance );

	pGlobalError = new NUTSError( 0, L"" );

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MSG msg;
	HACCEL hAccelTable;

	leftPane = new CFileViewer();
	rightPane = new CFileViewer();

	leftPane->PaneIndex  = 0;
	rightPane->PaneIndex = 1;

	leftPane->pTitleStack  = &leftTitles;
	rightPane->pTitleStack = &rightTitles;
	leftPane->pFSStack     = &leftFS;
	rightPane->pFSStack    = &rightFS;

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
	InitFSActions();

	hAccelTable	= LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NUTS));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			if ( ( hActiveWnd == NULL ) || ( !IsDialogMessage( hActiveWnd, &msg ) ) )
			{

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	DestroyWindow( hMainWnd );

	StopAppActions();
	StopFSActions();

	delete leftPane;
	delete rightPane;
	delete pStatusBar;

	UnloadStacks();

	delete pCollector;

	FSPlugins.UnloadPlugins();

	delete pGlobalError;

#ifdef _DEBUG
//	_CrtDumpMemoryLeaks();
#endif

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

void ReCalculateTitleStack( CFileViewer *pPane )
{
	pPane->ReCalculateTitleStack( );
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
	RootCommandSet menus = FSPlugins.GetRootCommands();
	RootCommandSet::iterator iter;

	if ( menus.size() == 0 )
	{
		AppendMenu( hExtraMenu, MF_STRING, (UINT) EXTRA_MENU_BASE, L"None available" );

		EnableMenuItem( hExtraMenu, 0, MF_DISABLED | MF_BYPOSITION );
	}

	UINT index = EXTRA_MENU_BASE;

	for (iter = menus.begin(); iter != menus.end(); iter++ )
	{
		AppendMenu( hExtraMenu, MF_STRING, (UINT) index, iter->Text.c_str() );

		RootCommandMap[ index ] = *iter;
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
	ExtReg.RegisterExtension( L"ISO", FT_MiscImage, FT_CDImage );
}

BOOL LoadMainWindow( HINSTANCE hInstance )
{
	HWND hWnd;

	DWORD WindowWidth  = Preference( L"WindowWidth",  (DWORD) 800 );
	DWORD WindowHeight = Preference( L"WindowHeight", (DWORD) 500 );

	hWnd = CreateWindowEx(
		WS_EX_CONTROLPARENT,
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
		CW_USEDEFAULT, 0, WindowWidth, WindowHeight,
		GetDesktopWindow(), NULL, hInstance, NULL
	);

	if (!hWnd)
		return FALSE;

	hMainWnd	= hWnd;
	CharMap::hMainWnd = hWnd;

	return TRUE;
}



BOOL LoadSplashWindow( HINSTANCE hInstance )
{
	int screenX = GetSystemMetrics( SM_CXSCREEN );
	int screenY = GetSystemMetrics( SM_CYSCREEN );

	int xc = ( screenX / 2 ) - 150;
	int yc = ( screenY / 2 ) - 100;

	hSplashWnd = CreateWindowEx(
		NULL,
		L"NUTSSplashScreen",
		L"Splash Screen",
		WS_DLGFRAME | WS_VISIBLE | WS_POPUP,
		xc, yc, 300, 200,
		GetDesktopWindow(), NULL, hInstance, NULL
	);

	if ( !hSplashWnd )
	{
		return FALSE;
	}

	return TRUE;
}

unsigned int dwPluginThreadID;
HANDLE hPluginThread = NULL;

unsigned int __stdcall _PluginThread(void *param)
{
	FSPlugins.LoadPlugins();

	return 0;
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) { 
	hInst = hInstance; // Store instance handle in our global variable

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

	LoadLibrary( L"RichEd20.dll" );

	BitmapCache.LoadBitmaps();

	SetUpBaseMappings();

	// LUT init for ISO raw sectors
	eccedc_init();
	
	if ( !LoadSplashWindow( hInstance ) )
	{
		return FALSE;
	}

	hPluginThread = (HANDLE) _beginthreadex(NULL, NULL, _PluginThread, NULL, NULL, &dwPluginThreadID);

	return TRUE;
}

BOOL ShowMainWindow( HINSTANCE hInstance )
{
	if ( hPluginThread != NULL )
	{
		CloseHandle( hPluginThread );
	}

	if ( !LoadMainWindow( hInstance ) )
	{
		return FALSE;
	}

	ConfigureExtrasMenu();

	leftPane->FS  = new RootFileSystem(); 
	rightPane->FS = new RootFileSystem(); 

	leftFS.push_back( leftPane->FS );
	rightFS.push_back( rightPane->FS );

	leftPane->Update();
	rightPane->Update();

	ReCalculateTitleStack( leftPane );
	ReCalculateTitleStack( rightPane );

	ShowWindow( hMainWnd, SW_SHOW );

	UpdateWindow(hMainWnd);

	SetFocus( leftPane->hWnd );

	FocusPane = leftPane->hWnd;

	SetForegroundWindow( hMainWnd );
	SetActiveWindow( hMainWnd );
	SetWindowPos( hMainWnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE );

	return TRUE;
}

unsigned int DoParent( FSAction *pVars )
{
	pVars->pane->SetSearching( true );

	FileSystem *FS = pVars->pane->FS;

	if ( FS->IsRoot() ) {
		delete FS;

		pVars->pStack->pop_back();

		FS = pVars->pStack->back();

		pVars->pane->FS       = FS;
		pVars->pane->FS->FSID = FS->FSID;
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

	ReCalculateTitleStack( pVars->pane );

	pVars->pane->SetSearching( false );
	pVars->pane->Updated = true;
	pVars->pane->Update();

	return 0;
}

unsigned int DoEnter( FSAction *pVars )
{
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

			ReCalculateTitleStack( pVars->pane );
		}

		pVars->pane->SetSearching( false );
		pVars->pane->Updated	= true;
		pVars->pane->Update();

		return 0;
	}

	if (
		( pCurrentFS->pDirectory->Files[pVars->EnterIndex].Type != FT_MiscImage ) &&
		( pCurrentFS->pDirectory->Files[pVars->EnterIndex].Type != FT_Archive ) )
	{
		pVars->pane->SetSearching( false );

		return 0;
	}

	FileSystem *pNewFS = pCurrentFS->FileFilesystem( pVars->EnterIndex );

	if ( pNewFS != nullptr )
	{
		pNewFS->EnterIndex       = 0xFFFFFFFF;
		pNewFS->pParentFS        = pCurrentFS;
		pNewFS->UseResolvedIcons = UseResolvedIcons;
		pNewFS->HideSidecars     = HideSidecars;
		pNewFS->ProcessFOP       = FSPlugins.GetProcessFOP();
		pNewFS->LoadFOPFS        = FSPlugins.GetFOPLoadFS();

		if ( pNewFS->Init() != NUTS_SUCCESS )
		{
			NUTSError::Report( L"Initialise File System", hMainWnd );

			delete pNewFS;
		}
		else
		{
			pNewFS->hMainWindow = hMainWnd;
			pNewFS->hPaneWindow = pVars->pane->hWnd;

			pVars->pStack->push_back( pNewFS );

			pVars->pane->FS	      = pNewFS;
			pVars->pane->FS->FSID = pNewFS->FSID;

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

			DS_RELEASE( pSource );

			if ( pNewFS != nullptr )
			{
				pNewFS->EnterIndex       = 0xFFFFFFFF;
				pNewFS->pParentFS        = pCurrentFS;
				pNewFS->UseResolvedIcons = UseResolvedIcons;
				pNewFS->HideSidecars     = HideSidecars;
				pNewFS->ProcessFOP       = FSPlugins.GetProcessFOP();
				pNewFS->LoadFOPFS        = FSPlugins.GetFOPLoadFS();

				if ( pNewFS->Init() != NUTS_SUCCESS )
				{
					NUTSError::Report( L"Initialise File System", hMainWnd );

					delete pNewFS;
				}
				else
				{
					pNewFS->IsRaw = false;

					pVars->pStack->push_back( pNewFS );

					pVars->pane->FS	      = pNewFS;
					pVars->pane->FS->FSID = pNewFS->FSID;
					pNewFS->hMainWindow   = hMainWnd;
					pNewFS->hPaneWindow   = pVars->pane->hWnd;

					pVars->pane->SelectionStack.push_back( -1 );
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
					MessageBox(hMainWnd,
						L"The drive or image contains an unrecognised file system.\n\nDo you need to run NUTS as Administrator?",
						L"NUTS FileSystem Probe", MB_OK|MB_ICONSTOP
					);
				}

				pVars->pane->SetSearching( false );

				return NUTSError( 0x00000022, L"Unrecognised file system" );
			}
		}
		else
		{

			pVars->pane->SetSearching( false );

			NUTSError::Report( L"Load Data Source", hMainWnd );

			return 0;
		}
	}

	pCurrentFS->EnterIndex = pVars->EnterIndex;

	ReCalculateTitleStack( pVars->pane );

	pVars->pane->SetSearching( false );
	pVars->pane->Updated = true;
	pVars->pane->Update();

	return 0; 
}

unsigned int DoEnterAs( FSAction *pVars )
{
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
			DS_RELEASE( pSource );

			return 0;
		}
	}

	pVars->pane->SetSearching( true );

	pGlobalError->GlobalCode = NUTS_SUCCESS;

	if ( pVars->FSID != FS_Null )
	{
		FileSystem	*newFS = FSPlugins.LoadFSWithWrappers( pVars->FSID, pSource );

		DS_RELEASE( pSource );

		if (newFS) {
			newFS->EnterIndex       = 0xFFFFFFFF;
			newFS->pParentFS        = pVars->FS;
			newFS->UseResolvedIcons = UseResolvedIcons;
			newFS->HideSidecars     = HideSidecars;
			newFS->hMainWindow      = hMainWnd;
			newFS->hPaneWindow      = pVars->pane->hWnd;
			newFS->ProcessFOP       = FSPlugins.GetProcessFOP();
			newFS->LoadFOPFS        = FSPlugins.GetFOPLoadFS();

			if ( newFS->Init() != NUTS_SUCCESS )
			{
				NUTSError::Report( L"Initialise File System", hMainWnd );
			}

			pVars->pane->FS    = newFS;

			pVars->pStack->push_back( newFS );

			pVars->pane->FS->FSID = newFS->FSID;

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
				MessageBox( hMainWnd, L"Unable to load data source", L"NUTS", MB_ICONERROR | MB_OK );
			}
		}
	}
	else
	{
		DS_RELEASE( pSource );
	}

	ReCalculateTitleStack( pVars->pane );

	pVars->pane->SetSearching( false );

	pVars->pane->Updated     = true;

	pVars->pane->Update();

	return 0;
}

void DoRootFS( FSAction *pVars )
{
	/* Remove items from the stack until we get back to the root FS */
	while ( 1 )
	{
		FileSystem *pFS = pVars->pStack->back();

		if ( pFS->FSID != FS_Root )
		{
			pVars->pStack->pop_back();

			delete pFS;
		}
		else
		{
			break;
		}
	}

	pVars->pane->FS = pVars->pStack->front();
	pVars->pane->SelectionStack.clear();
	pVars->pane->SelectionStack.push_back( -1 );

	pVars->pane->Update();

	ReCalculateTitleStack( pVars->pane );
}

void DoRefresh( FSAction *pVars )
{
	if ( pVars->FS->Refresh() != NUTS_SUCCESS )
	{
		NUTSError::Report( L"Refresh", hMainWnd );
	}
	else
	{				
		pVars->pane->Update();
	}
}


unsigned int __stdcall FSActionThread(void *param)
{
	while ( 1 )
	{
		HANDLE evts[2];

		evts[ 0 ] = hActionEvent;
		evts[ 1 ] = hActionShutdown;

		WaitForMultipleObjects( 2, evts, FALSE, 250 );

		if ( WaitForSingleObject( hActionShutdown, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		FSAction action;
		bool     HasWork = false;

		EnterCriticalSection( &FSActionLock );

		if ( ActionQueue.size() > 0 )
		{
			action = ActionQueue.front();

			ActionQueue.pop_front();

			HasWork = true;
		}

		LeaveCriticalSection( &FSActionLock );

		if ( !HasWork ) { continue; }

		switch ( action.Type )
		{
		case ActionDoBack:
			DoParent( &action );
			break;

		case ActionDoEnter:
			DoEnter( &action );
			break;

		case ActionDoEnterAs:
			DoEnterAs( &action );
			break;

		case ActionDoRoot:
			DoRootFS( &action );
			break;

		case ActionDoRefresh:
			DoRefresh( &action );
			break;

		case ActionDoRename:
			action.pane->RenameFile();
			break;

		case ActionDoNewDir:
			action.pane->NewDirectory();
			break;

		case ActionDoDirType:
			action.pane->SetDirType();
			break;

		case ActionHookInvoke:
			{
				FileSystem *pNewFS = action.pane->DoRootHook();

				if ( pNewFS != nullptr )
				{
					pNewFS->EnterIndex       = 0xFFFFFFFF;
					pNewFS->pParentFS        = action.pane->FS;
					pNewFS->UseResolvedIcons = Preference( L"UseResolvedIcons", false );
					pNewFS->HideSidecars     = Preference( L"HideSidecars",     false );

					if ( pNewFS->Init() != NUTS_SUCCESS )
					{
						NUTSError::Report( L"Initialise File System", hMainWnd );

						delete pNewFS;
					}
					else
					{
						pNewFS->hMainWindow = hMainWnd;
						pNewFS->hPaneWindow = action.pane->hWnd;

						action.pStack->push_back( pNewFS );

						action.pane->FS	      = pNewFS;
						action.pane->FS->FSID = pNewFS->FSID;

						action.pane->SelectionStack.push_back( -1 );
					}
				}
				else
				{
					(void) MessageBox( action.pane->hWnd, L"Root hook failed to present anything meaningful.", L"NUTS", MB_ICONERROR | MB_OK );
				}
			}

			break;

		default:
			break;
		}

		action.pane->UpdateSidePanelFlags();
	}

	return 0;
}

void DoResizeWindow(HWND hWnd) {
	WINDOWPLACEMENT	placement;
	RECT			rect;

	GetWindowPlacement(hWnd, &placement);

	if (placement.showCmd == SW_MINIMIZE)
		return;

	if (placement.showCmd == SW_SHOWMINIMIZED)
		return;

	if (placement.showCmd == SW_FORCEMINIMIZE)
		return;

	GetClientRect(hWnd, &rect);

	int	paneWidth	= (rect.right - rect.left) / 2;
	int	paneHeight	= (rect.bottom - rect.top) - 24;

	paneWidth -= 8;
	paneHeight -= 2;

	SetWindowPos( leftPane->hWnd, NULL, 4, 0, paneWidth, paneHeight, SWP_NOREPOSITION | SWP_NOZORDER );
	SetWindowPos( rightPane->hWnd, NULL, paneWidth + 12, 0, paneWidth, paneHeight, SWP_NOREPOSITION | SWP_NOZORDER );

	leftPane->Resize( paneWidth, paneHeight );
	rightPane->Resize( paneWidth, paneHeight );

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

void DoExternalDrop( HWND hDroppedWindow, void *pPaths )
{
	std::vector<std::wstring> *Paths = (std::vector<std::wstring> *) pPaths;

	/* Prepare some vars for the AppAction */
	AppAction action;

	CFileViewer *pane = leftPane;

	if ( hDroppedWindow == rightPane->hWnd )
	{
		pane = rightPane;
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

	case WM_OPENCHARMAP:
		CharMap::OpenTheMap( hCharmapFocusWnd, CharmapFontID );
		break;

	case WM_UPDATESTATUS:
		if (wParam == (WPARAM) leftPane)
		{
			pStatusBar->SetPanelText( PANELID_LEFT_STATUS, FSPlugins.FindFont( leftPane->FS->GetEncoding(), 0 ), (BYTE *) lParam );
		}
		else
		{
			pStatusBar->SetPanelText( PANELID_RIGHT_STATUS, FSPlugins.FindFont( rightPane->FS->GetEncoding(), 1 ), (BYTE *) lParam );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_CREATE:
		{
			leftPane->Create(hWnd, hInst, 0, 480, 640);
			rightPane->Create(hWnd, hInst, 480, 520, 640);

			CreateStatusBar(hWnd);

			TrackMouse();

			hNextClip = SetClipboardViewer( hWnd );

			HMENU hMainMenu = GetMenu( hWnd );

			CheckMenuItem( hMainMenu, ID_OPTIONS_RESOLVEDICONS, (UseResolvedIcons)?MF_CHECKED:MF_UNCHECKED );
			CheckMenuItem( hMainMenu, IDM_HIDESIDECARS, (HideSidecars)?MF_CHECKED:MF_UNCHECKED );
			CheckMenuItem( hMainMenu, IDM_CONFIRM, ((bool)Preference( L"Confirm", true ))?MF_CHECKED:MF_UNCHECKED );
			CheckMenuItem( hMainMenu, IDM_SIDECAR_PATHS, ((bool)Preference( L"SidecarPaths", true ))?MF_CHECKED:MF_UNCHECKED );
			CheckMenuItem( hMainMenu, IDM_SLOWENHANCE, ((bool)Preference( L"SlowEnhance", true ))?MF_CHECKED:MF_UNCHECKED );

			AlwaysRename = (bool) Preference( L"RenameDirectories", false );
			AlwaysMerge  = (bool) Preference( L"MergeDirectories", false );

			bool SidecarsAnyway = (bool) Preference( L"SidecarsAnyway", false );

			CheckMenuItem( hMainMenu, ID_OPTIONS_RENAMEDIRECTORIES, (AlwaysRename)?MF_CHECKED:MF_UNCHECKED );
			CheckMenuItem( hMainMenu, ID_OPTIONS_MERGEDIRECTORIES, (AlwaysMerge)?MF_CHECKED:MF_UNCHECKED );
			CheckMenuItem( hMainMenu, ID_OPTIONS_SIDECARSANYWAY, (SidecarsAnyway)?MF_CHECKED:MF_UNCHECKED );

			SetTimer( hWnd, 0x5016CE, 5000, NULL );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_IDENTIFYFONT:
		{
			DWORD Panel  = PANELID_LEFT_FONT;
			DWORD SPanel = PANELID_LEFT_STATUS;
			BYTE  PIndex = 0;

			CFileViewer *pane = (CFileViewer *) wParam;

			if ( pane == rightPane )
			{
				Panel  = PANELID_RIGHT_FONT;
				SPanel = PANELID_RIGHT_STATUS;
				PIndex = 1;
			}

			std::wstring FontName = FSPlugins.FontName( FontIdentifier( (WCHAR *) lParam ) );

			if ( FontName != L"" )
			{
				pStatusBar->SetPanelText( Panel, FONTID_PC437, (BYTE *) AString( (WCHAR *) FontName.c_str() ) );
				pStatusBar->SetPanelFont( SPanel, FSPlugins.FindFont( pane->FS->GetEncoding(), PIndex ) );
			}
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_ROOTFS:
		{
			if (wParam == (WPARAM) leftPane->hWnd)
			{
				DoAction( ActionDoRoot, leftPane, &leftFS, &leftTitles, lParam );
			} 
			else if (wParam == (WPARAM) rightPane->hWnd)
			{
				DoAction( ActionDoRoot, rightPane, &rightFS, &rightTitles, lParam );
			}
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_REFRESH_PANE:
		{
			if (wParam == (WPARAM) leftPane->hWnd)
			{
				DoAction( ActionDoRefresh, leftPane, &leftFS, &leftTitles, lParam );
			} 
			else if (wParam == (WPARAM) rightPane->hWnd)
			{
				DoAction( ActionDoRefresh, rightPane, &rightFS, &rightTitles, lParam );
			}
		}
		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:

		if ( ( wmId >= EXTRA_MENU_BASE ) && ( wmId <= EXTRA_MENU_END ) )
		{
			if ( RootCommandMap.find( wmId ) != RootCommandMap.end() )
			{
				RootCommandResult r = (RootCommandResult) FSPlugins.PerformRootCommand( hMainWnd, RootCommandMap[ wmId ].PLID, RootCommandMap[ wmId ].CmdIndex );

				if ( r == GC_ResultRefresh )
				{
					DoAction( ActionDoRefresh, leftPane, &leftFS, &leftTitles, lParam );
					DoAction( ActionDoRefresh, rightPane, &leftFS, &leftTitles, lParam );
				}

				if ( r == GC_ResultRootRefresh )
				{
					if ( leftPane->FS->FSID == FS_Root )
					{
						DoAction( ActionDoRefresh, leftPane, &leftFS, &leftTitles, lParam );
					}

					if ( rightPane->FS->FSID == FS_Root )
					{
						DoAction( ActionDoRefresh, rightPane, &leftFS, &leftTitles, lParam );
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

		case ID_OPTIONS_PORTCONFIGURATION:
			PortConfig.ConfigurePorts();

			FSPlugins.SetPortConfiguration();

			// Refresh any file viewers on the root so that configuration takes effect
			if ( leftPane->FS->FSID == FS_Root )
			{
				DoAction( ActionDoRefresh, leftPane, &leftFS, &leftTitles, lParam );
			}

			if ( rightPane->FS->FSID == FS_Root )
			{
				DoAction( ActionDoRefresh, rightPane, &leftFS, &leftTitles, lParam );
			}

			break;

		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;

		case ID_OPTIONS_SIDECARSANYWAY:
			{
				bool SidecarsAnyway = (bool) Preference( L"SidecarsAnyway", false );

				SidecarsAnyway = !SidecarsAnyway;

				Preference( L"SidecarsAnyway" ) = SidecarsAnyway;

				HMENU hMainMenu = GetMenu( hWnd );

				CheckMenuItem( hMainMenu, ID_OPTIONS_SIDECARSANYWAY, (SidecarsAnyway)?MF_CHECKED:MF_UNCHECKED );
			}
			break;

		case ID_OPTIONS_RENAMEDIRECTORIES:
			if ( AlwaysRename )
			{
				AlwaysRename = false;
				AlwaysMerge  = false;
			}
			else
			{
				AlwaysRename = true;
				AlwaysMerge  = false;
			}

			Preference( L"RenameDirectories" ) = AlwaysRename;
			Preference( L"MergeDirectories" )  = AlwaysMerge;

			{
				HMENU hMainMenu = GetMenu( hWnd );

				CheckMenuItem( hMainMenu, ID_OPTIONS_RENAMEDIRECTORIES, (AlwaysRename)?MF_CHECKED:MF_UNCHECKED );
				CheckMenuItem( hMainMenu, ID_OPTIONS_MERGEDIRECTORIES, (AlwaysMerge)?MF_CHECKED:MF_UNCHECKED );
			}

			break;

		case ID_OPTIONS_MERGEDIRECTORIES:
			if ( AlwaysMerge )
			{
				AlwaysRename = false;
				AlwaysMerge  = false;
			}
			else
			{
				AlwaysRename = false;
				AlwaysMerge  = true;
			}

			Preference( L"RenameDirectories" ) = AlwaysRename;
			Preference( L"MergeDirectories" )  = AlwaysMerge;

			{
				HMENU hMainMenu = GetMenu( hWnd );

				CheckMenuItem( hMainMenu, ID_OPTIONS_RENAMEDIRECTORIES, (AlwaysRename)?MF_CHECKED:MF_UNCHECKED );
				CheckMenuItem( hMainMenu, ID_OPTIONS_MERGEDIRECTORIES, (AlwaysMerge)?MF_CHECKED:MF_UNCHECKED );
			}

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

				leftPane->Updated = true;
				leftPane->Redraw();

				for ( iStack = rightFS.begin(); iStack != rightFS.end(); iStack++ )
				{
					(*iStack)->UseResolvedIcons = UseResolvedIcons;

					(*iStack)->Refresh();
				}

				rightPane->Updated = true;
				rightPane->Redraw();

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

				leftPane->Updated = true;
				leftPane->Redraw();

				for ( iStack = rightFS.begin(); iStack != rightFS.end(); iStack++ )
				{
					(*iStack)->HideSidecars = HideSidecars;

					(*iStack)->Refresh();
				}

				rightPane->Updated = true;
				rightPane->Redraw();

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
			if (wParam == (WPARAM) leftPane->hWnd)
			{
				DoAction( ActionDoEnterAs, leftPane, &leftFS, &leftTitles, 0, FSIdentifier( (WCHAR *) lParam ) );
			} 
			else if (wParam == (WPARAM) rightPane->hWnd)
			{
				DoAction( ActionDoEnterAs, rightPane, &rightFS, &rightTitles, 0, FSIdentifier( (WCHAR *) lParam ) );
			}
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_ENTERICON:
		{
			OutputDebugString(L"Double click\n");

			if (wParam == (WPARAM) leftPane->hWnd)
			{
				DoAction( ActionDoEnter, leftPane, &leftFS, &leftTitles, lParam );
			} 
			else if (wParam == (WPARAM) rightPane->hWnd)
			{
				DoAction( ActionDoEnter, rightPane, &rightFS, &rightTitles, lParam );
			}
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_COPYOBJECT:
		{
			CFileViewer *pane   = leftPane;
			CFileViewer *target = rightPane;

			if ( wParam == (WPARAM) rightPane->hWnd )
			{
				pane   = rightPane;
				target = leftPane;
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
			CFileViewer *pane   = leftPane;
			CFileViewer *target = rightPane;

			if ( wParam == (WPARAM) rightPane->hWnd )
			{
				pane   = rightPane;
				target = leftPane;
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
		if (wParam == (WPARAM) leftPane->hWnd)
		{
			DoAction( ActionDoBack, leftPane, &leftFS, &leftTitles, 0 );
		} 
		else if (wParam == (WPARAM) rightPane->hWnd)
		{
			DoAction( ActionDoBack, rightPane, &rightFS, &rightTitles, 0 );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_RENAME_FILE:
		if (wParam == (WPARAM) leftPane->hWnd)
		{
			DoAction( ActionDoRename, leftPane, &leftFS, &leftTitles, 0 );
		} 
		else if (wParam == (WPARAM) rightPane->hWnd)
		{
			DoAction( ActionDoRename, rightPane, &rightFS, &rightTitles, 0 );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_SETDIRTYPE:
		if (wParam == (WPARAM) leftPane->hWnd)
		{
			DoAction( ActionDoDirType, leftPane, &leftFS, &leftTitles, 0 );
		} 
		else if (wParam == (WPARAM) rightPane->hWnd)
		{
			DoAction( ActionDoDirType, rightPane, &rightFS, &rightTitles, 0 );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_NEW_DIR:
		if (wParam == (WPARAM) leftPane->hWnd)
		{
			DoAction( ActionDoNewDir, leftPane, &leftFS, &leftTitles, 0 );
		} 
		else if (wParam == (WPARAM) rightPane->hWnd)
		{
			DoAction( ActionDoNewDir, rightPane, &rightFS, &rightTitles, 0 );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_ROOTINVOKE:
		if (wParam == (WPARAM) leftPane->hWnd)
		{
			DoAction( ActionHookInvoke, leftPane, &leftFS, &leftTitles, 0 );
		} 
		else if (wParam == (WPARAM) rightPane->hWnd)
		{
			DoAction( ActionHookInvoke, rightPane, &rightFS, &rightTitles, 0 );
		}

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_SIZE:
		DoResizeWindow(hWnd);

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_FVSTARTDRAG:
		if ( DragSource == NULL )
		{
			leftPane->StartDragging();
			rightPane->StartDragging();

			char smeg[256];
			sprintf(smeg, "Start drag from %08X, left is %08X, right is %08X\n", wParam, leftPane->hWnd, rightPane->hWnd );
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
		leftPane->EndDragging();
		rightPane->EndDragging();

		char smeg[256];
		sprintf(smeg, "End drag from %08X, left is %08X, right is %08X\n", wParam, leftPane->hWnd, rightPane->hWnd );
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
		leftPane->SetDragType(wParam);
		rightPane->SetDragType(wParam);

		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_FV_FOCUS:
		FocusPane = (HWND) wParam;
		return 0;

	case WM_ACTIVATEAPP:
	case WM_ACTIVATE:
		{
			if ( hWnd == hMainWnd )
			{
				if ( wParam == 0 )
				{
					hActiveWnd = 0;
				}
				else
				{
					hActiveWnd = hWnd;
				}

				return 0;
			}

//			SetFocus( FocusPane );
		}
		break;

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

void DoAction( ActionType t, CFileViewer *p, std::vector<FileSystem *> *s, std::vector<TitleComponent> *pT, int i, FSIdentifier TargetFSID )
{
	FSAction a;

	a.Type        = t;
	a.pane        = p;
	a.pStack      = s;
	a.pTitleStack = pT;
	a.EnterIndex  = i;
	a.FS          = p->FS;
	a.FSID        = TargetFSID;

	EnterCriticalSection( &FSActionLock );

	ActionQueue.push_back( a );

	LeaveCriticalSection( &FSActionLock );

	SetEvent( hActionEvent );
}
