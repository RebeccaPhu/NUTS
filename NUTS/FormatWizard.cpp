#include "stdafx.h"

#include "NUTS.h"
#include "FileSystem.h"
#include "AppAction.h"
#include "Plugins.h"
#include "FontBitmap.h"
#include "libfuncs.h"
#include "resource.h"

#include <process.h>
#include <commctrl.h>
#include <WindowsX.h>
#include <WinUser.h>

FileSystem	*pFormatter = NULL;
DataSource  *pSource    = NULL;

DWORD   Format_FT = 0;
HANDLE	hFormatEvent;
HANDLE	hFormatThread;
HWND	hFormatWindow;
bool    Formatting = false;

FSIdentifier FormatFSID = FS_Null;

FontBitmap *srcName = nullptr;
size_t      srcNameLen;

static DWORD dwthreadid;

std::vector<DWORD> AvailableFSIDs;

static FormatList   Formats;
static DWORD        Encoding;
static FormatDesc   ChosenFS;
static bool         FormatSet;
static bool         WrapperSet;

static NUTSProviderList Providers;
static std::vector<FormatDesc> FSList;

BOOL EN1,EN2,EN3,EN4;

BYTE RawDrive;
bool DismountFS;

std::wstring ConcludeTitle;
std::wstring ConcludeMessage;

void Dismount( BYTE Drive )
{
	BYTE path[ 64 ];

	rsprintf( path, "\\\\.\\%c:", Drive );

	HANDLE hDevice = CreateFileA(
		(char *) path, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING, 0, NULL
	);

	DWORD returned;
	BOOL  Result;

	if (hDevice != INVALID_HANDLE_VALUE) {
		Result =  DeviceIoControl( hDevice, (DWORD) FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &returned, NULL );

		CloseHandle( hDevice );
	}
}

void SetOptions( HWND hwndDlg, DWORD FSFlags )
{
	if ( FSFlags & DS_SupportsLLF )
	{
		if ( FSFlags & DS_AlwaysLLF )
		{
			Button_SetCheck( GetDlgItem( hwndDlg, IDC_LLF ), BST_CHECKED );
			EnableWindow( GetDlgItem( hwndDlg, IDC_LLF ),    FALSE );
			EN1 = TRUE;
		}
		else
		{
			Button_SetCheck( GetDlgItem( hwndDlg, IDC_LLF ), EN1?BST_CHECKED:BST_UNCHECKED );
			EnableWindow( GetDlgItem( hwndDlg, IDC_LLF ),    TRUE );
		}
	}
	else
	{
		Button_SetCheck( GetDlgItem( hwndDlg, IDC_LLF ), BST_UNCHECKED );
		EnableWindow( GetDlgItem( hwndDlg, IDC_LLF ),    FALSE );
		EN1 = FALSE;
	}

	EnableWindow( GetDlgItem( hwndDlg, IDC_BLANKSECTS  ),  TRUE );
	EnableWindow( GetDlgItem( hwndDlg, IDC_FINITIALISE  ), TRUE );

	Button_SetCheck( GetDlgItem( hwndDlg, IDC_BLANKSECTS ), EN2?BST_CHECKED:BST_UNCHECKED );
	Button_SetCheck( GetDlgItem( hwndDlg, IDC_FINITIALISE), EN3?BST_CHECKED:BST_UNCHECKED );

	if ( FSFlags & DS_SupportsTruncate )
	{
		EnableWindow( GetDlgItem( hwndDlg, IDC_TRUNCATE ), TRUE );

		Button_SetCheck( GetDlgItem( hwndDlg, IDC_TRUNCATE), EN4?BST_CHECKED:BST_UNCHECKED );
	}
	else
	{
		EnableWindow( GetDlgItem( hwndDlg, IDC_TRUNCATE ), FALSE );

		EN4 = FALSE;
	}
}

unsigned int __stdcall FormatThread(void *param) {
	if ( EN1 ) { Format_FT |= FTF_LLF; }
	if ( EN2 ) { Format_FT |= FTF_Blank; }
	if ( EN3 ) { Format_FT |= FTF_Initialise; }
	if ( EN4 ) { Format_FT |= FTF_Truncate; }

	unsigned int Result = 0xFF;

	if ( DismountFS )
	{
		Dismount( RawDrive );
	}

	if ( pFormatter->pSource->PrepareFormat() == NUTS_SUCCESS )
	{
		if ( RawDrive != 0 )
		{
			Dismount( RawDrive );
		}

		Result = pFormatter->Format_Process( Format_FT, hFormatWindow );

		pFormatter->pSource->CleanupFormat();
	}

	Formatting = false;

	return Result;
}

void UpdateFS()
{
	FileSystem *pFS = (FileSystem *) CurrentAction.FS;

	pFS->pDirectory->ReadDirectory();

	CFileViewer *pPane = (CFileViewer *) CurrentAction.Pane;

	pPane->Update();
}

static HWND  hWizard = NULL;
static HFONT hFont = NULL;

WrapperIdentifier ChosenWrapper = WID_Null;

static INT_PTR CALLBACK Wiz1WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			{
				if ( !hFont ) {
					hFont	= CreateFont( 24,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
				}

				::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_WELCOME ), WM_SETFONT, (WPARAM) hFont, (LPARAM) TRUE );

				::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_NEXT );
			}
			return TRUE;

		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code ==  PSN_SETACTIVE )
				{
					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_NEXT );

					FormatSet = false;
				}

				if ( pNM->code == PSN_QUERYCANCEL )
				{
					ConcludeTitle   = L"Format Wizard Cancelled";
					ConcludeMessage = L"The wizard was cancelled before the process could start. No changes have been made to the data source.";

					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 6, (LPARAM) NULL );

					return TRUE;
				}
			}
			break;

		case WM_COMMAND:
			{
				return FALSE;
			}
			break;

		case WM_PAINT:
		{
			if ( srcName != nullptr )
			{
				HDC hDC = GetDC( hwndDlg );

				RECT r;

				GetClientRect( hwndDlg, &r );

				r.top += 122;
				r.left += 173;
				r.right = r.left + ( 8 * srcNameLen );
				r.bottom = r.top + 16;

				srcName->DrawText( hDC, r.left, r.top, DT_LEFT | DT_TOP );

				ReleaseDC( hwndDlg, hDC );

//				ValidateRect( hwndDlg, &r );
			}
		}
		return TRUE;
	}

	return FALSE;
}

static INT_PTR CALLBACK Wiz2WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch ( uMsg )
	{
		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code ==  PSN_SETACTIVE )
				{
					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK );

					if ( !FormatSet )
					{
						::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_PROVIDER ), LB_RESETCONTENT, 0, 0 );
						::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_FORMAT ), LB_RESETCONTENT, 0, 0 );

						Providers = FSPlugins.GetProviders();

						for ( NUTSProvider_iter iProvider = Providers.begin(); iProvider != Providers.end(); iProvider++ )
						{
							::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_PROVIDER ), LB_ADDSTRING, 0, (LPARAM) iProvider->FriendlyName.c_str() );
						}
					}
				}

				if ( pNM->code == PSN_QUERYCANCEL )
				{
					ConcludeTitle   = L"Format Wizard Cancelled";
					ConcludeMessage = L"The wizard was cancelled before the process could start. No changes have been made to the data source.";

					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 6, (LPARAM) NULL );

					return TRUE;
				}
			}
			break;

		case WM_SHOWWINDOW:
			{
				if ( wParam == TRUE )
				{
					if ( FormatSet )
					{
						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );
					}
					else
					{
						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK );
					}
				}
			}
			return FALSE;

		case WM_COMMAND:
			{
				switch ( LOWORD( wParam ) )
				{
				case IDC_WIZ_PROVIDER:
					{
						if ( HIWORD( wParam ) == LBN_SELCHANGE )
						{
							::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_FORMAT ), LB_RESETCONTENT, 0, 0 );

							::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK );

							FormatSet = false;

							WORD Index = (WORD) ::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_PROVIDER ), LB_GETCURSEL, 0, 0 );

							if ( Index != 0xFFFF )
							{
								FSList.clear();

								Formats = FSPlugins.GetFormats( Providers[ Index ].ProviderID );

								for ( FormatDesc_iter iFormat = Formats.begin(); iFormat != Formats.end(); iFormat++ )
								{
									if ( iFormat->Flags & FSF_Creates_Image )
									{
										::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_FORMAT ), LB_ADDSTRING, 0, (LPARAM) iFormat->Format.c_str() );

										FSList.push_back( *iFormat );
									}
								}
							}
						}
					}
					break;

				case IDC_WIZ_FORMAT:
					{
						if ( HIWORD( wParam ) == LBN_SELCHANGE )
						{
							WORD FSNum = (WORD) ::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_FORMAT ), LB_GETCURSEL, 0, 0 );

							if ( FSNum != 0xFFFF )
							{
								ChosenFS = FSList[ FSNum ];

								FormatSet = true;

								::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) ( PSWIZB_NEXT | PSWIZB_BACK ) );
							}
							else
							{
								::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) ( PSWIZB_BACK ) );
							}
						}
					}
					break;
				}

				return FALSE;
			}
			break;
	}

	return FALSE;
}

static INT_PTR CALLBACK Wiz3WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch ( uMsg )
	{
		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code ==  PSN_SETACTIVE )
				{
					FormatSet = true;

					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );

					WrapperList list = FSPlugins.GetWrappers();

					FileSystem *pFS = FSPlugins.LoadFS( ChosenFS.FSID, pSource );

					std::vector<WrapperIdentifier> recs = pFS->RecommendWrappers();

					delete pFS;

					::SendMessage( GetDlgItem( hwndDlg, IDC_WRAPPERS ), LB_RESETCONTENT, NULL, NULL );

					if ( recs.size() == 0U )
					{
						CheckRadioButton( hwndDlg, IDC_NO_WRAPPER, IDC_SOME_WRAPPER, IDC_NO_WRAPPER );

						SetWindowText( GetDlgItem( hwndDlg, IDC_NO_WRAPPER ), L"No wrapper (recommended)" );
					}
					else
					{
						CheckRadioButton( hwndDlg, IDC_NO_WRAPPER, IDC_SOME_WRAPPER, IDC_SOME_WRAPPER );

						SetWindowText( GetDlgItem( hwndDlg, IDC_NO_WRAPPER ), L"No wrapper" );
					}

					int i = 0;
					int s = -1;
					int c = -1;

					for ( WrapperList::iterator iWrapper = list.begin(); iWrapper != list.end(); iWrapper++ )
					{
						std::wstring WrapperName = iWrapper->FriendlyName;

						for ( std::vector<WrapperIdentifier>::iterator iRec = recs.begin(); iRec != recs.end(); iRec++ )
						{
							if ( *iRec == iWrapper->Identifier )
							{
								WrapperName += L" (Recommended)";

								s = i;
							}
						}

						if ( ( iWrapper->Identifier == ChosenWrapper ) && (WrapperSet) )
						{
							c = i;
						}

						::SendMessage( GetDlgItem( hwndDlg, IDC_WRAPPERS ), LB_ADDSTRING, NULL, (LPARAM) WrapperName.c_str() );

						i++;
					}

					if ( c != -1 )
					{
						::SendMessage( GetDlgItem( hwndDlg, IDC_WRAPPERS ), LB_SETCURSEL, c, NULL );

						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );

						CheckRadioButton( hwndDlg, IDC_NO_WRAPPER, IDC_SOME_WRAPPER, IDC_SOME_WRAPPER );
					}
					else if ( s != -1 )
					{
						::SendMessage( GetDlgItem( hwndDlg, IDC_WRAPPERS ), LB_SETCURSEL, s, NULL );

						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );
					}
				}

				if ( pNM->code == PSN_QUERYCANCEL )
				{
					ConcludeTitle   = L"Format Wizard Cancelled";
					ConcludeMessage = L"The wizard was cancelled before the process could start. No changes have been made to the data source.";

					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 6, (LPARAM) NULL );

					return TRUE;
				}
			}
			break;

		case WM_COMMAND:
			{
				switch ( LOWORD( wParam ) )
				{
				case IDC_WRAPPERS:
					{
						if ( HIWORD( wParam ) == LBN_SELCHANGE )
						{
							WORD Index = (WORD) ::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_PROVIDER ), LB_GETCURSEL, 0, 0 );

							if ( Index != 0xFFFF )
							{
								WrapperList Wrappers = FSPlugins.GetWrappers();
							
								ChosenWrapper = Wrappers[ Index ].Identifier;
							
								CheckRadioButton( hwndDlg, IDC_NO_WRAPPER, IDC_SOME_WRAPPER, IDC_SOME_WRAPPER );

								::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );
							}
						}
					}
					break;

				case IDC_NO_WRAPPER:
					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );
				case IDC_SOME_WRAPPER:
					{
						if ( HIWORD( wParam ) == BN_CLICKED )
						{
							::SendMessage( GetDlgItem( hwndDlg, IDC_WRAPPERS ), LB_SETCURSEL, (WPARAM) -1, 0 );

							ChosenWrapper = WID_Null;
						}
						
						if ( LOWORD( wParam ) == IDC_SOME_WRAPPER )
						{
							::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK );
						}
					}
					break;
				}
			}
			break;
	}

	return FALSE;
}

static INT_PTR CALLBACK Wiz4WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HFONT hFont = nullptr;

	static DataSource *WrapSource = nullptr;

	switch ( uMsg )
	{
		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code == PSN_KILLACTIVE )
				{
					if ( WrapSource != nullptr )
					{
						DS_RELEASE( WrapSource );
					}

					WrapSource = nullptr;
				}

				if ( pNM->code == PSN_SETACTIVE )
				{
					WrapperSet = true;

					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );

					if ( hFont == nullptr ) {
						hFont	= CreateFont(10,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
					}

					::SendMessage( GetDlgItem( hwndDlg, IDC_LLF ), WM_SETFONT, 0, (LPARAM) hFont );
					::SendMessage( GetDlgItem( hwndDlg, IDC_BLANKSECTS ), WM_SETFONT, 0, (LPARAM) hFont );
					::SendMessage( GetDlgItem( hwndDlg, IDC_FINITIALISE ), WM_SETFONT, 0, (LPARAM) hFont );
					::SendMessage( GetDlgItem( hwndDlg, IDC_TRUNCATE ), WM_SETFONT, 0, (LPARAM) hFont );

					EnableWindow( GetDlgItem( hwndDlg, IDC_LLF ),          FALSE );
					EnableWindow( GetDlgItem( hwndDlg, IDC_BLANKSECTS  ),  FALSE );
					EnableWindow( GetDlgItem( hwndDlg, IDC_FINITIALISE  ), FALSE );
					EnableWindow( GetDlgItem( hwndDlg, IDC_TRUNCATE ),     FALSE );

					WrapSource = nullptr;

					if ( ChosenWrapper != WID_Null )
					{
						WrapSource = FSPlugins.LoadWrapper( ChosenWrapper, pSource );

						SetOptions( hwndDlg, WrapSource->Flags );
					}
					else
					{
						SetOptions( hwndDlg, pSource->Flags );
					}
				}

				if ( pNM->code == PSN_QUERYCANCEL )
				{
					ConcludeTitle   = L"Format Wizard Cancelled";
					ConcludeMessage = L"The wizard was cancelled before the process could start. No changes have been made to the data source.";

					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 6, (LPARAM) NULL );

					return TRUE;
				}
			}
			break;

		case WM_COMMAND:
			if ( (LOWORD(wParam) == IDC_BLANKSECTS ) && ( HIWORD(wParam) == BN_CLICKED ) )
			{
				if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_BLANKSECTS ) ) == BST_CHECKED )
				{
					EnableWindow( GetDlgItem( hwndDlg, IDC_TRUNCATE ), FALSE );
				}
				else
				{
					if ( pSource->Flags & DS_SupportsTruncate )
					{
						EnableWindow( GetDlgItem( hwndDlg, IDC_TRUNCATE ), TRUE );
					}
				}
			}
			break;
	}

	return FALSE;
}

static INT_PTR CALLBACK Wiz5WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HFONT hFont = nullptr;

	switch ( uMsg )
	{
		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code ==  PSN_SETACTIVE )
				{
					WrapperSet = true;

					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );

					if ( hFont == nullptr ) {
						hFont	= CreateFont(10,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
					}

					::SendMessage( GetDlgItem( hwndDlg, IDC_FORMATWHAT ), WM_SETFONT, 0, (LPARAM) hFont );
					::SendMessage( GetDlgItem( hwndDlg, IDC_FORMATWRAPPER ), WM_SETFONT, 0, (LPARAM) hFont );
					::SendMessage( GetDlgItem( hwndDlg, IDC_FORMATWHICH ), WM_SETFONT, 0, (LPARAM) hFont );

					::SendMessage( GetDlgItem( hwndDlg, IDC_FORMATWHATTEXT ), WM_SETTEXT, 0, (LPARAM) ChosenFS.Format.c_str() );

					if ( ChosenWrapper == WID_Null )
					{
						::SendMessage( GetDlgItem( hwndDlg, IDC_FORMATWRAPPERTEXT ), WM_SETTEXT, 0, (LPARAM) L"No Wrapper" );
					}
					else
					{
						::SendMessage( GetDlgItem( hwndDlg, IDC_FORMATWRAPPERTEXT ), WM_SETTEXT, 0, (LPARAM) FSPlugins.GetWrapperName( ChosenWrapper ).c_str() );
					}

					std::wstring Which = L"";

					if ( EN1 ) { Which += L"Low-level format the source\n"; }
					if ( EN2 ) { Which += L"Write blank sectors\n"; }
					if ( EN3 ) { Which += L"Initialise the file system structure\n"; }
					if ( EN4 ) { Which += L"Truncate the source\n"; }

					::SendMessage( GetDlgItem( hwndDlg, IDC_FORMATWHICHTEXT ), WM_SETTEXT, 0, (LPARAM) Which.c_str() );
				}

				if ( pNM->code == PSN_WIZNEXT )
				{
					if ( MessageBox( hwndDlg,
						L"Warning! Formatting a data source completely erases its contents, replacing them with blank data. "
						L"This operation CANNOT be undone. This is especially important if you are formatting a drive recognised as a volume "
						L"by Windows. Do not proceed if you are not sure of what you are doing.\n\n"
						L"Do you wish to proceed with the format?",

						L"NUTS Formatter", MB_ICONWARNING | MB_YESNO ) == IDNO )
					{
						ConcludeTitle   = L"Format Wizard Cancelled";
						ConcludeMessage = L"The wizard was cancelled before the process could start. No changes have been made to the data source.";

						SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

						::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 6, (LPARAM) NULL );

						return TRUE;
					}
				}

				if ( pNM->code == PSN_QUERYCANCEL )
				{
					ConcludeTitle   = L"Format Wizard Cancelled";
					ConcludeMessage = L"The wizard was cancelled before the process could start. No changes have been made to the data source.";

					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 6, (LPARAM) NULL );

					return TRUE;
				}
			}
			break;
	}

	return FALSE;
}

static INT_PTR CALLBACK Wiz6WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HFONT hFont = nullptr;

	switch ( uMsg )
	{
		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code ==  PSN_SETACTIVE )
				{
					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, 0 );

					SendMessage(GetDlgItem(hwndDlg, IDC_CREATE_PROGRESS), PBM_SETRANGE32, 0, 100);
					SendMessage(GetDlgItem(hwndDlg, IDC_CREATE_PROGRESS), PBM_SETPOS, 0, 0);

					// Load the source up
					DataSource *pSrc  = pSource;
					DataSource *pWrap = nullptr;

					if ( ChosenWrapper != WID_Null )
					{
						DataSource *pWrap = FSPlugins.LoadWrapper( ChosenWrapper, pSrc );

						if ( pWrap == nullptr )
						{
							NUTSError::Report( L"Loading wrapper for format", hwndDlg );

							SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) MAKEINTRESOURCE( IDD_FWIZ_DIALOG7 ) );

							return TRUE;
						}

						pSrc = pWrap;
					}

					pFormatter = FSPlugins.LoadFS( ChosenFS.FSID, pSrc );

					if ( pWrap != nullptr )
					{
						DS_RELEASE( pWrap ); // FS has this now
					}

					if ( pFormatter == nullptr )
					{
						NUTSError::Report( L"Loading file system for format", hwndDlg );

						SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) MAKEINTRESOURCE( IDD_FWIZ_DIALOG7 ) );

						return TRUE;
					}

					DWORD FormatFlags = 0;

					if ( EN1 ) { FormatFlags |= FTF_LLF; }
					if ( EN2 ) { FormatFlags |= FTF_Blank; }
					if ( EN3 ) { FormatFlags |= FTF_Initialise; }
					if ( EN4 ) { FormatFlags |= FTF_Truncate; }

					if ( pFormatter->Format_PreCheck( FormatFlags, hwndDlg ) != NUTS_SUCCESS )
					{
						SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) MAKEINTRESOURCE( IDD_FWIZ_DIALOG7 ) );

						return TRUE;
					}

					hFormatWindow = hwndDlg;
					hFormatThread = (HANDLE) _beginthreadex(NULL, NULL, FormatThread, NULL, NULL, (unsigned int *) &dwthreadid);
				}

				if ( pNM->code ==  PSN_QUERYCANCEL )
				{
					SuspendThread( hFormatThread );

					if ( MessageBox( hwndDlg,
						L"The format is currently running. If you cancel now the data source will be left in an inconsistent, "
						L"and most likely unusable state.\n\nAre you sure you wish to cancel the format?",
						L"NUTS Formatter", MB_ICONQUESTION | MB_YESNO ) == IDYES )
					{
						pFormatter->CancelFormat();

						ResumeThread( hFormatThread );

						if ( WaitForSingleObject( hFormatThread, 500 ) != WAIT_OBJECT_0 )
						{
							TerminateThread( hFormatThread, 500 );
						}

						ConcludeTitle   = L"Format Wizard Cancelled";
						ConcludeMessage =
							L"The Format Wizard was cancelled during formatting. The data source is most likely in "
							L"an inconsistent state, and should be treated with a high degree of suspicion.";

						SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

						::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 6, (LPARAM) NULL );

						return TRUE;
					}

					ResumeThread( hFormatThread );

					return TRUE;
				}
			}
			break;

		case WM_FORMATPROGRESS:
			{
				int Percent = (int) wParam;
				WCHAR *msg  = (WCHAR *) lParam;

				::SendMessage( GetDlgItem( hwndDlg, IDC_CREATE_OP ), WM_SETTEXT, 0, (LPARAM) msg );
				::SendMessage(GetDlgItem(hwndDlg, IDC_CREATE_PROGRESS), PBM_SETPOS, wParam, 0 );

				if ( Percent == 100 )
				{
					if ( WaitForSingleObject( hFormatThread, 500 ) != WAIT_OBJECT_0 )
					{
						TerminateThread( hFormatThread, 500 );
					}

					/* This is here purely to make the "and I'm spent" moment visible to the user */
					Sleep( 2000 );

					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 6, (LPARAM) NULL );
				}
			}
			break;

		case WM_COMMAND:
			{
				return FALSE;
			}
			break;
	}

	return FALSE;
}

static INT_PTR CALLBACK Wiz7WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HFONT hFont = nullptr;

	switch ( uMsg )
	{
		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code ==  PSN_SETACTIVE )
				{
					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_FINISH );

					if ( !hFont ) {
						hFont	= CreateFont( 24,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
					}

					::SendMessage( GetDlgItem( hwndDlg, IDC_FWIZ_SUCCESS ), WM_SETFONT, (WPARAM) hFont, (LPARAM) TRUE );

					::SendMessage( GetDlgItem( hwndDlg, IDC_FWIZ_SUCCESS ), WM_SETTEXT, 0, (LPARAM) ConcludeTitle.c_str() );
					::SendMessage( GetDlgItem( hwndDlg, IDC_FWIZ_MESSAGE ), WM_SETTEXT, 0, (LPARAM) ConcludeMessage.c_str() );

					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) FALSE );

					UpdateFS();
				}
			}
			break;
	}

	return FALSE;
}

static int CALLBACK WizPageProc( HWND hwndDlg, UINT uMsg, LPARAM lParam )
{
	hWizard = hwndDlg;

	FormatSet = false;
	WrapperSet = false;

	BYTEString src = BYTEString( CurrentAction.Selection[0].Filename, CurrentAction.Selection[0].Filename.length() );

	if ( CurrentAction.Selection[0].Flags & FF_Extension )
	{
		size_t l = CurrentAction.Selection[0].Filename.length();

		src = BYTEString( CurrentAction.Selection[0].Filename, l + 5 );

		BYTE *pSrc = src;

		pSrc[ l ] = (BYTE) '.';

		srcNameLen = src.length();

		rstrncpy( &pSrc[ l + 1 ], CurrentAction.Selection[0].Extension, CurrentAction.Selection[0].Extension.length() );
	}

	if ( srcName != nullptr )
	{
		delete srcName;
	}

	srcName = new FontBitmap( FSPlugins.FindFont( CurrentAction.Selection[0].EncodingID, 0 ), src, src.length(), false, false );

	return 0;
}

int	Format_Handler(AppAction &Action)
{
	if ( Action.Selection.size() != 1U )
	{
		MessageBox( Action.hWnd, L"Can't format multiple selections", L"NUTS Formatter", MB_OK | MB_ICONEXCLAMATION );

		return -1;
	}

	FileSystem *pFS = (FileSystem *) Action.FS;

	CurrentAction = Action;

	RawDrive   = 0;
	DismountFS = false;

	if (pFS->FSID == FS_Root)
	{
		//	A drive is being formatted

		if (!IsRawFS( UString( (char *) Action.Selection[0].Filename ) ) ) {
			WCHAR msg[512];

			wsprintf(msg, L"The volume %S is a recognised Windows volume. If you format this drive it will no longer be recognised by Windows.\n\nAre you sure you want to continue?", Action.Selection[0].Filename );
		
			if (MessageBox( Action.hWnd, msg, L"NUTS Formatter", MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
				return -1;

			DismountFS = true;

			RawDrive = Action.Selection[ 0 ].Filename[ 0 ];
		}
	}

	EN1 = FALSE; // Probably don't want LLF
	EN2 = FALSE; // Blanking takes forever!
	EN3 = TRUE;  // Always initialise the source, but let the user turn this off.
	EN4 = FALSE; // Don't truncate, for purity

	pSource = pFS->FileDataSource( CurrentAction.Selection[ 0 ].fileID );

	ChosenWrapper = WID_Null;

	PROPSHEETPAGE psp[ 7 ];

	PROPSHEETHEADER psh;

	psp[0].dwSize      = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags     = PSP_HIDEHEADER; 
	psp[0].hInstance   = hInst;
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_FWIZ_DIALOG1);
	psp[0].pszIcon     = 0;
	psp[0].pfnDlgProc  = Wiz1WindowProc;
	psp[0].lParam      = 0;
	psp[0].pfnCallback = NULL;
	psp[0].pszTitle    = L"Format Wizard";

	psp[1].dwSize      = sizeof(PROPSHEETPAGE);
	psp[1].dwFlags     = PSP_USEHEADERTITLE | PSP_USEHEADERSUBTITLE | PSP_DEFAULT;
	psp[1].hInstance   = hInst;
	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_FWIZ_DIALOG2);
	psp[1].pszIcon     = 0;
	psp[1].pfnDlgProc  = Wiz2WindowProc;
	psp[1].lParam      = 0;
	psp[1].pfnCallback = NULL;
	psp[1].pszTitle    = L"Format Wizard";
	psp[1].pszHeaderTitle = L"Select system for image";
	psp[1].pszHeaderSubTitle = L"Select the container format for file or device."; 

	
	psp[2].dwSize      = sizeof(PROPSHEETPAGE);
	psp[2].dwFlags     = PSP_USEHEADERTITLE | PSP_USEHEADERSUBTITLE | PSP_DEFAULT;
	psp[2].hInstance   = hInst;
	psp[2].pszTemplate = MAKEINTRESOURCE(IDD_FWIZ_DIALOG3);
	psp[2].pszIcon     = 0;
	psp[2].pfnDlgProc  = Wiz3WindowProc;
	psp[2].lParam      = 0;
	psp[2].pfnCallback = NULL;
	psp[2].pszTitle    = L"Format Wizard";
	psp[2].pszHeaderTitle = L"Select Wrapper";
	psp[2].pszHeaderSubTitle = L"Optionally specific a wrapper to use for the selected format."; 

	psp[3].dwSize      = sizeof(PROPSHEETPAGE);
	psp[3].dwFlags     = PSP_USEHEADERTITLE | PSP_USEHEADERSUBTITLE | PSP_DEFAULT;
	psp[3].hInstance   = hInst;
	psp[3].pszTemplate = MAKEINTRESOURCE(IDD_FWIZ_DIALOG4);
	psp[3].pszIcon     = 0;
	psp[3].pfnDlgProc  = Wiz4WindowProc;
	psp[3].lParam      = 0;
	psp[3].pfnCallback = NULL;
	psp[3].pszTitle    = L"Format Wizard";
	psp[3].pszHeaderTitle = L"Specify format options";
	psp[3].pszHeaderSubTitle = L"Specify options controlling the format processes."; 

	psp[4].dwSize      = sizeof(PROPSHEETPAGE);
	psp[4].dwFlags     = PSP_USEHEADERTITLE | PSP_USEHEADERSUBTITLE | PSP_DEFAULT;
	psp[4].hInstance   = hInst;
	psp[4].pszTemplate = MAKEINTRESOURCE(IDD_FWIZ_DIALOG5);
	psp[4].pszIcon     = 0;
	psp[4].pfnDlgProc  = Wiz5WindowProc;
	psp[4].lParam      = 0;
	psp[4].pfnCallback = NULL;
	psp[4].pszTitle    = L"Format Wizard";
	psp[4].pszHeaderTitle = L"Summary";
	psp[4].pszHeaderSubTitle = L"Summary of the actions to perform."; 

	psp[5].dwSize      = sizeof(PROPSHEETPAGE);
	psp[5].dwFlags     = PSP_USEHEADERTITLE | PSP_USEHEADERSUBTITLE | PSP_DEFAULT;
	psp[5].hInstance   = hInst;
	psp[5].pszTemplate = MAKEINTRESOURCE(IDD_FWIZ_DIALOG6);
	psp[5].pszIcon     = 0;
	psp[5].pfnDlgProc  = Wiz6WindowProc;
	psp[5].lParam      = 0;
	psp[5].pfnCallback = NULL;
	psp[5].pszTitle    = L"Format Wizard";
	psp[5].pszHeaderTitle = L"Formatting";
	psp[5].pszHeaderSubTitle = L"Formatting the source with the specified file system and options."; 

	psp[6].dwSize      = sizeof(PROPSHEETPAGE);
	psp[6].dwFlags     = PSP_HIDEHEADER; 
	psp[6].hInstance   = hInst;
	psp[6].pszTemplate = MAKEINTRESOURCE(IDD_FWIZ_DIALOG7);
	psp[6].pszIcon     = 0;
	psp[6].pfnDlgProc  = Wiz7WindowProc;
	psp[6].lParam      = 0;
	psp[6].pfnCallback = NULL;
	psp[6].pszTitle    = L"Format Wizard";

	psh.dwSize         = sizeof(PROPSHEETHEADER);
	psh.dwFlags        = PSH_WATERMARK | PSH_PROPSHEETPAGE | PSH_USECALLBACK | PSH_WIZARD97 | PSH_HEADER;
	psh.hwndParent     = Action.hWnd;
	psh.hInstance      = hInst;
	psh.pszbmWatermark = MAKEINTRESOURCE( IDB_WIZ_MARK );
	psh.pszCaption     = (LPWSTR) L"Format Wizard";
	psh.nPages         = 7;
	psh.nStartPage     = 0;
	psh.ppsp           = (LPCPROPSHEETPAGE) &psp;
	psh.pfnCallback    = WizPageProc;
	psh.pszbmHeader    = MAKEINTRESOURCE( IDB_WIZBANNER );

	ConcludeTitle   = L"Format Successful";
	ConcludeMessage =
		L"The data source was successfully formatted with the selected file system and wrapper (if selected). "
		L"If the data source is a raw device, it may take a few moments to be recognised as such in Windows.";
    
	PropertySheet( &psh );

	if ( pFormatter != nullptr )
	{
		delete pFormatter;
	}

	pFormatter = nullptr;

	DS_RELEASE( pSource );

	return 0;
}

