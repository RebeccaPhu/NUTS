#include "stdafx.h"

#include "AboutBox.h"
#include "Defs.h"
#include "resource.h"

#include "PluginDescriptor.h"
#include "Plugins.h"

#include <commctrl.h>
#include <Richedit.h>
#include <WindowsX.h>
#include <ShellAPI.h>

#include <vector>

HWND pCurrentTab = nullptr;

INT_PTR CALLBACK CreditsDlgFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

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
	case WM_INITDIALOG:
	case WM_ABOUT_RESIZE:
		{
			RECT r;

			GetClientRect( hDlg, &r );

			SetWindowPos( GetDlgItem( hDlg, IDC_LICENSE ), NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOREPOSITION );

			FillRichEditFromFile( GetDlgItem( hDlg, IDC_LICENSE ), L"./License.rtf" );
		}
		break;
	}

	return FALSE;
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

			tie.pszText = L"Plugins / Credits / Thanks";
			TabCtrl_InsertItem( GetDlgItem( hDlg, IDC_ABOUTTABS ), 1, &tie );

			tie.pszText = L"LICENSE / COPYRIGHT";
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

					pCurrentTab = CreateDialogIndirect( hInst, dlg, GetDlgItem( hDlg, IDC_ABOUTTABS ), CreditsDlgFunc );
				}
				
				if ( iSel == 1 )
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

// === Below here is the credits roll

HDC     CreditsDC     = NULL;
HBITMAP CreditsBitmap = NULL;
HGDIOBJ hOld          = NULL;
int     cw;
int     ch;
int     maxh;
int     cpass;
int     cc = 0;
HFONT   h1Font  = NULL;
HFONT   h2Font  = NULL;
HFONT   h3Font  = NULL;
HFONT   hPFont  = NULL;
HFONT   hLFont  = NULL;
HFONT   h2BFont = NULL;
HWND    hScroll = NULL;

const COLORREF NormalLink = RGB( 0, 12, 120 );
const COLORREF HoverLink  = RGB( 120, 12, 0 );

typedef struct _uRect
{
	DWORD left;
	DWORD right;
	DWORD top;
	DWORD bottom;
	std::wstring url;
} uRect;

std::vector<uRect> LinkRects;

void PlaceBitmap( int res, int x, int y, int w, int h, int pass )
{
	if ( pass == 1 )
	{
		HBITMAP bm = LoadBitmap( hInst, MAKEINTRESOURCE( res ) );
		HDC hBM    = CreateCompatibleDC( CreditsDC );

		HGDIOBJ hOldBM = SelectObject( hBM, bm );
		
		BITMAP bms;

		GetObject( bm, sizeof(BITMAP), &bms );

		StretchBlt( CreditsDC, x, y, w, h, hBM, 0, 0, bms.bmWidth, bms.bmHeight, SRCCOPY );

		SelectObject( hBM, hOldBM );

		DeleteDC( hBM );
	}

	if ( ch < y + h )
	{
		ch = y + h;
	}
}

void PlaceBitmap( HBITMAP bm, int x, int y, int w, int h, int pass )
{
	if ( pass == 1 )
	{
		HDC hBM    = CreateCompatibleDC( CreditsDC );

		HGDIOBJ hOldBM = SelectObject( hBM, bm );
		
		BITMAP bms;

		GetObject( bm, sizeof(BITMAP), &bms );

		StretchBlt( CreditsDC, x, y, w, h, hBM, 0, 0, bms.bmWidth, bms.bmHeight, SRCCOPY );

		SelectObject( hBM, hOldBM );

		DeleteDC( hBM );
	}

	if ( ch < y + h )
	{
		ch = y + h;
	}
}

void PlaceText( HFONT hf, int x, int y, int w, int h, std::wstring t, int cpass )
{
	if ( cpass == 1 )
	{
		RECT r;

		r.left   = x;
		r.right  = x + w;
		r.top    = y;
		r.bottom = y + h;

		SelectObject( CreditsDC, hf );

		DrawText( CreditsDC, t.c_str(), t.length(), &r, DT_LEFT | DT_TOP );
	}

	if ( ch < y + h )
	{
		ch = y + h;
	}
}

void PlaceLine( int x, int w, int y, int cpass )
{
	if ( cpass == 1 )
	{
		RECT r;

		r.left   = x;
		r.right  = x + w;
		r.top    = y;
		r.bottom = y;

		DrawEdge( CreditsDC, &r, EDGE_SUNKEN, BF_BOTTOM );
	}

	if ( ch < y + 2 )
	{
		ch = y + 2;
	}
}

std::wstring People[] = {
	L"Duncan Woodward and Andrew Spencer",
	L"Simon Rodriguez"
};

std::wstring Reasons[] = {
	L"Application testing and feedback",
	L"The FXAA anti-aliasing algorithm. See http://blog.simonrodriguez.fr/articles/30-07-2016_implementing_fxaa.html"
};

#define CORE_PEOPLE 2

typedef struct _IconCredit
{
	int BitmapResource;
	std::wstring IconName;
	std::wstring IconSet;
	std::wstring IconAuthor;
	std::wstring License;
	std::wstring URL;
} IconCredit;

IconCredit BitmapIconCreds[] =
{
	{ IDB_ACORN,      L"Acorn Icon", L"FatCow Hosting Extra Icons 2", L"FatCow", L"CC Attribution 3.0 US", L"https://creativecommons.org/licenses/by/3.0/us/" },
	{ IDB_APP,        L"Application Icon", L"Blue Bits Icons", L"Icojam", L"Freeware", L"" },
	{ IDB_BINARY,     L"Mimetypes Binary Icon", L"Crystal Clear Icons", L"Everaldo Coelho", L"LGPL", L"http://www.gnu.org/licenses/lgpl-3.0.html" },
	{ IDB_CDROM,      L"CD-Rom Icon", L"Influens Icons", L"Mat-u", L"CC Attribution-NonCommerical-NoDerivs 3.0 Unported", L"https://creativecommons.org/licenses/by-nc-nd/3.0/" },
	{ IDB_DATA,       L"Server Icon", L"Latte Developer Icons", L"Macintex", L"CC Attribution 3.0 Unported", L"https://creativecommons.org/licenses/by/3.0/" },
	{ IDB_DISKIMAGE,  L"Floppy Icon", L"NX10 Icon Set", L"MazelNL77", L"CC Attribution 3.0 Unported", L"https://creativecommons.org/licenses/by/3.0/" },
	{ IDB_FLOPPYDISC, L"Floppy Drive 3,5 Icon", L"DeepSea Blue Icons", L"MiRa", L"CC Attribution-Non-Commercial-NoDervis 3.0 Unported", L"https://creativecommons.org/licenses/by-nc-nd/3.0/" },
	{ IDB_FOLDER,     L"Generic Folder Blue Icon", L"Smooth Leopard Icons", L"McDo Design", L"Free for non-commerical use.", L"" },
	{ IDB_GRAPHIC,    L"Graphic File Icon", L"32x32 Free Design Icons", L"Aha-Soft", L"CC Attribution 3.0 US", L"https://creativecommons.org/licenses/by/3.0/us/" },
	{ IDB_HARDDISC,   L"Hard Drive Icon", L"Refresh CL Icons", L"TPDK", L"CC Attribution-NonCommercial-NoDerivs 3.0 Unported", L"https://creativecommons.org/licenses/by-nc-nd/3.0/" },
	{ IDB_SCRIPT,     L"Script Icon", L"Stainless! Applications Icons", L"IconLeak", L"", L"" },
	{ IDB_TEXT,       L"Text Icon", L"Crystal Office Icon Set", L"MediaJon", L"Linkware", L"http://www.iconshots.com" },
	{ IDB_TAPEIMAGE,  L"B Side Icon", L"Cassette Tape Icons", L"barkerbaggies", L"CC Attribution-NonCommercial-ShareAlike 3.0 Unported", L"https://creativecommons.org/licenses/by-nc-sa/3.0/" },
	{ IDB_HARDIMAGE,  L"Apps Hard Drive 2 Icon", L"Crystal Project Icons", L"Everaldo Coelho", L"LGPL", L"http://www.gnu.org/licenses/lgpl-3.0.html" },
	{ IDB_CDIMAGE,    L"CD Icon", L"Soft Icons", L"Lokas Software", L"CC Attribution 3.0 Unported", L"http://creativecommons.org/licenses/by/3.0/" },
	{ IDB_ARCHIVE,    L"Light Brown ZIP Icon", L"Cats Icons 2", L"McDo Design", L"Free for non-commerical use.", L"" },
	{ IDB_SYSTEM,     L"Computer Icon", L"Aero Icons", L"Lokas Software", L"CC Attribution 3.0 Unported.", L"https://creativecommons.org/licenses/by/3.0/" },
	{ IDB_WINDOWS,    L"Windows Icon", L"Crystal Intense Icons", L"Tatice", L"CC Attribution-Noncommercial-NoDervis 3.0", L"https://creativecommons.org/licenses/by-nc-nd/3.0/" },
	{ IDB_MUSICFILE,  L"File Music Icon", L"Phuzion Icons", L"kyo-tux", L"CC Attribution-Noncommerical-ShareALike 3.0 Unported", L"https://creativecommons.org/licenses/by-nc-sa/3.0/" },
	{ IDB_ROMDISK,    L"Chip Icon", L"Diagram Old Icons", L"Double-J Design", L"CC Attribution 3.0 Unported", L"https://creativecommons.org/licenses/by/3.0/" },
	{ IDB_TZX,        L"TZX Block, based on Text Icon", L"Crystal Office Icon Set", L"MediaJon", L"Linkware", L"http://www.iconshots.com" },

	{ IDB_PLAYER_EJECT,   L"Player Eject", L"Square Cyan Buttons", L"Axialis Team", L"CC Attribution 2.5 Generic", L"https://creativecommons.org/licenses/by-sa/2.5/" },
	{ IDB_PLAYER_FIRST,   L"Player Previous", L"Square Cyan Buttons", L"Axialis Team", L"CC Attribution 2.5 Generic", L"https://creativecommons.org/licenses/by-sa/2.5/" },
	{ IDB_PLAYER_FORWARD, L"Player Fast Foward", L"Square Cyan Buttons", L"Axialis Team", L"CC Attribution 2.5 Generic", L"https://creativecommons.org/licenses/by-sa/2.5/" },
	{ IDB_PLAYER_PLAY,    L"Player Player", L"Square Cyan Buttons", L"Axialis Team", L"CC Attribution 2.5 Generic", L"https://creativecommons.org/licenses/by-sa/2.5/" },
	{ IDB_PLAYER_REWIND,  L"Player Fast Rewind", L"Square Cyan Buttons", L"Axialis Team", L"CC Attribution 2.5 Generic", L"https://creativecommons.org/licenses/by-sa/2.5/" },
	{ IDB_PLAYER_STOP,    L"Player Stop", L"Square Cyan Buttons", L"Axialis Team", L"CC Attribution 2.5 Generic", L"https://creativecommons.org/licenses/by-sa/2.5/" },

	{ IDB_WIZBANNER,  L"Floppy Icon", L"NX10 Icon set", L"MazeNL77", L"CC Attribution 3.0 Unported", L"https://creativecommons.org/licenses/by/3.0/" },
	{ IDB_WIZBANNER,  L"B Side Icon", L"Cassette Tape Icons", L"barkerbaggies", L"CC Attribution-NonCommercial-ShareAlike 3.0 Unported", L"https://creativecommons.org/licenses/by-nc-sa/3.0/" },
	{ IDB_WIZBANNER,  L"Devices 5.25 Floppy Unmount Icon", L"Glaze Icons", L"Marco Martin", L"LGPL", L"http://www.gnu.org/licenses/lgpl-3.0.html" }
};

#define BITMAPICONCREDS ( sizeof(BitmapIconCreds) / sizeof(IconCredit ) )

IconCredit IconIconCreds[] =
{
	{ IDI_NEWDIR,     L"New Folder Graphite Icon", L"Aqua Candy Revolution Icons", L"McDo Design", L"Free for non-commercial use", L"" },
	{ IDI_FONTSWITCH, L"Font Icon", L"FatCow Hosting Icons", L"FatCow", L"CC Attribution 3.0 US", L"https://creativecommons.org/licenses/by/3.0/us/" },
	{ IDI_PRINT,      L"Print Icon", L"16x16 Free Application Icons", L"Aha-Soft", L"CC Attribution-ShareAlike 3.0 Unported", L"https://creativecommons.org/licenses/by-sa/3.0/" },
	{ IDI_SAVE,       L"Save Icon", L"16x16 Free Application Icons", L"Aha-Soft", L"CC Attribution-ShareAlike 3.0 Unported", L"https://creativecommons.org/licenses/by-sa/3.0/" },
	{ IDI_OPEN,       L"Open Icon", L"Shimmer Icons", L"Creative Freedom", L"CC Attribution-NoDerivs 3.0 Unported", L"https://creativecommons.org/licenses/by-nd/3.0/" },
	{ IDI_COPY,       L"Copy Icon", L"Mini Icons 2", L"Brand Spanking New", L"CC Attribution-ShareAlike 2.5 Generic", L"https://creativecommons.org/licenses/by-sa/2.5/" },
	{ IDI_UPFILE,     L"Move File Up Icon", L"Blue Bits Icons", L"Icojam", L"Freeware", L"" },
	{ IDI_DOWNFILE,   L"Move File Down Icon", L"Blue Bits Icons", L"Icojam", L"Freeware", L"" },
	{ IDI_RENAME,     L"Text Field Rename Icon", L"Web Design Icon Set", L"SEM Labs", L"Freeware", L"" },
	{ IDI_COPYFILE,   L"Copy Icon", L"Funktional Icons", L"Creative Freedom", L"CC Attribution-NoDerivs 3.0 Unported", L"https://creativecommons.org/licenses/by-nd/3.0/" },
	{ IDI_DELETEFILE, L"Delete Icon", L"Funktional Icons", L"Creative Freedom", L"CC Attribution-NoDerivs 3.0 Unported", L"https://creativecommons.org/licenses/by-nd/3.0/" },
	{ IDI_AUDIO,      L"Sound Icon", L"FatCow Hosting Icons", L"FatCow", L"CC Attribution 3.0 US", L"https://creativecommons.org/licenses/by/3.0/us/" },
	{ IDI_ROOTFS,     L"Actions Top Icon", L"Glaze Icons", L"Marco Martin", L"LGPL", L"http://www.gnu.org/licenses/lgpl-3.0.html" },
	{ IDI_REFRESH,    L"Refresh Icon", L"16x16 Free Application Icons", L"Aha-Soft", L"CC Attribution-ShareAlike 3.0 Unported", L"https://creativecommons.org/licenses/by-sa/3.0/" },
	{ IDI_BACK,       L"Back Icon", L"16x16 Free Application Icons", L"Aha-Soft", L"CC Attribution-ShareAlike 3.0 Unported", L"https://creativecommons.org/licenses/by-sa/3.0/" },
	{ IDI_BASIC,      L"Text Icon", L"Simply Icons 32px", L"Carlos Quintana", L"CC Attribution 3.0 Unported", L"https://creativecommons.org/licenses/by/3.0/" },
	{ IDI_CASSETTE,   L"Cassette Icon", L"Fugue 16px Icons", L"Yusuke Kamiyamane", L"CC Attribution 3.0 Unported", L"https://creativecommons.org/licenses/by/3.0/" },
	{ IDI_CHARMAP,    L"Apps Accessories Character Map Icon", L"Discovery Icon Theme", L"Hyike Bons", L"CC Attribution-ShareAlike 3.0 Unported", L"https://creativecommons.org/licenses/by-sa/3.0/" }
};

#define ICONICONCREDS ( sizeof(IconIconCreds) / sizeof(IconCredit ) )

void PlaceBitmapCredit( int x, int y, int i, int cpass )
{
	if ( cpass == 1 )
	{
		PlaceBitmap( BitmapIconCreds[ i ].BitmapResource, x, y, 32, 32, cpass );

		PlaceText( h2BFont, x + 36, y - 2, ( cw / 2 ) - 40, 18, BitmapIconCreds[ i ].IconName, cpass );
		PlaceText( hPFont, x + 36, y + 12, ( cw / 2 ) - 40, 14, BitmapIconCreds[ i ].IconSet + L" / " + BitmapIconCreds[ i ].IconAuthor, cpass );

		std::wstring url = BitmapIconCreds[ i ].URL;

		HFONT urlFont = hPFont;

		if ( url != L"" )
		{
			SetTextColor( CreditsDC, NormalLink );

			urlFont = hLFont;

			uRect r;

			r.left   = x + 36;
			r.right  = ( x + 36 + ( cw / 2 ) ) - 40;
			r.top    = y + 24;
			r.bottom = y + 24 + 14;
			r.url    = url;

			LinkRects.push_back( r );
		}

		PlaceText( urlFont, x + 36, y + 24, ( cw / 2 ) - 40, 14, BitmapIconCreds[ i ].License, cpass );

		SetTextColor( CreditsDC, 0 );
	}

	if ( ch < y + 40 )
	{
		ch = y + 40;
	}
}

void PlaceIconCredit( int x, int y, int i, int cpass )
{
	if ( cpass == 1 )
	{
		HICON hIcon = LoadIcon( hInst, MAKEINTRESOURCE( IconIconCreds[ i ].BitmapResource ) );

		DrawIconEx( CreditsDC, x + 8, y + 8, hIcon, 16, 16, 0, NULL, DI_NORMAL );

		PlaceText( h2BFont, x + 36, y - 2, ( cw / 2 ) - 40, 18, IconIconCreds[ i ].IconName, cpass );
		PlaceText( hPFont, x + 36, y + 12, ( cw / 2 ) - 40, 14, IconIconCreds[ i ].IconSet + L" / " + IconIconCreds[ i ].IconAuthor, cpass );

		std::wstring url = IconIconCreds[ i ].URL;

		HFONT urlFont = hPFont;

		if ( url != L"" )
		{
			SetTextColor( CreditsDC, NormalLink );

			urlFont = hLFont;

			uRect r;

			r.left   = x + 36;
			r.right  = ( x + 36 + ( cw / 2 ) ) - 40;
			r.top    = y + 24;
			r.bottom = y + 24 + 14;
			r.url    = url;

			LinkRects.push_back( r );
		}

		PlaceText( urlFont, x + 36, y + 24, ( cw / 2 ) - 40, 14, IconIconCreds[ i ].License, cpass );

		SetTextColor( CreditsDC, 0 );
	}

	if ( ch < y + 40 )
	{
		ch = y + 40;
	}
}

void CreditProvider( NUTSProvider *pProvider, int &sy, int cpass )
{
	PluginCommand cmd;

	NUTSPlugin *pPlugin = FSPlugins.GetPlugin( MAKEPROVID( pProvider->PluginID, 0 ) );

	if ( pPlugin == nullptr )
	{
		return;
	}

	cmd.CommandID = PC_ReportPluginCreditStats;
	cmd.InParams[ 0 ].Value = pProvider->ProviderID;

	WCHAR *pAuthor = nullptr;
	DWORD  Icons   = 0;
	DWORD  Thanks  = 0;

	if ( pPlugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
	{
		HBITMAP hLogo = (HBITMAP) cmd.OutParams[ 0 ].pPtr;

		PlaceBitmap( hLogo, 4, sy, 350, 64, cpass );

		sy += 68;

		pAuthor = (WCHAR *) cmd.OutParams[ 1 ].pPtr;
		Icons   = cmd.OutParams[ 2 ].Value;
		Thanks  = cmd.OutParams[ 3 ].Value;

		PlaceText( h2BFont, 4, sy, cw, 14, L"Writen by " + std::wstring( pAuthor ), cpass );
		
		sy += 20;
	}
	else
	{
		PlaceText( h1Font, 4, sy + 16, cw - 8, 64, pProvider->FriendlyName, cpass );

		sy += 68;
	}

	FSDescriptorList FSList = FSPlugins.GetFilesystems( pProvider->ProviderID );

	if ( FSList.size() > 0 )
	{
		PlaceText( h2BFont, 4, sy, cw - 8, 16, L"Supported FileSystems:", cpass ); sy += 18;

		int i = 0;

		FSDescriptor_iter iFS;

		for ( iFS = FSList.begin(); iFS != FSList.end(); iFS++ )
		{
			if ( i & 1 )
			{
				PlaceText( h3Font, 4 + ( ( cw -8 ) / 2 ), sy, ( cw - 8 ) / 2, 16, iFS->FriendlyName, cpass );

				sy += 18;
			}
			else
			{
				PlaceText( h3Font, 4, sy, ( cw - 8 ) / 2, 16, iFS->FriendlyName, cpass );
			}

			i++;
		}

		if ( i & 1 ) { sy += 18; }

		sy += 6;

		PlaceLine( 4, cw - 8, sy, cpass );

		sy += 4;
	}

	if ( ch < sy )
	{
		ch = sy;
	}

	TranslatorList TXList = FSPlugins.GetTranslators( pProvider->ProviderID, TXTextTranslator | TXGFXTranslator | TXAUDTranslator );

	if ( TXList.size() > 0 )
	{
		PlaceText( h2BFont, 4, sy, cw - 8, 16, L"Provided Translators:", cpass ); sy += 18;

		int i = 0;

		TranslatorIterator iTX;

		for ( iTX = TXList.begin(); iTX != TXList.end(); iTX++ )
		{
			if ( i & 1 )
			{
				PlaceText( h3Font, 4 + ( ( cw -8 ) / 2 ), sy, ( cw - 8 ) / 2, 16, iTX->FriendlyName, cpass );

				sy += 18;
			}
			else
			{
				PlaceText( h3Font, 4, sy, ( cw - 8 ) / 2, 16, iTX->FriendlyName, cpass );
			}

			i++;
		}

		if ( i & 1 ) { sy += 18; }

		sy += 6;

		PlaceLine( 4, cw - 8, sy, cpass );

		sy += 4;
	}

	if ( ch < sy )
	{
		ch = sy;
	}

	if ( Icons > 0 )
	{
		for ( DWORD i=0; i<Icons; i++ )
		{
			cmd.CommandID = PC_GetIconLicensing;
			cmd.InParams[ 0 ].Value = i;

			if ( pPlugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				int y = sy;
				int x = 4;

				if ( i & 1 ) { x += cw / 2; }

				if ( cpass == 1 )
				{
					PlaceBitmap( (HBITMAP) cmd.OutParams[ 1 ].pPtr, x, y, 32, 32, cpass );

					PlaceText( h2BFont, x + 36, y - 2, ( cw / 2 ) - 40, 18, std::wstring( (WCHAR *) cmd.OutParams[ 0 ].pPtr ), cpass );
					PlaceText( hPFont, x + 36, y + 12, ( cw / 2 ) - 40, 14, std::wstring( (WCHAR *) cmd.OutParams[ 2 ].pPtr ) + L" / " + std::wstring( (WCHAR *) cmd.OutParams[ 3 ].pPtr ), cpass );

					std::wstring url = std::wstring( (WCHAR *) cmd.OutParams[ 5 ].pPtr );

					HFONT urlFont = hPFont;

					if ( url != L"" )
					{
						SetTextColor( CreditsDC, NormalLink );

						urlFont = hLFont;

						if ( cpass == 1 )
						{
							uRect r;

							r.left   = x + 36;
							r.right  = x + 36 + ( cw / 2 );
							r.top    = y + 24;
							r.bottom = y + 24 + 14;
							r.url    = url;

							LinkRects.push_back( r );
						}
					}

					PlaceText( urlFont, x + 36, y + 24, ( cw / 2 ) - 40, 14, std::wstring( (WCHAR *) cmd.OutParams[ 4 ].pPtr ), cpass );

					SetTextColor( CreditsDC, 0 );
				}

				if ( i & 1 ) { sy += 40; }
			}
		}

		if ( Icons & 1 ) { sy += 40; }

		sy += 6;

		PlaceLine( 4, cw - 8, sy, cpass );

		sy += 4;
	}

	if ( ch < sy )
	{
		ch = sy;
	}

	if ( Thanks > 0 )
	{
		PlaceText( h2BFont, 4, sy, cw - 8, 16, L"With thanks to:", cpass ); sy += 18;

		for ( DWORD i=0; i<Thanks; i++ )
		{
			cmd.CommandID = PC_GetPluginCredits;
			cmd.InParams[ 0 ].Value = pProvider->ProviderID;
			cmd.InParams[ 1 ].Value = i;

			if ( pPlugin->CommandHandler( &cmd ) == NUTS_PLUGIN_SUCCESS )
			{
				WCHAR *pPerson = (WCHAR *) cmd.OutParams[ 0 ].pPtr;

				PlaceText( hPFont, 4, sy, cw - 16, 12, std::wstring( pPerson ), cpass ); sy += 14;
			}
		}

		sy += 6;

		PlaceLine( 4, cw - 8, sy, cpass );

		sy += 4;
	}

	if ( ch < sy )
	{
		ch = sy;
	}
}

void CreateCreditRoll( HWND hDlg )
{
	LinkRects.clear();

	RECT r;

	GetClientRect( hDlg, &r );

	cw   = r.right - r.left;
	maxh = r.bottom - r.top;

	ch   = 0;

	cw   -= 8;
	maxh -= 8;

	cw   -= 16; // Scroll bar

	for (cpass=0; cpass<2; cpass++ )
	{
		int rp = 8;

		// Do the things here
		
		PlaceBitmap( IDB_NUTSACORN, 8, rp, 64, 64, cpass );
		PlaceText( h1Font, 80, 4,  cw - 90, 48, L"N.U.T.S.", cpass );
		PlaceText( h2Font, 80, 36, cw - 90, 24, L"The Native Universal Transfer System", cpass );
		PlaceText( h3Font, 80, 56, cw - 90, 16, L"Written by Rebecca Gellman - (C) 2011 - 2021", cpass );

		PlaceLine( 4, cw - 8, 80, cpass );
		
		PlaceText( h2BFont, 4, 84, cw - 16, 16, L"Thanks are due to the following people:", cpass );

		rp = 100;

		for ( int p=0; p<CORE_PEOPLE; p++ )
		{
			PlaceText( hPFont, 4, rp, cw - 16, 12, std::wstring( People[ p ] + L": " + Reasons[ p ] ), cpass );

			rp += 14;
		}

		PlaceLine( 4, cw - 8, rp + 4, cpass ); rp += 6;

		PlaceText( h2BFont, 4, rp, cw - 16, 16, L"Icon Credits and Licensing:", cpass ); rp += 20;

		for ( int i=0; i<BITMAPICONCREDS; i++ )
		{
			if ( i & 1 )
			{
				PlaceBitmapCredit( 4 + ( cw / 2 ), rp, i, cpass ); rp += 40;
			}
			else
			{
				PlaceBitmapCredit( 4, rp, i, cpass );
			}
		}

		if ( BITMAPICONCREDS & 1 ) { rp += 40; }

		rp += 4;

		for ( int i=0; i<ICONICONCREDS; i++ )
		{
			if ( i & 1 )
			{
				PlaceIconCredit( 4 + ( cw / 2 ), rp, i, cpass ); rp += 40;
			}
			else
			{
				PlaceIconCredit( 4, rp, i, cpass );
			}
		}

		if ( ICONICONCREDS & 1 ) { rp += 40; }

		rp += 4;

		PlaceLine( 4, cw - 8, rp, cpass ); rp += 4;
		PlaceText( h1Font, 4, rp, cw - 8, 40, L"Loaded Plugins:", cpass ); rp += 36;

		NUTSProviderList provs = FSPlugins.GetProviders();

		NUTSProvider_iter iProv;

		for ( iProv = provs.begin(); iProv != provs.end(); iProv++ )
		{
			CreditProvider( &*iProv, rp, cpass );
		}

		if ( cpass == 0 )
		{
			ch = max( ch, maxh );
		}

		if ( CreditsDC == NULL )
		{
			HDC hDC = GetDC( hDlg );

			CreditsDC = CreateCompatibleDC( hDC );

			CreditsBitmap = CreateCompatibleBitmap( hDC, cw, ch );

			hOld = SelectObject( CreditsDC, CreditsBitmap );

			SelectObject( CreditsDC, GetStockObject( WHITE_BRUSH ) );

			Rectangle( CreditsDC, 0, 0, cw, ch );

			SetStretchBltMode( CreditsDC, 3 );
		}

		if ( maxh < ch )
		{
			SCROLLINFO si;

			si.cbSize = sizeof( SCROLLINFO );
			si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;
			si.nMin   = 0;
			si.nMax   = ( ch - maxh ) + 63;
			si.nPage  = 64;
			si.nPos   = 0;

			SetScrollInfo( hScroll, SB_CTL, &si, TRUE );

			EnableWindow( hScroll, TRUE );
		}
	}
}

void DoScroll(WPARAM wParam, LPARAM lParam)
{
	if (LOWORD(wParam) == SB_THUMBTRACK) {
		cc = HIWORD(wParam);

		SetScrollPos( hScroll, SB_CTL, cc, TRUE);
	}

	if (LOWORD(wParam) == SB_LINEUP) {
		if ( cc > 0 )
		{
			cc--;
		}

		SetScrollPos( hScroll, SB_CTL, cc, TRUE);
	}

	if (LOWORD(wParam) == SB_PAGEUP) {
		if ( cc > 4 )
		{
			cc -= 4;
		}
		else
		{
			cc = 0;
		}

		SetScrollPos( hScroll, SB_CTL, cc, TRUE);
	}

	if (LOWORD(wParam) == SB_LINEDOWN) {
		if ( cc < ( ch - maxh ) )
		{
			cc++;
		}

		SetScrollPos( hScroll, SB_CTL, cc, TRUE);
	}

	if (LOWORD(wParam) == SB_PAGEDOWN) {
		if ( cc < ( ch - maxh - 4 ) )
		{
			cc += 4;
		}
		else
		{
			cc = ch - maxh - 4;
		}

		SetScrollPos( hScroll, SB_CTL, cc, TRUE);
	}

	if (LOWORD(wParam) == SB_ENDSCROLL) {
		SetScrollPos( hScroll, SB_CTL, cc, TRUE);
	}
}

void DoMouseLink( UINT message, LPARAM lParam )
{
	DWORD mx = GET_X_LPARAM( lParam );
	DWORD my = GET_Y_LPARAM( lParam );

	bool WillHover = false;

	if ( ( mx > 4 ) && ( mx < ( cw + 4 ) ) )
	{
		if ( ( my > 4 ) && ( my < ( maxh + 4 ) ) )
		{
			mx -= 4;
			my -= 4;

			my += cc;

			std::vector<uRect>::iterator iR;

			for ( iR = LinkRects.begin(); iR != LinkRects.end(); iR++ )
			{
				if ( ( mx >= iR->left ) && ( mx <= iR->right ) && ( my >= iR->top ) && ( my <= iR->bottom ) )
				{
					if ( message == WM_MOUSEMOVE )
					{
						WillHover = true;
					}
					else if ( message == WM_LBUTTONUP )
					{
						ShellExecute( GetDesktopWindow(), 0, iR->url.c_str(), NULL, NULL, SW_SHOW );
					}
				}
			}
		}
	}

	if ( WillHover )
	{
		SetCursor( LoadCursor( NULL, IDC_HAND ) );
	}
	else
	{
		SetCursor( LoadCursor( NULL, IDC_ARROW ) );
	}
}

INT_PTR CALLBACK CreditsDlgFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch ( message )
	{
	case WM_INITDIALOG:
		if ( h1Font == NULL )
		{
			h1Font = CreateFont(36,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Arial"));

			h2Font = CreateFont(18,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Arial"));

			h2BFont = CreateFont(14,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Arial"));

			h3Font = CreateFont(14,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Arial"));

			hPFont = CreateFont(12,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Arial"));

			hLFont = CreateFont(12,0,0,0,FW_DONTCARE,FALSE,TRUE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Arial"));
		}

		{
			RECT r;

			GetClientRect( hDlg, &r );

			InvalidateRect( hDlg, &r, FALSE );
		}

		cc = 0;

 		return FALSE;

	case WM_VSCROLL:
		DoScroll( wParam, lParam );

		{
			RECT r;

			GetClientRect( hDlg, &r );

			InvalidateRect( hDlg, &r, FALSE );
		}

		return TRUE;

	case WM_ERASEBKGND:
		return FALSE;

	case WM_ABOUT_RESIZE:
		if ( hScroll == NULL )
		{
			RECT r;

			GetClientRect( hDlg, &r );

			cw   = r.right - r.left;
			maxh = r.bottom - r.top;

			hScroll = CreateWindowEx(
				NULL,
				L"SCROLLBAR", L"",
				WS_CHILD|WS_VISIBLE|SBS_VERT|WS_DISABLED,
					cw - 16, 0, 16, maxh,
				hDlg, NULL, hInst, NULL
			);
		}

		CreateCreditRoll( hDlg );

		{
			RECT r;

			GetClientRect( hDlg, &r );

			InvalidateRect( hDlg, &r, FALSE );
		}

		return FALSE;

	case WM_PAINT:
		{
			HDC hDC = GetDC( hDlg );

			BitBlt( hDC, 4, 4, cw, maxh, CreditsDC, 0, cc, SRCCOPY );

			RECT r;

			GetClientRect( hDlg, &r );

			RECT r2;

			r.right -= 16;

			r2 = r; r2.bottom = r2.top + 4;    FillRect( hDC, &r2, GetSysColorBrush( COLOR_BTNFACE ) );
			r2 = r; r2.top    = r2.bottom - 4; FillRect( hDC, &r2, GetSysColorBrush( COLOR_BTNFACE ) );
			r2 = r; r2.right  = r2.left + 4;   FillRect( hDC, &r2, GetSysColorBrush( COLOR_BTNFACE ) );
			r2 = r; r2.left   = r2.right - 4;  FillRect( hDC, &r2, GetSysColorBrush( COLOR_BTNFACE ) );

			ReleaseDC( hDlg, hDC );

			ValidateRect( hDlg, &r );

			::PostMessage( hScroll, WM_PAINT, 0, 0 );
		}
		return FALSE;

	case WM_DESTROY:
		NixWindow( hScroll );

		hScroll = NULL;

		break;

	case WM_MOUSEMOVE:
	case WM_LBUTTONUP:
		DoMouseLink( message, lParam );

		{
			RECT r;

			GetClientRect( hDlg, &r );

			InvalidateRect( hDlg, &r, FALSE );
		}

		break;

	case WM_MOUSEWHEEL:
		{
			long WheelDelta = 0 - (GET_WHEEL_DELTA_WPARAM(wParam) / 30);

			if ( ( cc > 0 ) && ( WheelDelta < 0 ) )
			{
				if ( abs(WheelDelta) < cc )
				{
					cc += WheelDelta;
				}
				else
				{
					cc = 0;
				}
			}

			if ( ( cc < ch - maxh ) && ( WheelDelta > 0 ) )
			{
				if ( ( ch - maxh - cc ) > WheelDelta )
				{
					cc += WheelDelta;
				}
				else
				{
					cc = ch - maxh - 4;
				}
			}

			SetScrollPos(hScroll, SB_CTL, cc, TRUE);

			{
				RECT r;

				GetClientRect( hDlg, &r );

				InvalidateRect( hDlg, &r, FALSE );
			}
		}

		return TRUE;
	}

	return FALSE;
}

