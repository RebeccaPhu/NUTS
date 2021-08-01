#include "stdafx.h"

#include "TZXConfig.h"
#include "../NUTS/Preference.h"
#include "resource.h"

std::wstring TZXPrefix;
HWND hOpts;

void ReadOptions( void ) 
{
	DWORD ImportOpt  = Preference( TZXPrefix + L"_ImportOption",     (DWORD) 0 );
	DWORD ExportData = Preference( TZXPrefix + L"_ExportDataOption", (DWORD) 0 );
	DWORD ExportText = Preference( TZXPrefix + L"_ExportTextOption", (DWORD) 0 );

	if ( ImportOpt  > 2 ) { ImportOpt  = 0; }
	if ( ExportData > 1 ) { ExportData = 1; }
	if ( ExportText > 1 ) { ExportText = 1; }

	::SendMessage( GetDlgItem( hOpts, IDC_IMPORT_STD   ), BM_SETCHECK, ( ( ImportOpt == 0 ) ? BST_CHECKED : BST_UNCHECKED ), 0 );
	::SendMessage( GetDlgItem( hOpts, IDC_IMPORT_TURBO ), BM_SETCHECK, ( ( ImportOpt == 1 ) ? BST_CHECKED : BST_UNCHECKED ), 0 );
	::SendMessage( GetDlgItem( hOpts, IDC_IMPORT_PURE  ), BM_SETCHECK, ( ( ImportOpt == 2 ) ? BST_CHECKED : BST_UNCHECKED ), 0 );

	::SendMessage( GetDlgItem( hOpts, IDC_EXPORT_RAW_DATA  ), BM_SETCHECK, ( ( ExportData == 0 ) ? BST_CHECKED : BST_UNCHECKED ), 0 );
	::SendMessage( GetDlgItem( hOpts, IDC_EXPORT_DATA_ONLY ), BM_SETCHECK, ( ( ExportData == 1 ) ? BST_CHECKED : BST_UNCHECKED ), 0 );

	::SendMessage( GetDlgItem( hOpts, IDC_EXPORT_RAW_TEXT  ), BM_SETCHECK, ( ( ExportText == 0 ) ? BST_CHECKED : BST_UNCHECKED ), 0 );
	::SendMessage( GetDlgItem( hOpts, IDC_EXPORT_TEXT_ONLY ), BM_SETCHECK, ( ( ExportText == 1 ) ? BST_CHECKED : BST_UNCHECKED ), 0 );
}

void WriteOptions( void ) 
{
	DWORD ImportOpt = 0;

	if ( ::SendMessage( GetDlgItem( hOpts, IDC_IMPORT_STD ),   BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { ImportOpt = 0; }
	if ( ::SendMessage( GetDlgItem( hOpts, IDC_IMPORT_TURBO ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { ImportOpt = 1; }
	if ( ::SendMessage( GetDlgItem( hOpts, IDC_IMPORT_PURE ),  BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { ImportOpt = 2; }

	DWORD ExportData = 0;

	if ( ::SendMessage( GetDlgItem( hOpts, IDC_EXPORT_RAW_DATA ),  BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { ExportData = 0; }
	if ( ::SendMessage( GetDlgItem( hOpts, IDC_EXPORT_DATA_ONLY ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { ExportData = 1; }

	DWORD ExportText = 0;

	if ( ::SendMessage( GetDlgItem( hOpts, IDC_EXPORT_RAW_TEXT ),  BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { ExportText = 0; }
	if ( ::SendMessage( GetDlgItem( hOpts, IDC_EXPORT_TEXT_ONLY ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ) { ExportText = 1; }

	Preference( TZXPrefix + L"_ImportOption",     (DWORD) 0 ) = ImportOpt;
	Preference( TZXPrefix + L"_ExportDataOption", (DWORD) 0 ) = ExportData;
	Preference( TZXPrefix + L"_ExportTextOption", (DWORD) 0 ) = ExportText;
}

INT_PTR CALLBACK ExportDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			hOpts = hDlg;

			ReadOptions();
		}
		break;

	case WM_COMMAND:
		{
			WORD id = LOWORD( wParam );

			if ( id == IDOK )
			{
				WriteOptions();

				EndDialog( hDlg, 0 );
			}

			if ( id == IDCANCEL )
			{
				EndDialog( hDlg, 0 );
			}
		}
		break;
	}

	return FALSE;
}

TZXCONFIG_API int TZXExportOptions( HWND hParent, std::wstring RegPrefix )
{
	TZXPrefix = RegPrefix;

	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_EXPORT ), hParent, ExportDialogProc, NULL );

	return 0;
}