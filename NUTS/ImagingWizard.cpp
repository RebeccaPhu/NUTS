#include "stdafx.h"

#include "ImagingWizard.h"
#include "AppAction.h"
#include "resource.h"
#include "Plugins.h"
#include "ImageDataSource.h"
#include "FileDialogs.h"
#include "NUTSError.h"

#include "libfuncs.h"

#include <process.h>
#include <commctrl.h>
#include <WindowsX.h>
#include <WinUser.h>

static HWND  hWizard = NULL;

static HFONT hFont, hWhatFont = NULL;

static NUTSProviderList Providers;

static WrapperIdentifier ChosenWrapper = WID_Null;

static bool FormatSet  = false;
static bool WrapperSet = false;

static HANDLE hImagingThread = NULL;
static HANDLE hImagingStop   = NULL;

static DWORD dwthreadid;

static BYTE  AutoOrManual = 1U;

static std::wstring ConcludeTitle;
static std::wstring ConcludeMessage;

void MakeFont()
{
	if ( !hFont ) {
		hFont	= CreateFont( 24,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
	}

	if ( !hWhatFont ) {
		hWhatFont	= CreateFont(10,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
	}
}

BOOL WizCancel( HWND hwndDlg )
{
	ConcludeTitle   = L"Imaging Wizard Cancelled";
	ConcludeMessage = L"The wizard was cancelled before the process could start. No changes have been made to any data source.";

	SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

	::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 8, (LPARAM) NULL );

	return TRUE;
}

INT_PTR CALLBACK ImagingWizBegin(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
	case WM_INITDIALOG:
		{
			MakeFont();

			::SendMessage( GetDlgItem( hwndDlg, IDC_WIZ_WELCOME ), WM_SETFONT, (WPARAM) hFont, (LPARAM) TRUE );

			::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_NEXT );
		}
		return (INT_PTR) TRUE;
	}

	return (INT_PTR) FALSE;
}

INT_PTR CALLBACK ImagingWizAutoManual(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
	case WM_COMMAND:
		{
			switch ( LOWORD( wParam ) )
			{
			case IDC_IMAGING_WIZ_AUTO:
				AutoOrManual = 1U;
				break;

			case IDC_IMAGING_WIZ_MANUAL:
				AutoOrManual = 2U;
				break;
			}
		}
		break;

	case WM_NOTIFY:
		{
			NMHDR *pNM = (NMHDR *) lParam;

			if ( pNM->code == PSN_QUERYCANCEL )
			{
				return WizCancel( hwndDlg );
			}

			if ( pNM->code ==  PSN_SETACTIVE )
			{
				if ( AutoOrManual == 1U )
				{
					CheckRadioButton( hwndDlg, IDC_IMAGING_WIZ_AUTO, IDC_IMAGING_WIZ_MANUAL, IDC_IMAGING_WIZ_AUTO );
				}

				if ( AutoOrManual == 2U )
				{
					CheckRadioButton( hwndDlg, IDC_IMAGING_WIZ_AUTO, IDC_IMAGING_WIZ_MANUAL, IDC_IMAGING_WIZ_MANUAL );
				}

				::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_NEXT | PSWIZB_BACK );
			}

			if ( pNM->code == PSN_WIZNEXT )
			{
				// If we choose auto, go to page 2, else go to page 3.
				SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

				if ( AutoOrManual == 1U )
				{
					// Auto
					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 2, (LPARAM) NULL );
				}

				if ( AutoOrManual == 2U )
				{
					// Manual
					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 3, (LPARAM) NULL );
				}

				return TRUE;
			}
		}
		break;
	}

	return (INT_PTR) FALSE;
}

static FileSystem *pDetectFS = nullptr;

unsigned int __stdcall AutoDetectThread(void *param)
{
	FileSystem *FS = (FileSystem *) CurrentAction.FS;
	
	DWORD FileID = CurrentAction.Selection[ 0 ].fileID;

	DataSource *pSource = FS->FileDataSource( FileID );

	if ( pDetectFS != nullptr )
	{
		// Delete previous instance
		delete pDetectFS;
	}

	pDetectFS = FSPlugins.FindAndLoadFS( pSource, &CurrentAction.Selection[ 0 ] );

	DS_RELEASE( pSource );

	return 0;
}

static bool GotItRight = false;

INT_PTR CALLBACK ImagingWizAutoDetect(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
	case WM_TIMER:
		{
			if ( WaitForSingleObject( hImagingThread, 0 ) == WAIT_OBJECT_0 )
			{
				// Cool. Cool.
				CloseHandle( hImagingThread );

				hImagingThread = NULL;

				KillTimer( hwndDlg, 0x0E7E );

				ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_AUTODETECT ), SW_HIDE );

				std::wstring FSName = L"File system not detected";

				if ( pDetectFS != nullptr )
				{
					FSName = FSPlugins.FSName( pDetectFS->FSID );

					ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_CORRECT_PROMPT ), SW_SHOW );
					ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FSCORRECT ), SW_SHOW );
					ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_WRONG ), SW_SHOW );
				}
				else
				{
					// Enable the next button. It'll take us to the manual page.
					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_NEXT | PSWIZB_BACK );
				}

				::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_DETECTED ), WM_SETTEXT, 0, (LPARAM) FSName.c_str() );

				ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_DETECTED ), SW_SHOW );
			}
		}
		break;

	case WM_COMMAND:
		{
			switch ( LOWORD( wParam ) )
			{
				case IDC_IMAGING_WIZ_FSCORRECT:
					GotItRight = true;
					break;

				case IDC_IMAGING_WIZ_WRONG:
					GotItRight = false;
					break;
			}

			::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );
		}
		break;

	case WM_NOTIFY:
		{
			NMHDR *pNM = (NMHDR *) lParam;

			if ( pNM->code == PSN_QUERYCANCEL )
			{
				return WizCancel( hwndDlg );
			}

			if ( pNM->code ==  PSN_SETACTIVE )
			{
				// Make sure the initial bits are invisible
				ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_DETECTED ), SW_HIDE );
				ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_CORRECT_PROMPT ), SW_HIDE );
				ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FSCORRECT ), SW_HIDE );
				ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_WRONG ), SW_HIDE );

				// Show the marquee
				SetWindowLong( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_AUTODETECT ), GWL_STYLE, GetWindowLong( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_AUTODETECT ), GWL_STYLE) | PBS_MARQUEE);
				ShowWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_AUTODETECT ), SW_SHOW );
				::PostMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_AUTODETECT ), PBM_SETMARQUEE, (WPARAM) TRUE, 0 );

				// Disable the buttons
				::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK );

				// Start the detector thread - just in case the process is long lived
				if ( hImagingThread != NULL )
				{
					SetEvent( hImagingStop );

					if ( WaitForSingleObject( hImagingThread, 2000 ) != WAIT_OBJECT_0 )
					{
						TerminateThread( hImagingThread, 500 );
					}

					CloseHandle( hImagingThread );
				}

				ResetEvent( hImagingStop );

				hImagingThread = (HANDLE) _beginthreadex(NULL, NULL, AutoDetectThread, NULL, NULL, (unsigned int *) &dwthreadid );

				SetTimer( hwndDlg, 0x0E7E, 500, NULL );
			}

			if ( pNM->code == PSN_WIZNEXT )
			{
				// If we got it right, we'll go to 4. Otherwise, jump to 3.
				SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

				if ( pDetectFS != nullptr )
				{
					if ( GotItRight )
					{
						// Yay!
						::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 4, (LPARAM) NULL );
					}
					else
					{
						// Boo!
						::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 3, (LPARAM) NULL );
					}
				}
				else
				{
					// Couldn't figure it out at all. Next takes us to 3.
					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 3, (LPARAM) NULL );
				}

				return TRUE;
			}

			if ( pNM->code == PSN_WIZBACK )
			{
				// Always jump to 1
				SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

				::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 1, (LPARAM) NULL );

				return TRUE;
			}
		}
		break;
	}

	return (INT_PTR) FALSE;
}

static FormatList   Formats;
static std::vector<FormatDesc> FSList;
static FormatDesc   ChosenFS;

INT_PTR CALLBACK ImagingWizChooseFS(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code == PSN_QUERYCANCEL )
				{
					return WizCancel( hwndDlg );
				}

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

				if ( pNM->code == PSN_WIZBACK )
				{
					// Always jump to 1
					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 1, (LPARAM) NULL );

					return TRUE;
				}

				if ( pNM->code == PSN_WIZNEXT )
				{
					// Initialise pDetectFS
					if ( pDetectFS != nullptr )
					{
						delete pDetectFS;
					}

					FileSystem *FS = (FileSystem *) CurrentAction.FS;

					DWORD FileID = CurrentAction.Selection[ 0 ].fileID;

					DataSource *pSource = FS->FileDataSource( FileID );

					pDetectFS = FSPlugins.LoadFSWithWrappers( ChosenFS.FSID, pSource );

					DS_RELEASE( pSource );
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

static BYTE ReadWriteOp = 0U;
static std::wstring ImageFilename = L"";

static FileSystem *pImageFS = nullptr;

INT_PTR CALLBACK ImagingWizTarget(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code == PSN_QUERYCANCEL )
				{
					return WizCancel( hwndDlg );
				}

				if ( pNM->code ==  PSN_SETACTIVE )
				{
					if ( ImageFilename != L"" )
					{
						::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FILENAME ), WM_SETTEXT, 0, (LPARAM) ImageFilename.c_str() );

						if ( ReadWriteOp != 0U )
						{
							::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );
						}
					}
					else
					{
						::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FILENAME ), WM_SETTEXT, 0, (LPARAM) L"No File Selected" );

						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK );
					}

					if ( ReadWriteOp != 0U )
					{
						// Enable the select button
						EnableWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FILENAME ), TRUE );
						EnableWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_CHOOSE_FILE ), TRUE );
					}
					else
					{
						EnableWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FILENAME ), FALSE );
						EnableWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_CHOOSE_FILE ), FALSE );
					}
				}

				if ( pNM->code == PSN_WIZBACK )
				{
					// Always jump to 1
					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 1, (LPARAM) NULL );

					return TRUE;
				}

				if ( pNM->code == PSN_WIZNEXT )
				{
					DataSource *pSource = new ImageDataSource( ImageFilename );

					// Jump to 6 if this is a write to object from file, otherwise jump to 5 to select wrapper.
					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					if ( ReadWriteOp == 1U )
					{
						// writing to object
						::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 6, (LPARAM) NULL );
					}
					else if ( ReadWriteOp == 2U)
					{
						::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 5, (LPARAM) NULL );
					}

					if ( pImageFS != nullptr )
					{
						delete pImageFS;
					}

					pImageFS = FSPlugins.LoadFS( pDetectFS->FSID, pSource );

					DS_RELEASE( pSource );

					return TRUE;
				}
			}
			break;

	case WM_COMMAND:
		{
			switch ( LOWORD( wParam ) )
			{
				case IDC_IMAGING_WIZ_READIMAGE:
					ReadWriteOp = 2U;

					::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FILENAME ), WM_SETTEXT, 0, (LPARAM) L"No File Selected" );

					ImageFilename = L"";

					break;

				case IDC_IMAGING_WIZ_WRITEIMAGE:
					ReadWriteOp = 1U;

					::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FILENAME ), WM_SETTEXT, 0, (LPARAM) L"No File Selected" );

					ImageFilename = L"";

					break;

				case IDC_IMAGING_WIZ_CHOOSE_FILE:
					{
						if ( ReadWriteOp == 1U )
						{
							// Write image to object
							if ( OpenFileDialog( hwndDlg, ImageFilename, L"File System Image", L"*", L"Select file system image" ) )
							{
								::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FILENAME ), WM_SETTEXT, 0, (LPARAM) ImageFilename.c_str() );

								::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );
							}
						}

						if ( ReadWriteOp == 2U )
						{
							// Read image from object
							std::wstring ext = FSPlugins.FSExt( pDetectFS->FSID );

							if ( SaveFileDialog( hwndDlg, ImageFilename, L"File System Image (*." + ext + L")", ext, L"Select file system image" ) )
							{
								FILE *f;

								(void) _wfopen_s( &f, ImageFilename.c_str(), L"rb" );

								if ( f != NULL )
								{
									fclose( f );

									if ( MessageBox( hwndDlg,
										L"The file specified already exists. If you proceed, the image file will be overwritten. This operation cannot be undone.\r\n\r\n"
										L"Are you sure you want to use this file?", L"NUTS Imaging Wizard", MB_ICONWARNING | MB_YESNO ) == IDYES )
									{
										f = NULL;
									}
								}

								if ( f == NULL )
								{
									::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FILENAME ), WM_SETTEXT, 0, (LPARAM) ImageFilename.c_str() );

									::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );
								}
								else
								{
									ImageFilename = L"";
								}
							}
						}
					}
					break;
			}

			if ( ReadWriteOp != 0U )
			{
				EnableWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_FILENAME ), TRUE );
				EnableWindow( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_CHOOSE_FILE ), TRUE );
			}

			if ( ( ImageFilename != L"" ) && ( ReadWriteOp != 0U ) )
			{
				::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );
			}
			else
			{
				::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK );
			}
		}
		break;

	}

	return (INT_PTR) FALSE;
}

INT_PTR CALLBACK ImagingWizWrapper(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code == PSN_QUERYCANCEL )
				{
					return WizCancel( hwndDlg );
				}

				if ( pNM->code ==  PSN_SETACTIVE )
				{
					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );

					WrapperList list = FSPlugins.GetWrappers();

					std::vector<WrapperIdentifier> recs = pImageFS->RecommendWrappers();

					::SendMessage( GetDlgItem( hwndDlg, IDC_WRAPPERS ), LB_RESETCONTENT, NULL, NULL );

					if ( recs.size() == 0U )
					{
						if ( ChosenWrapper == L"" )
						{
							CheckRadioButton( hwndDlg, IDC_NO_WRAPPER, IDC_SOME_WRAPPER, IDC_NO_WRAPPER );
						}

						SetWindowText( GetDlgItem( hwndDlg, IDC_NO_WRAPPER ), L"No wrapper (recommended)" );
					}
					else
					{
						if ( ChosenWrapper == L"" )
						{
							CheckRadioButton( hwndDlg, IDC_NO_WRAPPER, IDC_SOME_WRAPPER, IDC_SOME_WRAPPER );
						}

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

						if ( iWrapper->Identifier == ChosenWrapper )
						{
							c = i;
						}

						::SendMessage( GetDlgItem( hwndDlg, IDC_WRAPPERS ), LB_ADDSTRING, NULL, (LPARAM) WrapperName.c_str() );

						i++;
					}

					if ( c != -1 )
					{
						::SendMessage( GetDlgItem( hwndDlg, IDC_WRAPPERS ), LB_SETCURSEL, c, NULL );

						CheckRadioButton( hwndDlg, IDC_NO_WRAPPER, IDC_SOME_WRAPPER, IDC_SOME_WRAPPER );
					}
					else if ( ( s != -1 ) && ( ChosenWrapper == L"" ) )
					{
						::SendMessage( GetDlgItem( hwndDlg, IDC_WRAPPERS ), LB_SETCURSEL, s, NULL );

						::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );
					}
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
					}
					break;
				}
			}
			break;
	}

	return (INT_PTR) FALSE;
}

INT_PTR CALLBACK ImagingWizSummary(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
		case WM_NOTIFY:
			{
				NMHDR *pNM = (NMHDR *) lParam;

				if ( pNM->code == PSN_QUERYCANCEL )
				{
					return WizCancel( hwndDlg );
				}

				if ( pNM->code ==  PSN_SETACTIVE )
				{
					MakeFont();

					// Fill in the summary

					// File system
					::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_WHATFS ), WM_SETTEXT, 0, (LPARAM) FSPlugins.FSName( pDetectFS->FSID ).c_str() );

					// Wrapper, if any
					std::wstring WrapperText1,WrapperText2,WrapperLink,WrapperTextFinal;
					
					WrapperText1 = L"No wrapper in use";
					WrapperText2 = L"No wrapper in use";

					if ( ReadWriteOp == 1U )
					{
						WrapperLink = L" <= ";
					}
					else
					{
						WrapperLink = L" => ";
					}

					// write image to object
					if ( pDetectFS->pSource->Feedback != L"" )
					{
						WrapperText1 = pDetectFS->pSource->Feedback;
					}

					if ( ( ChosenWrapper != L"" ) && ( ChosenWrapper != WID_Null ) )
					{
						WrapperText2 = FSPlugins.GetWrapperName( ChosenWrapper );
					}
					
					WrapperTextFinal = WrapperText1 + WrapperLink + WrapperText2;

					::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_WHAT_WRAPPER ), WM_SETTEXT, 0, (LPARAM) WrapperTextFinal.c_str() );

					// And now, the actions
					std::wstring ImagingActions;

					if ( ReadWriteOp == 2U )
					{
						ImagingActions += L"Create image file " + ImageFilename + L"\r\nFormat image file\r\nRead imaging object to image file";
					}
					else
					{
						ImagingActions += L"Write image file to imaging object";
					}

					::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_WHATACTIONS ), WM_SETTEXT, 0, (LPARAM) ImagingActions.c_str() );

					::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_BACK | PSWIZB_NEXT );

					::SendMessage( GetDlgItem( hwndDlg, IDC_FORMATWHAT ),    WM_SETFONT, 0, (LPARAM) hWhatFont );
					::SendMessage( GetDlgItem( hwndDlg, IDC_FORMATWRAPPER ), WM_SETFONT, 0, (LPARAM) hWhatFont );
					::SendMessage( GetDlgItem( hwndDlg, IDC_FORMATWHICH ),   WM_SETFONT, 0, (LPARAM) hWhatFont );
				}

				if ( pNM->code == PSN_WIZBACK )
				{
					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					// If this is a read op, jump to 5, else jump to 4.
					if ( ReadWriteOp == 1U )
					{
						// Write image to object
						::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 4, (LPARAM) NULL );
					}

					if ( ReadWriteOp == 2U )
					{
						// Write image to object
						::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 5, (LPARAM) NULL );
					}

					return TRUE;
				}
			}
			break;
	}

	return (INT_PTR) FALSE;
}

static volatile bool IsFormatting;
static volatile bool IsImaging;

unsigned int __stdcall ImagingThread(void *param)
{
	/*
		OK this process is really clever. What we have right now is two FileSystem objects:
		pDetectFS, which was loaded to deal with the imaging object, and pImageFS which is
		attached to the image file we're going to use as the other end of the transaction.

		If we are writing to the image file, we'll run the format process first, which will
		leave us with a serviceable data object. Otherwise, we'll Init the pImageFS.

		Meanwhile, the imaging object is either in post-offer state, or newly initalised
		state, depending on whether it was auto or manually detected respectively. We'll
		call Init() on that, so it sets up the interface (Disk Shape et al) correctly.

		This leaves us with two data sources that can be copied from one to the other, with
		a FileSystem attached to each. We can then just call Imaging() with the two
		DataSource's and watch the magic happen.
	*/

	// Get some window handles
	HWND hDlg = (HWND) param;

	// Going to have to regenerate pImageFS, because there might be a wrapper.
	DataSource *pSource = pImageFS->pSource;

	// Retain this, so it doesn't run away
	DS_RETAIN( pSource );

	delete pImageFS;

	if ( ( ChosenWrapper != L"" ) && ( ChosenWrapper != WID_Null ) )
	{
		DataSource *pWrapper = FSPlugins.LoadWrapper( ChosenWrapper, pSource );

		pImageFS = FSPlugins.LoadFS( pDetectFS->FSID, pWrapper );

		{
			DS_RELEASE( pSource );
		}
		// Braces due to macro scope in DEBUG builds.
		{
			DS_RELEASE( pWrapper );
		}
	}
	else
	{
		pImageFS = FSPlugins.LoadFS( pDetectFS->FSID, pSource );

		DS_RELEASE( pSource );
	}

	// Now we're ready to do this.

	if ( ReadWriteOp == 2U )
	{
		// Read object to image. We need to format it.
		IsFormatting = true;

		// Need to delete + recreate first.
		(void) _wunlink( ImageFilename.c_str() );

		CTempFile FileCreate( ImageFilename );

		FileCreate.Dump();

		FileCreate.SetExt( pDetectFS->pSource->PhysicalDiskSize );

		FileCreate.Keep();

		// Note that we explicitly exclude the initialise flag here. We don't want an
		// actual file system on the target image, we just want it formatically initialised.
		DWORD Flags = FTF_Blank | FTF_LLF;

		if ( pImageFS->pSource->Flags & DS_SupportsTruncate )
		{
			Flags |= FTF_Truncate;
		}

		if ( pImageFS->Format_Process( Flags, hDlg ) != DS_SUCCESS )
		{
			ConcludeTitle   = L"Imaging Failed";
			ConcludeMessage = L"The imaging operation failed while formatting the target image file: " + pGlobalError->GlobalString;

			return 0;
		}

		IsFormatting = false;
	}
	else
	{
		pImageFS->Init();
	}

	if ( WaitForSingleObject( hImagingStop, 0 ) == WAIT_OBJECT_0 )
	{
		return 0;
	}

	IsImaging = true;

	pDetectFS->Init();

	if ( pDetectFS->Imaging( pDetectFS->pSource, pImageFS->pSource ) != DS_SUCCESS )
	{
		ConcludeTitle   = L"Imaging Failed";
		ConcludeMessage = L"The imaging operation failed: " + pGlobalError->GlobalString;

		return 0;
	}

	ConcludeTitle   = L"Imaging Successful";
	ConcludeMessage = L"The specified imaging operation was completed with no errors.";

	// This is an inverse Docherty-threshold. It makes the user think something has actually happened.
	Sleep( 2000 );

	return 0;
}

INT_PTR CALLBACK ImagingWizExecute(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
	case WM_NOTIFY:
		{
			NMHDR *pNM = (NMHDR *) lParam;

			if ( pNM->code == PSN_QUERYCANCEL )
			{
				SuspendThread( hImagingThread );

				if ( MessageBox( hwndDlg,
					L"If you cancel the imaging operation, either the imaging object or the image file may be corrupt and unusable.\r\n\r\nAre you sure you want to cancel?",
					L"NUTS Imaging Wizard", MB_ICONWARNING | MB_YESNO ) == IDYES )
				{
					KillTimer( hwndDlg, 0x5073 );

					SetEvent( hImagingStop );

					if ( IsFormatting )
					{
						(void) pImageFS->CancelFormat();
					}

					if ( IsImaging )
					{
						(void) pDetectFS->StopImaging();
					}

					ResumeThread( hImagingThread );

					// Wait for the thread to die off
					if ( WaitForSingleObject( hImagingThread, 5000 ) != WAIT_OBJECT_0 )
					{
						TerminateThread( hImagingThread, 500 );
					}

					CloseHandle( hImagingThread );

					hImagingThread = NULL;

					// Jump to the closing credits
					ConcludeTitle   = L"Imaging Cancelled";
					ConcludeMessage = L"The imaging operation was cancelled part way through the process. The imaging object or the image file may be corrupt and unusable.";

					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 8, (LPARAM) NULL );

					return TRUE;
				}
				else
				{
					ResumeThread( hImagingThread );
				}

				return FALSE;
			}

			if ( pNM->code ==  PSN_SETACTIVE )
			{
				IsImaging    = false;
				IsFormatting = false;

				ResetEvent( hImagingStop );

				hImagingThread = (HANDLE) _beginthreadex(NULL, NULL, ImagingThread, (void *) hwndDlg, NULL, (unsigned int *) &dwthreadid );

				SetTimer( hwndDlg, 0x5073, 500, NULL );
			}
		}
		break;

	case WM_TIMER:
		{
			if ( hImagingThread != NULL )
			{
				if ( WaitForSingleObject( hImagingThread, 100 ) == WAIT_OBJECT_0 )
				{
					CloseHandle( hImagingThread );

					hImagingThread = NULL;

					KillTimer( hwndDlg, 0x5073 );

					SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR) TRUE );

					::PostMessage( hWizard, PSM_SETCURSEL, (WPARAM) 8, (LPARAM) NULL );
				}
			}
		}
		break;

	case WM_FORMATPROGRESS:
		{
			int Percent = (int) wParam;
			WCHAR *msg  = (WCHAR *) lParam;

			std::wstring per = std::to_wstring( (QWORD) (unsigned) Percent ) + L" %";

			::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_OP ),       WM_SETTEXT, 0,     (LPARAM) msg );
			::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_PROGRESS ), PBM_SETPOS, wParam, 0 );
			::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_PERCENT ),  WM_SETTEXT, 0,     (LPARAM) per.c_str() );			
		}
	}

	return (INT_PTR) FALSE;
}

INT_PTR CALLBACK ImagingWizResult(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
	case WM_INITDIALOG:
		{
			MakeFont();

			::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_RESULT_TITLE ), WM_SETFONT, (WPARAM) hFont, (LPARAM) TRUE );
			::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_RESULT_TITLE ), WM_SETTEXT, (WPARAM) hFont, (LPARAM) ConcludeTitle.c_str() );
			::SendMessage( GetDlgItem( hwndDlg, IDC_IMAGING_WIZ_RESULT_DESC ),  WM_SETTEXT, (WPARAM) hFont, (LPARAM) ConcludeMessage.c_str() );

			::SendMessage( hWizard, PSM_SETWIZBUTTONS, 0, (LPARAM) PSWIZB_FINISH );
		}
		return (INT_PTR) TRUE;
	}

	return (INT_PTR) FALSE;
}

static int CALLBACK WizPageProc( HWND hwndDlg, UINT uMsg, LPARAM lParam )
{
	hWizard = hwndDlg;

	return 0;
}

typedef struct _WizPage
{
	DLGPROC      HandlerProc;
	int          DlgRes;
	std::wstring Title;
	std::wstring SubTitle;
} WizPage;

int ImagingWiz_Handler( AppAction Action )
{
	if ( hImagingStop == NULL )
	{
		hImagingStop = CreateEvent( NULL, TRUE, FALSE, NULL );
	}

	WizPage pages[] = {
		{ ImagingWizAutoManual, IDD_IMAGING_WIZ_SCANFS,     L"Select File System", L"Select the file system that is being imaged, or allow NUTS to auto-detect it" },
		{ ImagingWizAutoDetect, IDD_IMAGING_WIZ_AUTODETECT, L"File System Auto Detect", L"Confirm the autodetected file system is correct" },
		{ ImagingWizChooseFS,   IDD_IMAGING_WIZ_SELECTFS,   L"Select File System", L"Select the file system that is being imaged" },
		{ ImagingWizTarget,     IDD_IMAGING_WIZ_TARGET,     L"Select the operation and image file", L"Choose the file to image and whether to read or write" },
		{ ImagingWizWrapper,    IDD_IMAGING_WIZ_WRAPPER,    L"Select the wrapper", L"Choose the wrapper to use with the image file" },
		{ ImagingWizSummary,    IDD_IMAGING_WIZ_SUMMARY,    L"Summary", L"Summary of proposed operations" },
		{ ImagingWizExecute,    IDD_IMAGING_WIZ_PROGRESS,   L"Imaging the file system", L"Processing the file system image" },
	};

	CurrentAction	= Action;

	PROPSHEETPAGE psp[ 9 ];

	PROPSHEETHEADER psh;

	psp[0].dwSize      = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags     = PSP_HIDEHEADER; 
	psp[0].hInstance   = hInst;
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_IMAGING_WIZ_BEGIN );
	psp[0].pszIcon     = 0;
	psp[0].pfnDlgProc  = ImagingWizBegin;
	psp[0].lParam      = 0;
	psp[0].pfnCallback = NULL;
	psp[0].pszTitle    = L"Imaging Wizard";

	for ( int i=1; i<8; i++ )
	{
		psp[i].dwSize      = sizeof(PROPSHEETPAGE);
		psp[i].dwFlags     = PSP_USEHEADERTITLE | PSP_USEHEADERSUBTITLE | PSP_DEFAULT;
		psp[i].hInstance   = hInst;
		psp[i].pszTemplate = MAKEINTRESOURCE( pages[i - 1].DlgRes );
		psp[i].pszIcon     = 0;
		psp[i].pfnDlgProc  = pages[i - 1].HandlerProc;
		psp[i].lParam      = 0;
		psp[i].pfnCallback = NULL;
		psp[i].pszTitle    = L"Imaging Wizard";
		psp[i].pszHeaderTitle = pages[i - 1].Title.c_str();
		psp[i].pszHeaderSubTitle = pages[i - 1].SubTitle.c_str(); 
	}

	psp[8].dwSize      = sizeof(PROPSHEETPAGE);
	psp[8].dwFlags     = PSP_HIDEHEADER; 
	psp[8].hInstance   = hInst;
	psp[8].pszTemplate = MAKEINTRESOURCE( IDD_IMAGING_WIZ_RESULT );
	psp[8].pszIcon     = 0;
	psp[8].pfnDlgProc  = ImagingWizResult;
	psp[8].lParam      = 0;
	psp[8].pfnCallback = NULL;
	psp[8].pszTitle    = L"Imaging Wizard";

	psh.dwSize         = sizeof(PROPSHEETHEADER);
	psh.dwFlags        = PSH_WATERMARK | PSH_PROPSHEETPAGE | PSH_USECALLBACK | PSH_WIZARD97 | PSH_HEADER;
	psh.hwndParent     = Action.hWnd;
	psh.hInstance      = hInst;
	psh.pszbmWatermark = MAKEINTRESOURCE( IDB_WIZ_MARK );
	psh.pszCaption     = (LPWSTR) L"Imaging Wizard";
	psh.nPages         = 9;
	psh.nStartPage     = 0;
	psh.ppsp           = (LPCPROPSHEETPAGE) &psp;
	psh.pfnCallback    = WizPageProc;
	psh.pszbmHeader    = MAKEINTRESOURCE( IDB_WIZBANNER );

	AutoOrManual   = 1U;
	hImagingThread = NULL;
	GotItRight     = false;
	ReadWriteOp    = 0U; // None selected
	ImageFilename  = L"";
	pDetectFS      = nullptr;
	pImageFS       = nullptr;
	ChosenWrapper  = L"";

	ConcludeTitle   = L"Imaging Wizard Cancelled";
	ConcludeMessage = L"The wizard was cancelled before the process could start. No changes have been made to any data source.";

	ResetEvent( hImagingStop );
    
	PropertySheet( &psh );

	// Kill any remaining thread.

	if ( hImagingThread != NULL )
	{
		SetEvent( hImagingStop );

		if ( WaitForSingleObject( hImagingThread, 500 ) != WAIT_OBJECT_0 )
		{
			TerminateThread( hImagingThread, 500 );
		}

		CloseHandle( hImagingThread );
	}

	if ( pDetectFS != nullptr )
	{
		delete pDetectFS;
	}

	if ( pImageFS != nullptr )
	{
		delete pImageFS;
	}

	return 0;
}
