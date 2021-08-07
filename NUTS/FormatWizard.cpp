#include "stdafx.h"

#include "Defs.h"
#include "NUTS.h"
#include "FileSystem.h"
#include "AppAction.h"
#include "Plugins.h"
#include "libfuncs.h"

#include <process.h>
#include <commctrl.h>

FileSystem	*pFormatter = NULL;
FormatType Format_FT    = FormatType_Quick;
HANDLE	hFormatEvent;
HANDLE	hFormatThread;
HWND	hFormatWindow;
DWORD   FormatFSID      = FS_Null;
bool    Formatting      = false;

static DWORD dwthreadid;

std::vector<DWORD> AvailableFSIDs;

unsigned int __stdcall FormatThread(void *param) {
	if (!pFormatter) {
		PostMessage(hFormatWindow, WM_FORMATPROGRESS, 200, 0);

		return 0;
	}

	unsigned int Result = pFormatter->Format_Process( Format_FT, hFormatWindow );

	Formatting = false;

	EnableWindow( GetDlgItem( hFormatWindow, IDC_QUICK_FORMAT ), TRUE );
	EnableWindow( GetDlgItem( hFormatWindow, IDC_FULL_FORMAT  ), TRUE );
	EnableWindow( GetDlgItem( hFormatWindow, IDC_FORMAT_LIST  ), TRUE );
	EnableWindow( GetDlgItem( hFormatWindow, IDC_FORMAT_START ), TRUE );

	return Result;
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
			if ((LOWORD(wParam) == IDC_FORMAT_CANCEL) && (HIWORD(wParam) == BN_CLICKED)) {
				if ( Formatting == false ) {
					//	Not started the format yet, so just cancel the dialog.
					EndDialog(hwndDlg, 0);

					return TRUE;
				} else {
					SuspendThread( hFormatThread );

					if ( MessageBox( hwndDlg,
						L"Are you sure you want to cancel the format? The drive or image may be unreadable if you cancel before the format is completed.",
						L"NUTS Formatter", MB_ICONEXCLAMATION | MB_YESNO ) == IDYES )
					{
						pFormatter->CancelFormat();

						::SendMessage( GetDlgItem( hwndDlg, IDC_FORMAT_CANCEL), WM_SETTEXT, 0, (LPARAM) L"Close" );
					}

					ResumeThread( hFormatThread );
				}
			}

			if (LOWORD(wParam) == IDC_QUICK_FORMAT)
				Format_FT = FormatType_Quick;

			if (LOWORD(wParam) == IDC_FULL_FORMAT)
				Format_FT = FormatType_Full;

			if ((HIWORD(wParam) == LBN_SELCHANGE) && (LOWORD(wParam) == IDC_FORMAT_LIST)) {
				FormatFSID = AvailableFSIDs[
					(DWORD) SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_LIST), LB_GETCURSEL, 0, 0)
				];
			}

			if ((LOWORD(wParam) == IDC_FORMAT_START) && (HIWORD(wParam) == BN_CLICKED)) {
				//	Check the user is really OK with this...
				if (MessageBox(hwndDlg,
					L"Formatting this disk or image will completely erase its contents and replace them "
					L"with a blank filing system.\n\n"
					L"This operation can not be undone in any way whatsoever.\n\n"
					L"Are you sure you wish to continue formatting?",
					L"NUTS Formatter", MB_ICONEXCLAMATION | MB_YESNO) == IDNO) {
						break;
				} else {
					::SendMessage( GetDlgItem( hwndDlg, IDC_FORMAT_CANCEL), WM_SETTEXT, 0, (LPARAM) L"Cancel" );

					int   pos  = SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_LIST), LB_GETCURSEL, 0, 0);
					FormatFSID = (DWORD) SendMessage( GetDlgItem(hwndDlg, IDC_FORMAT_LIST), LB_GETITEMDATA, pos, 0 );

					FileSystem *pFS = (FileSystem *) CurrentAction.FS;

					DataSource *pSource = pFS->FileDataSource( CurrentAction.Selection[ 0 ].fileID );

					if ( pSource == nullptr )
					{
						MessageBox( hwndDlg, L"Could not get data source for selected item.", L"NUTS", MB_ICONEXCLAMATION | MB_OK );

						break;
					}

					pFormatter = FSPlugins.LoadFS( FormatFSID, pSource );

					pSource->Release();

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

					EnableWindow( GetDlgItem( hwndDlg, IDC_QUICK_FORMAT ), FALSE);
					EnableWindow( GetDlgItem( hwndDlg, IDC_FULL_FORMAT  ), FALSE);
					EnableWindow( GetDlgItem( hwndDlg, IDC_FORMAT_LIST  ), FALSE);
					EnableWindow( GetDlgItem( hwndDlg, IDC_FORMAT_START ), FALSE);

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

		case WM_INITDIALOG:
			if (!hFont) {
				hFont	= CreateFont(24,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
			}

			{
				AvailableFSIDs.clear();

				SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_TITLE), WM_SETFONT, (WPARAM) hFont, 0);

				FileSystem	*pFS	= (FileSystem *) CurrentAction.FS;

				wsprintf(title, L"  Format disk or image:\n  %s", UString( (char *) CurrentAction.Selection[0].Filename ) );

				SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_TITLE), WM_SETTEXT, 0, (LPARAM) title);

				HWND	hList	= GetDlgItem(hwndDlg, IDC_FORMAT_LIST);

				SendMessage(GetDlgItem(hwndDlg, IDC_QUICK_FORMAT), BM_SETCHECK, (WPARAM) BST_CHECKED, 0);

				NUTSProviderList Providers = FSPlugins.GetProviders();
				NUTSProvider_iter iProvider;

				for ( iProvider = Providers.begin(); iProvider != Providers.end(); iProvider++ )
				{
					FSDescriptorList FSDescriptors = FSPlugins.GetFilesystems( iProvider->ProviderID );

					for ( FSDescriptor_iter iFS = FSDescriptors.begin(); iFS != FSDescriptors.end(); iFS++ )
					{
						if ( iFS->Flags & FSF_Formats_Image )
						{
							int pos = SendMessage(hList, LB_ADDSTRING, 0, (LPARAM) iFS->FriendlyName.c_str() );

							SendMessage( hList, LB_SETITEMDATA, pos, (LPARAM) iFS->PUID );

							AvailableFSIDs.push_back( iFS->PUID );
						}
						else if ( iFS->Flags & FSF_Formats_Raw )
						{
							int pos = SendMessage(hList, LB_ADDSTRING, 0, (LPARAM) iFS->FriendlyName.c_str() );

							SendMessage( hList, LB_SETITEMDATA, pos, (LPARAM) iFS->PUID );

							AvailableFSIDs.push_back( iFS->PUID );
						}
					}
				}

				if ( AvailableFSIDs.size() > 0 )
				{
					EnableWindow( GetDlgItem(hwndDlg, IDC_FORMAT_START), TRUE );
				}

				SendMessage(hList, LB_SETCURSEL, 0, 0);

				Formatting = false;
				pFormatter = nullptr;
				Format_FT  = FormatType_Quick;

				SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_PROGRESS), PBM_SETRANGE32, 0, 100);
				SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_PROGRESS), PBM_SETPOS, 0, 0);
			}

			return FALSE;

		case WM_CLOSE:
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

	FileSystem	*pFS	= (FileSystem *) Action.FS;

	CurrentAction	= Action;

	if (pFS->FSID == FS_Root) {
		//	A drive is being formatted

		if (!IsRawFS( UString( (char *) Action.Selection[0].Filename ) ) ) {
			WCHAR msg[512];

			wsprintf(msg, L"The volume %S is a recognised Windows volume. If you format this drive it will no longer be recognised by Windows.\n\nAre you sure you want to continue?", Action.Selection[0].Filename );
		
			if (MessageBox( Action.hWnd, msg, L"NUTS Formatter", MB_YESNO|MB_ICONEXCLAMATION) == IDNO)
				return -1;
		}
	}

	DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_FORMAT), Action.hWnd, FormatProc, NULL);

	return 0;
}