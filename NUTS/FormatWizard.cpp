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

FileSystem	*pFormatter = NULL;
DataSource  *pSource    = NULL;

DWORD   Format_FT = 0;
HANDLE	hFormatEvent;
HANDLE	hFormatThread;
HWND	hFormatWindow;
bool    Formatting = false;

FSIdentifier FormatFSID = FS_Null;

FontBitmap *srcName = nullptr;

static DWORD dwthreadid;

std::vector<DWORD> AvailableFSIDs;

static FormatList   Formats;
static DWORD        Encoding;
static FormatDesc   ChosenFS;
static bool         FormatSet;

static NUTSProviderList Providers;
static std::vector<FormatDesc> FSList;

BOOL EN1,EN2,EN3,EN4;

BYTE RawDrive;

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
	if ( pSource->Flags & DS_SupportsLLF )
	{
		if ( pSource->Flags & DS_AlwaysLLF )
		{
			Button_SetCheck( GetDlgItem( hwndDlg, IDC_LLF ), BST_CHECKED );
			EnableWindow( GetDlgItem( hwndDlg, IDC_LLF ),    FALSE );
			EN1 = FALSE;
		}
		else
		{
			Button_SetCheck( GetDlgItem( hwndDlg, IDC_LLF ), BST_UNCHECKED );
			EnableWindow( GetDlgItem( hwndDlg, IDC_LLF ),    TRUE );
			EN1 = TRUE;
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

	EN2 = TRUE;
	EN3 = TRUE;

	if ( pSource->Flags & DS_SupportsTruncate )
	{
		EnableWindow( GetDlgItem( hwndDlg, IDC_TRUNCATE ), TRUE );

		EN4 = TRUE;
	}
	else
	{
		EnableWindow( GetDlgItem( hwndDlg, IDC_TRUNCATE ), FALSE );

		EN4 = FALSE;
	}

	EnableWindow( GetDlgItem(hwndDlg, IDC_FORMAT_START), TRUE );
}

void ResetOptions( HWND hwndDlg )
{
	EnableWindow( GetDlgItem( hwndDlg, IDC_LLF ), EN1 );
	EnableWindow( GetDlgItem( hwndDlg, IDC_BLANKSECTS  ), EN2 );
	EnableWindow( GetDlgItem( hwndDlg, IDC_FINITIALISE  ), EN3 );
	EnableWindow( GetDlgItem( hwndDlg, IDC_TRUNCATE ), EN4 );
}

unsigned int __stdcall FormatThread(void *param) {
	if (!pFormatter) {
		PostMessage(hFormatWindow, WM_FORMATPROGRESS, 200, 0);

		return 0;
	}

	if ( Button_GetCheck( GetDlgItem( hFormatWindow, IDC_LLF ) ) == BST_CHECKED ) { Format_FT |= FTF_LLF; }
	if ( Button_GetCheck( GetDlgItem( hFormatWindow, IDC_BLANKSECTS ) ) == BST_CHECKED ) { Format_FT |= FTF_Blank; }
	if ( Button_GetCheck( GetDlgItem( hFormatWindow, IDC_FINITIALISE ) ) == BST_CHECKED ) { Format_FT |= FTF_Initialise; }
	if ( Button_GetCheck( GetDlgItem( hFormatWindow, IDC_TRUNCATE ) ) == BST_CHECKED ) { Format_FT |= FTF_Truncate; }

	unsigned int Result = 0xFF;

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

	ResetOptions( hFormatWindow );

	return Result;
}

INT_PTR CALLBACK FormatSelectWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			{
				::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_PROVIDER ), LB_RESETCONTENT, 0, 0 );
				::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_FORMAT ), LB_RESETCONTENT, 0, 0 );

				Providers = FSPlugins.GetProviders();

				for ( NUTSProvider_iter iProvider = Providers.begin(); iProvider != Providers.end(); iProvider++ )
				{
					::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_PROVIDER ), LB_ADDSTRING, 0, (LPARAM) iProvider->FriendlyName.c_str() );
				}

				FormatSet = false;
			}

			break;

		case WM_COMMAND:
			{
				switch ( LOWORD( wParam ) )
				{
				case IDC_WIZ_PROVIDER:
					{
						if ( HIWORD( wParam ) == LBN_SELCHANGE )
						{
							::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_FORMAT ), LB_RESETCONTENT, 0, 0 );

							WORD Index = (WORD) ::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_PROVIDER ), LB_GETCURSEL, 0, 0 );

							if ( Index != 0xFFFF )
							{
								FSList.clear();

								Formats = FSPlugins.GetFormats( Providers[ Index ].ProviderID );

								for ( FormatDesc_iter iFormat = Formats.begin(); iFormat != Formats.end(); iFormat++ )
								{
									// Need to filter this list according to where we're looking.
									if ( pSource->Flags & DS_RawDevice )
									{
										if ( !( iFormat->Flags & FSF_Formats_Raw ) )
										{
											continue;
										}
									}

									if ( !( iFormat->Flags & FSF_Formats_Image ) )
									{
										continue;
									}
									
									::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_FORMAT ), LB_ADDSTRING, 0, (LPARAM) iFormat->Format.c_str() );

									FSList.push_back( *iFormat );
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
							}
						}
					}
					break;

				case IDCANCEL:
					FormatSet = false;

					EndDialog( hwndDlg, 0 );
					
					break;

				case IDOK:
					FormatSet = true;

					EndDialog( hwndDlg, 0 );

					break;
				}

				return FALSE;
			}
			break;
	}

	return FALSE;
}

void UpdateFS()
{
	FileSystem *pFS = (FileSystem *) CurrentAction.FS;

	pFS->pDirectory->ReadDirectory();

	CFileViewer *pPane = (CFileViewer *) CurrentAction.Pane;

	pPane->Update();
}

INT_PTR CALLBACK FormatProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HFONT hFont = NULL;
	static WCHAR title[512];

	switch (uMsg) {
		case WM_CTLCOLORSTATIC:
			{
				if ((HWND) lParam == GetDlgItem(hwndDlg, IDC_FORMAT_TITLE)) {
					HDC	hDC	= (HDC) wParam;

					SetBkColor(hDC, 0xFFFFFF);

					return (INT_PTR) GetStockObject(WHITE_BRUSH);
				}
			}

			break;

		case WM_COMMAND:
			if ( (LOWORD(wParam) == IDC_SELECT_FORMAT ) && (HIWORD(wParam) == BN_CLICKED ) )
			{
				DialogBox( hInst, MAKEINTRESOURCE( IDD_WIZ_FORMAT ), hwndDlg, FormatSelectWindowProc );

				if ( FormatSet )
				{
					SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_PROVIDER), WM_SETTEXT, 0, (LPARAM) FSPlugins.ProviderName( ChosenFS.FSID ).c_str() );
					SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_FORMAT), WM_SETTEXT, 0, (LPARAM) FSPlugins.FSName( ChosenFS.FSID ).c_str() );

					SetOptions( hwndDlg, ChosenFS.Flags );
				}

				return 0;
			}

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

			if ((LOWORD(wParam) == IDC_FORMAT_CANCEL) && (HIWORD(wParam) == BN_CLICKED)) {
				if ( Formatting == false ) {
					//	Not started the format yet, so just cancel the dialog.
					if ( pFormatter != nullptr )
					{
						delete pFormatter;

						pFormatter = NULL;
					}

					UpdateFS();

					EndDialog(hwndDlg, 0);

					if ( pSource != nullptr )
					{
						DS_RELEASE( pSource );
					}

					return TRUE;
				} else {
					SuspendThread( hFormatThread );

					if ( MessageBox( hwndDlg,
						L"Are you sure you want to cancel the format? The drive or image may be unreadable if you cancel before the format is completed.",
						L"NUTS Formatter", MB_ICONEXCLAMATION | MB_YESNO ) == IDYES )
					{
						pFormatter->CancelFormat();

						::SendMessage( GetDlgItem( hwndDlg, IDC_FORMAT_CANCEL), WM_SETTEXT, 0, (LPARAM) L"Close" );

						ResetOptions( hwndDlg );
					}

					ResumeThread( hFormatThread );
				}
			}

			if ((LOWORD(wParam) == IDC_FORMAT_START) && (HIWORD(wParam) == BN_CLICKED)) {
				//	Check the user is really OK with this...
				if (MessageBox(hwndDlg,
					L"Formatting this device or image will completely erase its contents and replace them "
					L"with a blank filing system.\n\n"
					L"This operation can not be undone in any way whatsoever.\n\n"
					L"Are you sure you wish to continue formatting?",
					L"NUTS Formatter", MB_ICONEXCLAMATION | MB_YESNO) == IDNO) {
						break;
				} else {
					::SendMessage( GetDlgItem( hwndDlg, IDC_FORMAT_CANCEL), WM_SETTEXT, 0, (LPARAM) L"Cancel" );

					// PUID here is both plugin and provider ID.
					pFormatter = FSPlugins.LoadFS( ChosenFS.FSID, pSource );

					if ( pFormatter == nullptr )
					{
						MessageBox( hwndDlg, L"Could not start file system handler for selected format.", L"NUTS", MB_ICONEXCLAMATION | MB_OK );

						break;
					}

					if (pFormatter->Format_PreCheck( Format_FT, hwndDlg ) != 0 )
					{
						break;
					}

					hFormatWindow	= hwndDlg;
					hFormatEvent	= CreateEvent(NULL, TRUE, FALSE, NULL);

					EnableWindow( GetDlgItem( hwndDlg, IDC_LLF ),          FALSE );
					EnableWindow( GetDlgItem( hwndDlg, IDC_BLANKSECTS  ),  FALSE );
					EnableWindow( GetDlgItem( hwndDlg, IDC_FINITIALISE  ), FALSE );
					EnableWindow( GetDlgItem( hwndDlg, IDC_TRUNCATE ),     FALSE );

					hFormatThread	= (HANDLE) _beginthreadex(NULL, NULL, FormatThread, NULL, NULL, (unsigned int *) &dwthreadid);

					Formatting = true;
				}
			}

			break;

		case WM_FORMATPROGRESS:
			SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_PROGRESS), PBM_SETPOS, wParam, 0 );
			SendMessage(GetDlgItem(hwndDlg, IDC_PROGRESS_TEXT),   WM_SETTEXT, 0,      lParam );

			if ( wParam == 100 )
			{
				::SendMessage( GetDlgItem( hwndDlg, IDC_FORMAT_CANCEL), WM_SETTEXT, 0, (LPARAM) L"Close" );
			}

			break;

		case WM_PAINT:
			if ( srcName != nullptr )
			{
				HDC hDC = GetDC( hwndDlg );

				RECT r;

				GetClientRect( hwndDlg, &r );

				r.top += 24;
				r.bottom = r.top + 24;

				FillRect( hDC, &r, (HBRUSH) GetStockObject( WHITE_BRUSH ) );

				r.top = r.bottom;

				DrawEdge( hDC, &r, EDGE_SUNKEN, BF_BOTTOM );

				srcName->DrawText( hDC, 12, 28, DT_LEFT | DT_TOP );

				ReleaseDC( hwndDlg, hDC );
			}
			break;

		case WM_INITDIALOG:
			if (!hFont) {
				hFont	= CreateFont(24,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
			}

			{
				FileSystem *pFS = (FileSystem *) CurrentAction.FS;

				pSource = pFS->FileDataSource( CurrentAction.Selection[ 0 ].fileID );

				if ( pSource == nullptr )
				{
					MessageBox( hwndDlg, L"Could not get data source for selected item.", L"NUTS", MB_ICONEXCLAMATION | MB_OK );

					EndDialog( hwndDlg, 0 );
				}

				SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_TITLE), WM_SETFONT, (WPARAM) hFont, 0);

				SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_TITLE), WM_SETTEXT, 0, (LPARAM) L"  Format Device or Image" );
				SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_PROVIDER), WM_SETTEXT, 0, (LPARAM) L"None" );
				SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_FORMAT), WM_SETTEXT, 0, (LPARAM) L"None" );

				EnableWindow( GetDlgItem( hwndDlg, IDC_LLF ),          FALSE );
				EnableWindow( GetDlgItem( hwndDlg, IDC_BLANKSECTS  ),  FALSE );
				EnableWindow( GetDlgItem( hwndDlg, IDC_FINITIALISE  ), FALSE );
				EnableWindow( GetDlgItem( hwndDlg, IDC_TRUNCATE ),     FALSE );

				BYTEString src = BYTEString( CurrentAction.Selection[0].Filename, CurrentAction.Selection[0].Filename.length() );

				if ( CurrentAction.Selection[0].Flags & FF_Extension )
				{
					size_t l = CurrentAction.Selection[0].Filename.length();

					src = BYTEString( CurrentAction.Selection[0].Filename, l + 5 );

					BYTE *pSrc = src;

					pSrc[ l ] = (BYTE) '.';

					rstrncpy( &pSrc[ l + 1 ], CurrentAction.Selection[0].Extension, CurrentAction.Selection[0].Extension.length() );
				}

				if ( srcName != nullptr )
				{
					delete srcName;
				}

				srcName = new FontBitmap( FSPlugins.FindFont( CurrentAction.Selection[0].EncodingID, 0 ), src, src.length(), false, false );

				EnableWindow( GetDlgItem(hwndDlg, IDC_FORMAT_START), FALSE );

				Formatting = false;
				pFormatter = nullptr;

				SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_PROGRESS), PBM_SETRANGE32, 0, 100);
				SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_PROGRESS), PBM_SETPOS, 0, 0);
			}

			return FALSE;

		case WM_CLOSE:
			if ( pSource != nullptr )
			{
				DS_RELEASE( pSource );
			}

			UpdateFS();

			EndDialog(hwndDlg,0);

			return TRUE;

		default:
			break;
	}

	return FALSE;
}

int	Format_Handler(AppAction &Action) {

	if ( Action.Selection.size() != 1U )
	{
		MessageBox( Action.hWnd, L"Can't format multiple selections", L"NUTS Formatter", MB_OK | MB_ICONEXCLAMATION );

		return -1;
	}

	FileSystem *pFS = (FileSystem *) Action.FS;

	CurrentAction = Action;

	bool DismountFS = false;

	RawDrive = 0;

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

	DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_FORMAT), Action.hWnd, FormatProc, NULL);

	if ( DismountFS )
	{
		Dismount( RawDrive );
	}

	if ( pFormatter != nullptr )
	{
		delete pFormatter;
	}

	pFormatter = nullptr;

	return 0;
}

