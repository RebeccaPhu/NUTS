#include "stdafx.h"

#include "License.h"

#include "resource.h"

#include <Richedit.h>
#include <WindowsX.h>

static DWORD CALLBACK EditStreamCallback( DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, PLONG pcb )
{
	HANDLE hFile = (HANDLE)dwCookie;

	return !ReadFile(hFile, lpBuff, cb, (DWORD *)pcb, NULL);
}

static BOOL FillRichEditFromFile( HWND hwnd, LPCTSTR pszFile )
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


INT_PTR CALLBACK LicenseDlgFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message )
	{
	case WM_INITDIALOG:
		{
			FillRichEditFromFile( GetDlgItem( hDlg, IDC_LICTEXT2 ), L"./License.rtf" );
		}
		return TRUE;

	case WM_COMMAND:
		if ( wParam == IDCANCEL )
		{
			EndDialog( hDlg, IDCANCEL );

			break;
		}

		if ( wParam == IDOK )
		{
			EndDialog( hDlg, IDOK );

			break;
		}

		if ( wParam == IDC_LICACCEPT )
		{
			if ( Button_GetCheck( GetDlgItem( hDlg, IDC_LICACCEPT ) ) == BST_CHECKED )
			{
				EnableWindow( GetDlgItem( hDlg, IDOK ), TRUE );
			}
			else
			{
				EnableWindow( GetDlgItem( hDlg, IDOK ), FALSE );
			}

			break;
		}
	}

	return FALSE;
}


bool DoAcceptLicense()
{
	// If they've already accepted....
	if ( (bool) Preference( L"AcceptLicense", false ) == true )
	{
		return true;
	}

	// Show the dialog
	if ( DialogBoxParam( hInst, MAKEINTRESOURCE( IDD_ACCEPTLICENSE ), GetDesktopWindow(), LicenseDlgFunc, NULL ) == IDOK )
	{
		if ( MessageBox( GetDesktopWindow(),
			L"Do you understand and accept that any and all actions undertaken by this program are your "
			L"responsibility alone, including to abide by all laws pertaining to you regarding potential "
			L"modification of commercial software?", L"NUTS License", MB_ICONEXCLAMATION | MB_YESNO ) == IDYES )
		{
			if ( MessageBox( GetDesktopWindow(),
				L"Do you understand and accept that any and all actions undertaken by this program are your "
				L"responsibility alone, including any actions that cause irreperable damage to your system "
				L"or files?", L"NUTS License", MB_ICONEXCLAMATION | MB_YESNO ) == IDYES )
			{
				Preference( L"AcceptLicense" ) = true;

				MessageBox( GetDesktopWindow(), L"Thank you. You will not be asked these questions again.", L"NUTS License", MB_ICONINFORMATION | MB_OK );
			
				return true;
			}
		}
	}

	return false;
}