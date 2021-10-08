#include "stdafx.h"
#include "ImageWizard.h"
#include "AppAction.h"
#include "resource.h"
#include "Plugins.h"
#include "EncodingEdit.h"
#include "ImageDataSource.h"
#include "FileViewer.h"

#include "libfuncs.h"

#include "Defs.h"

#include <PrSht.h>
#include <process.h>
#include <commctrl.h>

HWND  hWizard = NULL;

static HFONT hFont = NULL;

NUTSProviderList Providers;

FormatList   Formats;
DWORD        Encoding;
FormatDesc   ChosenFS;

std::vector<FormatDesc> FSList;

EncodingEdit *pFilename = nullptr;

bool FormatSet = false;

HWND   hProgressWnd  = NULL;
HWND   hSizeWnd      = NULL;
HANDLE hCreateThread = NULL;

static DWORD dwthreadid;

INT_PTR CALLBACK Wiz1WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
			break;

		case WM_COMMAND:
			{
				return FALSE;
			}
			break;
	}

	return FALSE;
}

INT_PTR CALLBACK Wiz2WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

								Formats = FSPlugins.GetFormats( Providers[ Index ].PluginID );

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

INT_PTR CALLBACK Wiz3WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			{
				RECT rc;

				GetWindowRect( GetDlgItem( hwndDlg, IDC_FILENAME_FRAME ), &rc );

				POINT p = { rc.left, rc.top };

				ScreenToClient( hwndDlg, &p );

				pFilename = new EncodingEdit( hwndDlg, p.x + 8, p.y + 16, 304, true );

				pFilename->Encoding     = Encoding;
				pFilename->AllowedChars = EncodingEdit::textInputAny;

				hSizeWnd = hwndDlg;
			}
			break;

		case WM_SHOWWINDOW:
			{
				if ( wParam == TRUE )
				{
					if ( ChosenFS.Flags & FSF_ArbitrarySize )
					{
						ShowWindow( GetDlgItem( hwndDlg, IDC_SIZE_FRAME ), SW_SHOW );
						ShowWindow( GetDlgItem( hwndDlg, IDC_SIZE_ORDINAL ), SW_SHOW );
						ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGE_SIZE ), SW_SHOW );

						HWND hSizeDropdown = GetDlgItem( hwndDlg, IDC_SIZE_ORDINAL );

						::SendMessage( hSizeDropdown, CB_RESETCONTENT, 0, 0 );
						::SendMessage( hSizeDropdown, CB_ADDSTRING, 0, (LPARAM) L"bytes" );
						::SendMessage( hSizeDropdown, CB_ADDSTRING, 0, (LPARAM) L"Kilobytes" );
						::SendMessage( hSizeDropdown, CB_ADDSTRING, 0, (LPARAM) L"Megabytes" );
						::SendMessage( hSizeDropdown, CB_ADDSTRING, 0, (LPARAM) L"Gigabytes" );

						ShowWindow( GetDlgItem( hwndDlg, IDC_DYNAMIC ), SW_HIDE );
					}
					else
					{
						ShowWindow( GetDlgItem( hwndDlg, IDC_SIZE_FRAME ), SW_HIDE );
						ShowWindow( GetDlgItem( hwndDlg, IDC_SIZE_ORDINAL ), SW_HIDE );
						ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGE_SIZE ), SW_HIDE );

						ShowWindow( GetDlgItem( hwndDlg, IDC_DYNAMIC ), SW_SHOW );
					}

					int SizeField = ::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGE_SIZE ), WM_GETTEXTLENGTH, 0, 0 );

					if ( strlen( (char *) pFilename->GetText() ) == 0 )
					{
						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) ( PSWIZB_BACK ) );
					}
					else if ( ( ChosenFS.Flags & FSF_ArbitrarySize ) && ( SizeField == 0 ) )
					{
						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) ( PSWIZB_BACK ) );
					}
					else
					{
						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) ( PSWIZB_NEXT | PSWIZB_BACK ) );
					}
				}
			}
			return FALSE;

		case WM_COMMAND:
			{
				if ( HIWORD( wParam ) == EN_CHANGE )
				{
					int SizeField = ::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGE_SIZE ), WM_GETTEXTLENGTH, 0, 0 );

					if ( strlen( (char *) pFilename->GetText() ) == 0 )
					{
						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) ( PSWIZB_BACK ) );
					}
					else if ( ( ChosenFS.Flags & FSF_ArbitrarySize ) && ( SizeField == 0 ) )
					{
						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) ( PSWIZB_BACK ) );
					}
					else
					{
						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) ( PSWIZB_NEXT | PSWIZB_BACK ) );
					}
				}

				return FALSE;
			}
			break;
	}

	return FALSE;
}

unsigned int __stdcall CreationThread(void *param)
{
	CTempFile NewImage;

	DataSource *pSource = nullptr;

	unsigned int Result;

	QWORD Length;

	/* Set the image size. This is determined by the FS Flags. Firstly, if it's dynamic, don't bother */
	if ( ! ( ChosenFS.Flags & FSF_DynamicSize ) )
	{
		/* Is it a fixed size? */
		if ( ChosenFS.Flags & FSF_FixedSize )
		{
			NewImage.SetExt( ChosenFS.MaxSize );

			Length = ChosenFS.MaxSize;
		}
		else if ( ChosenFS.Flags & FSF_ArbitrarySize )
		{
			WCHAR Buffer[16];

			::SendMessage( GetDlgItem( hSizeWnd, IDC_IMAGE_SIZE ), WM_GETTEXT, 16, (LPARAM) Buffer );

			QWORD Length;

			try
			{
				Length = std::stoull( std::wstring( Buffer ), nullptr, 10 );

				if ( Length == 0 )
				{
					throw std::exception( "Zero size image" );
				}
			}

			catch ( const std::exception &e )
			{
				// TODO: reference e

				e.what();

				::SendMessage( hProgressWnd, WM_FORMATPROGRESS, 100, 0 );

				return 0;
			}

			int SizeOrdinal = ::SendMessage( GetDlgItem( hSizeWnd, IDC_SIZE_ORDINAL ), CB_GETCURSEL, 0, 0 );

			while ( SizeOrdinal > 0 ) { Length *= 1024; SizeOrdinal--; }

			if ( ChosenFS.Flags & FSF_UseSectors )
			{
				Length -= ( Length % ChosenFS.SectorSize );
			}

			NewImage.SetExt( Length );
		}
	}
	else
	{
		Length = 0;
	}

	pSource = (DataSource *) new ImageDataSource( NewImage.Name() );

	if ( pSource != nullptr )
	{
		FileSystem *FS = (FileSystem *) FSPlugins.LoadFS( ChosenFS.FUID, pSource );

		DS_RELEASE( pSource );

		if ( FS->Format_PreCheck( FTF_Initialise, hProgressWnd ) != 0 )
		{
			::SendMessage( hWizard, PSM_SETCURSEL, (WPARAM) 4, (LPARAM) NULL );

			delete FS;

			return -1;
		}
	
		Result = FS->Format_Process( FTF_Initialise, hProgressWnd );

		delete FS;
	}

	FileSystem *ContainerFS = (FileSystem *) CurrentAction.FS;

	if ( ContainerFS != nullptr )
	{
		NativeFile file;

		file.EncodingID   = ContainerFS->GetEncoding();
		file.Extension[0] = 0;
		file.fileID       = 0;
		file.Flags        = 0;
		file.FSFileType   = 0;
		file.Icon         = FT_MiscImage;
		file.Length       = NewImage.Ext();
		file.Type         = FT_MiscImage;

		BYTE *pF = pFilename->GetText();

		file.Filename = pF;

		ContainerFS->WriteFile( &file, NewImage );
		
		CFileViewer *pPane = (CFileViewer *) CurrentAction.Pane;

		pPane->Updated = true;
		pPane->Refresh();
	}

	/* This is here purely to make the "and I'm spent" moment visible to the user */
	Sleep( 2000 );

	::SendMessage( hWizard, PSM_SETCURSEL, (WPARAM) 4, (LPARAM) NULL );

	return Result;
}


INT_PTR CALLBACK Wiz4WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			{
				SendMessage(GetDlgItem(hwndDlg, IDC_CREATE_PROGRESS), PBM_SETRANGE32, 0, 100);
				SendMessage(GetDlgItem(hwndDlg, IDC_CREATE_PROGRESS), PBM_SETPOS, 0, 0);

				hProgressWnd  = hwndDlg;
				hCreateThread = (HANDLE) _beginthreadex(NULL, NULL, CreationThread, NULL, NULL, (unsigned int *) &dwthreadid);
			}
			break;

		case WM_FORMATPROGRESS:
			{
				int Percent = (int) wParam;
				WCHAR *msg  = (WCHAR *) lParam;

				::SendMessage( GetDlgItem( hwndDlg, IDC_CREATE_OP ), WM_SETTEXT, 0, (LPARAM) msg );
				::SendMessage(GetDlgItem(hwndDlg, IDC_CREATE_PROGRESS), PBM_SETPOS, wParam, 0 );
			}
			break;

		case WM_SHOWWINDOW:
			{
				if ( wParam == TRUE )
				{
					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, 0 );
				}
			}
			return FALSE;

		case WM_COMMAND:
			{
				return FALSE;
			}
			break;
	}

	return FALSE;
}

INT_PTR CALLBACK Wiz5WindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			{
				::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_SUCCESS ), WM_SETFONT, (WPARAM) hFont, (LPARAM) TRUE );
				::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, PSWIZB_FINISH );
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

int CALLBACK WizPageProc( HWND hwndDlg, UINT uMsg, LPARAM lParam )
{
	hWizard = hwndDlg;

	return 0;
}

int ImageWiz_Handler( AppAction Action )
{
	CurrentAction	= Action;

	DWORD *pEncoding = (DWORD *) Action.pData;

	Encoding = *pEncoding;

	PROPSHEETPAGE psp[ 5 ];

	PROPSHEETHEADER psh;

	psp[0].dwSize      = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags     = PSP_HIDEHEADER; 
	psp[0].hInstance   = hInst;
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_WIZ_DIALOG1);
	psp[0].pszIcon     = 0;
	psp[0].pfnDlgProc  = Wiz1WindowProc;
	psp[0].lParam      = 0;
	psp[0].pfnCallback = NULL;
	psp[0].pszTitle    = L"New Image Wizard";

	psp[1].dwSize      = sizeof(PROPSHEETPAGE);
	psp[1].dwFlags     = PSP_USEHEADERTITLE | PSP_USEHEADERSUBTITLE | PSP_DEFAULT;
	psp[1].hInstance   = hInst;
	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_WIZ_DIALOG2);
	psp[1].pszIcon     = 0;
	psp[1].pfnDlgProc  = Wiz2WindowProc;
	psp[1].lParam      = 0;
	psp[1].pfnCallback = NULL;
	psp[1].pszTitle    = L"New Image Wizard";
	psp[1].pszHeaderTitle = L"Select system for image";
	psp[1].pszHeaderSubTitle = L"Select the container format for the new image file."; 

	psp[2].dwSize      = sizeof(PROPSHEETPAGE);
	psp[2].dwFlags     = PSP_USEHEADERTITLE | PSP_USEHEADERSUBTITLE | PSP_DEFAULT;
	psp[2].hInstance   = hInst;
	psp[2].pszTemplate = MAKEINTRESOURCE(IDD_WIZ_DIALOG3);
	psp[2].pszIcon     = 0;
	psp[2].pfnDlgProc  = Wiz3WindowProc;
	psp[2].lParam      = 0;
	psp[2].pfnCallback = NULL;
	psp[2].pszTitle    = L"New Image Wizard";
	psp[2].pszHeaderTitle = L"Specify size and filename";
	psp[2].pszHeaderSubTitle = L"Specify the size of the image and the filename for the container file."; 

	psp[3].dwSize      = sizeof(PROPSHEETPAGE);
	psp[3].dwFlags     = PSP_USEHEADERTITLE | PSP_USEHEADERSUBTITLE | PSP_DEFAULT;
	psp[3].hInstance   = hInst;
	psp[3].pszTemplate = MAKEINTRESOURCE(IDD_WIZ_DIALOG4);
	psp[3].pszIcon     = 0;
	psp[3].pfnDlgProc  = Wiz4WindowProc;
	psp[3].lParam      = 0;
	psp[3].pfnCallback = NULL;
	psp[3].pszTitle    = L"New Image Wizard";
	psp[3].pszHeaderTitle = L"Creating image";
	psp[3].pszHeaderSubTitle = L"The specified container image is being created."; 

	psp[4].dwSize      = sizeof(PROPSHEETPAGE);
	psp[4].dwFlags     = PSP_HIDEHEADER; 
	psp[4].hInstance   = hInst;
	psp[4].pszTemplate = MAKEINTRESOURCE(IDD_WIZ_DIALOG5);
	psp[4].pszIcon     = 0;
	psp[4].pfnDlgProc  = Wiz5WindowProc;
	psp[4].lParam      = 0;
	psp[4].pfnCallback = NULL;
	psp[4].pszTitle    = L"New Image Wizard";

	psh.dwSize         = sizeof(PROPSHEETHEADER);
	psh.dwFlags        = PSH_WATERMARK | PSH_PROPSHEETPAGE | PSH_USECALLBACK | PSH_WIZARD97 | PSH_HEADER;
	psh.hwndParent     = Action.hWnd;
	psh.hInstance      = hInst;
	psh.pszbmWatermark = MAKEINTRESOURCE( IDB_WIZ_MARK );
	psh.pszCaption     = (LPWSTR) L"New Image Wizard";
	psh.nPages         = 5;
	psh.nStartPage     = 0;
	psh.ppsp           = (LPCPROPSHEETPAGE) &psp;
	psh.pfnCallback    = WizPageProc;
	psh.pszbmHeader    = MAKEINTRESOURCE( IDB_WIZBANNER );
    
	PropertySheet( &psh );

	if ( pFilename )
	{
		delete pFilename;
	}
    
	return 0;
}
