#include "stdafx.h"

#include "AboutBox.h"
#include "Defs.h"
#include "resource.h"

#include "PluginDescriptor.h"
#include "Plugins.h"

#include <commctrl.h>
#include <Richedit.h>

#include <vector>

HWND pCurrentTab = nullptr;

DWORD CALLBACK EditStreamCallback( DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, PLONG pcb )
{
	HANDLE hFile = (HANDLE)dwCookie;

	return !ReadFile(hFile, lpBuff, cb, (DWORD *)pcb, NULL);
}

BOOL FillRichEditFromFile( HWND hwnd, LPCTSTR pszFile )
{
	BOOL fSuccess = FALSE;
	
	HANDLE hFile = CreateFile(
		pszFile, GENERIC_READ, FILE_SHARE_READ,
		0, OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN, NULL
	);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		EDITSTREAM es = { (DWORD_PTR) hFile, 0, EditStreamCallback };

		if ( SendMessage( hwnd, EM_STREAMIN, SF_RTF, (LPARAM)&es) && es.dwError == 0 )
		{
			fSuccess = TRUE;
		}

		CloseHandle(hFile);
	}

	return fSuccess;
}

INT_PTR CALLBACK NullDlgFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	return FALSE;
}

INT_PTR CALLBACK LicenseFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message )
	{
	case WM_ABOUT_RESIZE:
		{
			RECT r;

			GetClientRect( hDlg, &r );

			SetWindowPos( GetDlgItem( hDlg, IDC_LICENSE ), NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOREPOSITION );

			FillRichEditFromFile( GetDlgItem( hDlg, IDC_LICENSE ), L"License.rtf" );
		}
		break;
	}

	return FALSE;
}

INT_PTR CALLBACK PluginsFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message )
	{
	case WM_ABOUT_RESIZE:
		{
			RECT r;

			GetClientRect( hDlg, &r );

			SetWindowPos( GetDlgItem( hDlg, IDC_PLUGINLIST ), NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOREPOSITION );
		}
		break;

	case WM_INITDIALOG:
		{
			std::vector<PluginDescriptor> Plugins = FSPlugins.GetPluginList();

			std::vector<PluginDescriptor>::iterator i;

			for (i=Plugins.begin(); i!= Plugins.end(); i++ )
			{
				SendMessage( GetDlgItem( hDlg, IDC_PLUGINLIST ), LB_ADDSTRING, NULL, (LPARAM) i->Provider.c_str() );

				for (int f=0; f<i->NumFS; f++ )
				{
					FSDescriptor *pFS = (FSDescriptor *) &(i->FSDescriptors[f]);

					std::wstring tFS = L"     " + pFS->FriendlyName;

					SendMessage( GetDlgItem( hDlg, IDC_PLUGINLIST ), LB_ADDSTRING, NULL, (LPARAM) tFS.c_str() );
				}
			}
		}
		return TRUE;
	}

	return (INT_PTR) FALSE;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		{
			TCITEM tie;

			tie.mask = TCIF_TEXT;

			tie.pszText = L"Icon Credits";
			TabCtrl_InsertItem( GetDlgItem( hDlg, IDC_ABOUTTABS ), 1, &tie );

			tie.pszText = L"LICENSE / COPYRIGHT";
			TabCtrl_InsertItem( GetDlgItem( hDlg, IDC_ABOUTTABS ), 1, &tie );

			tie.pszText = L"Plugins";
			TabCtrl_InsertItem( GetDlgItem( hDlg, IDC_ABOUTTABS ), 1, &tie );

			tie.pszText = L"Thanks";
			TabCtrl_InsertItem( GetDlgItem( hDlg, IDC_ABOUTTABS ), 1, &tie );

			NMHDR nm;

			TabCtrl_SetCurSel( GetDlgItem( hDlg, IDC_ABOUTTABS ), 0 );

			nm.code = TCN_SELCHANGE;

			::SendMessage( hDlg, WM_NOTIFY, 0, (LPARAM) &nm );

			return (INT_PTR)TRUE;
		}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;

	case WM_NOTIFY:
		{
			NMHDR *pnm = (NMHDR *) lParam;

			if ( pnm->code == TCN_SELCHANGE )
			{
				/* New tab please */
				if ( pCurrentTab != nullptr )
				{
					DestroyWindow( pCurrentTab );
				}

				int iSel = TabCtrl_GetCurSel( GetDlgItem( hDlg, IDC_ABOUTTABS ) );

				if ( iSel == 0 )
				{
					HRSRC hRes = FindResource( hInst, MAKEINTRESOURCE(IDD_ICONSDLG), RT_DIALOG );
					HGLOBAL hG = LoadResource( hInst, hRes );

					LPDLGTEMPLATEW dlg = (LPDLGTEMPLATEW) LockResource( hG );

					pCurrentTab = CreateDialogIndirect( hInst, dlg, GetDlgItem( hDlg, IDC_ABOUTTABS ), NullDlgFunc );
				}
				
				if ( iSel == 1 )
				{
					HRSRC hRes = FindResource( hInst, MAKEINTRESOURCE(IDD_THANKS), RT_DIALOG );
					HGLOBAL hG = LoadResource( hInst, hRes );

					LPDLGTEMPLATEW dlg = (LPDLGTEMPLATEW) LockResource( hG );

					pCurrentTab = CreateDialogIndirect( hInst, dlg, GetDlgItem( hDlg, IDC_ABOUTTABS ), NullDlgFunc );
				}
				
				if ( iSel == 2 )
				{
					HRSRC hRes = FindResource( hInst, MAKEINTRESOURCE(IDD_PLUGINS), RT_DIALOG );
					HGLOBAL hG = LoadResource( hInst, hRes );

					LPDLGTEMPLATEW dlg = (LPDLGTEMPLATEW) LockResource( hG );

					pCurrentTab = CreateDialogIndirect( hInst, dlg, GetDlgItem( hDlg, IDC_ABOUTTABS ), PluginsFunc );
				}
				
				if ( iSel == 3 )
				{
					HRSRC hRes = FindResource( hInst, MAKEINTRESOURCE(IDD_LICENSE), RT_DIALOG );
					HGLOBAL hG = LoadResource( hInst, hRes );

					LPDLGTEMPLATEW dlg = (LPDLGTEMPLATEW) LockResource( hG );

					pCurrentTab = CreateDialogIndirect( hInst, dlg, GetDlgItem( hDlg, IDC_ABOUTTABS ), LicenseFunc );
				}
				
				DWORD dwDlgBase = GetDialogBaseUnits();
				int cxMargin = LOWORD(dwDlgBase) / 4; 
				int cyMargin = HIWORD(dwDlgBase) / 8;

				RECT r, tr;

				GetClientRect( GetDlgItem( hDlg, IDC_ABOUTTABS ), &r );

				TabCtrl_GetItemRect( GetDlgItem( hDlg, IDC_ABOUTTABS ), iSel, &tr );

				SetWindowPos( pCurrentTab, NULL,
					r.left + cxMargin,
					r.top + ( tr.bottom - tr.top ) + cyMargin + 1,
					( r.right - r.left ) - ( 2 * cxMargin ) - 2,
					( r.bottom - r.top ) - ( tr.bottom - tr.top ) - ( 2 * cyMargin ) - 1,
					SWP_NOZORDER | SWP_NOREPOSITION
				);

				::PostMessage( pCurrentTab, WM_ABOUT_RESIZE, 0, 0 );
			}
		}
		break;
	}
	return (INT_PTR)FALSE;
}

void DoAboutBox( HWND hWnd )
{
	DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
}