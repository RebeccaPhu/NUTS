#include "stdafx.h"
#include "PropsPage.h"
#include "AppAction.h"
#include "resource.h"

#include "PiePercent.h"
#include "BlockMap.h"
#include "EncodingEdit.h"
#include "BitmapCache.h"
#include "libfuncs.h"
#include "ExtensionRegistry.h"
#include "FileSystem.h"
#include "EncodingEdit.h"
#include "EncodingComboBox.h"
#include "Plugins.h"

#include "Defs.h"

#include <PrSht.h>
#include <CommCtrl.h>
#include <time.h>
#include <map>

HWND hSpaceWnd   = NULL;
HWND hBlocksWnd  = NULL;
HWND hAttrsWnd   = NULL;
HWND hFSAttrsWnd = NULL;
HWND hPropSheet  = NULL;
HWND hToolsWnd   = NULL;
HWND hToolDlg    = NULL;

HBRUSH MagBrush  = NULL;
HBRUSH BluBrush  = NULL;
HBRUSH RedBrush  = NULL;
HFONT  hFont     = NULL;
HFONT  hToolFont = NULL;

PiePercent FreeSpacePieChart;
BlockMap   BlockAllocations;

bool IsMultiProps;

HANDLE hSpaceThread;
HANDLE hToolThread;
HANDLE hToolWindowThread;
HANDLE hToolWindowEvent;
DWORD  dwthreadid;
BYTE   RunningTool;

typedef struct _AttrWnd {
	HWND PromptWnd;
	HWND ControlWnd;
	bool Changed;

	AttrDesc *SourceAttr;

	DWORD CurrentValue;
	DWORD StartingValue;
	BYTE  *pNewStringValue;

	EncodingEdit *pEditClass;
	EncodingComboBox *pComboClass;
} AttrWnd;

std::map<BYTE, AttrWnd> AttrWnds;
std::map<BYTE, AttrWnd> FSAttrWnds;

std::vector<HWND> RunButtons;

typedef std::map<BYTE, AttrWnd>::iterator AttrWnd_iter;

FileSystem *pHostFS;
FileSystem *pTargetFS;
EncodingEdit *pFilenameEdit = nullptr;

NativeFile *pFSPropsFile;

/* Lists of things allocated. There are many. */
std::vector<HWND> Windows;
std::vector<EncodingEdit *> Edits;
std::vector<EncodingComboBox *> Combos;

void DrawLayout()
{
	HDC hDC = GetDC( hSpaceWnd );

	/* Draw file icon, or blank if multiple */
	HDC hIconDC = CreateCompatibleDC( hDC );

	HGDIOBJ hOld;
	HBITMAP hIcon = BitmapCache.GetBitmap( FT_Arbitrary );

	bool CreatedIcon = false;

	if ( CurrentAction.Selection.size() == 1 )
	{
		hIcon = BitmapCache.GetBitmap( CurrentAction.Selection[ 0 ].Type );
	}

	DWORD rw = 34;
	DWORD rh = 34;
	DWORD iw = 34;
	DWORD ih = 34;

	IconDef     ResolvedIcon;
	NativeFile *pFile = nullptr;

	if ( ( pTargetFS == pHostFS ) && ( pHostFS->pParentFS != nullptr ) )
	{
		pFile        = &pHostFS->pParentFS->pDirectory->Files[ pHostFS->pParentFS->EnterIndex ];

		ResolvedIcon = pHostFS->pParentFS->pDirectory->ResolvedIcons[ pHostFS->pParentFS->EnterIndex ];
	}
	else
	{
		if ( CurrentAction.Selection.size() == 1 )
		{
			pFile        = &CurrentAction.Selection[ 0 ];

			ResolvedIcon = pHostFS->pDirectory->ResolvedIcons[ pFile->fileID ];
		}
		else
		{
			hIcon = BitmapCache.GetBitmap( FT_Arbitrary );
		}
	}

	if ( pFile != nullptr )
	{
		hIcon        = BitmapCache.GetBitmap( pFile->Icon );

		if ( pFile->HasResolvedIcon )
		{
			if ( pHostFS->pDirectory->ResolvedIcons.find( pFile->fileID ) != pHostFS->pDirectory->ResolvedIcons.end() )
			{
				iw = ResolvedIcon.bmi.biWidth;
				ih = ResolvedIcon.bmi.biHeight;

				double ratio = (double) ResolvedIcon.Aspect.second / (double) ResolvedIcon.Aspect.first;
				double ratio_icon = (double) ih / (double) iw;

				rw = (DWORD) ( 34 * ( ratio / ratio_icon ) );

				if ( rw > 100 )
				{
					rw = 34;
					rh = (DWORD) ( 34 * ( ratio_icon / ratio ) );
				}

				hIcon = CreateBitmap( iw, ih, ResolvedIcon.bmi.biPlanes, ResolvedIcon.bmi.biBitCount, ResolvedIcon.pImage );

				CreatedIcon = true;
			}
		}
	}

	hOld = SelectObject( hIconDC, hIcon );

	StretchBlt( hDC, 16, 28, rw, rh, hIconDC, 0, 0, iw, ih, SRCCOPY );

	/* Select out and delete the icon */
	SelectObject( hIconDC, hOld );

	DeleteDC( hIconDC );

	if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_SupportFreeSpace ) )
	{
		/* Draw the free space pie chart */
		BitBlt( hDC, 100, 300, 150, 60, FreeSpacePieChart.hPieDC, 0, 0, SRCCOPY );
	}

	/* Draw sunken free space keys */
	RECT rect;

	if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_SupportFreeSpace ) )
	{
		/* Used space */
		rect.left = 12;
		rect.right = rect.left + 16;
		rect.top = 166;
		rect.bottom = rect.top + 16;

		FillRect( hDC, &rect, BluBrush );
		DrawEdge( hDC, &rect, EDGE_SUNKEN, BF_RECT);

		/* Free space */
		rect.top += 22; rect.bottom += 22;

		FillRect( hDC, &rect, MagBrush );
		DrawEdge( hDC, &rect, EDGE_SUNKEN, BF_RECT);
	}

	/* Dividing lines */
	rect.left = 8;
	rect.right = 308;
	rect.top = 74;
	rect.bottom = 74;
	
	DrawEdge( hDC, &rect, EDGE_ETCHED, BF_BOTTOM );

	rect.top = 144;
	rect.bottom = 144;
	
	DrawEdge( hDC, &rect, EDGE_ETCHED, BF_BOTTOM );

	if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_SupportFreeSpace ) )
	{
		rect.top = 220;
		rect.bottom = 220;
	
		DrawEdge( hDC, &rect, EDGE_ETCHED, BF_BOTTOM );
	}

	/* All done */
	ReleaseDC( hSpaceWnd, hDC );

	/* Draw block map */
	if ( hBlocksWnd != NULL )
	{
		if ( BlockAllocations.hBlockDC )
		{
			hDC = GetDC( hBlocksWnd );

			BitBlt( hDC,
				20, 20, BlockAllocations.MapSizeW, BlockAllocations.MapSizeH,
				BlockAllocations.hBlockDC,
				0, 0,
				SRCCOPY
			);

			/* Draw legend */
			RECT r;
			r.left   = 20;
			r.right  = r.left + 12;
			r.top    = 368;
			r.bottom = r.top + 12;

			FillRect( hDC, &r, MagBrush );
			DrawEdge( hDC, &r, EDGE_SUNKEN, BF_RECT );

			r.left  += 104;
			r.right += 104;

			FillRect( hDC, &r, BluBrush );
			DrawEdge( hDC, &r, EDGE_SUNKEN, BF_RECT );

			r.left  += 94;
			r.right += 94;

			FillRect( hDC, &r, RedBrush );
			DrawEdge( hDC, &r, EDGE_SUNKEN, BF_RECT );

			ReleaseDC( hBlocksWnd, hDC );
		}
	}

	if ( CreatedIcon )
	{
		DeleteObject( hIcon );
	}
}

void DisplayNumber( HWND hDlgItem, UINT64 val, bool Human )
{
	WCHAR NumString[ 64 ];
	WCHAR OutString[ 64 ];

	WCHAR Humans[] = L" KMGTPEZY";

	UINT64 v;
	int    s;

	if ( !Human )
	{
		swprintf( NumString, 64, L"%llu", val );
	}
	else
	{
		s = 0;
		v = val;

		while ( ( v > 1024 ) && ( s < 9 ) )
		{
			v /= 1024;
			s++;
		}

		swprintf( NumString, 64, L"%llu", v );
	}

	GetNumberFormat(LOCALE_USER_DEFAULT, 0, NumString, NULL, OutString, 64 );  

	/* GetNumberFormat adds .00 to the end of this. Getting to not do so involves
	   arsing about with 400 GetLocaleInfo calls. Sod that. It's easier to just
	   remove the last 3 digits */

	std::wstring str( OutString );
	size_t p = str.find_last_of( L"." );

	if ( p != std::wstring::npos ) { str = str.substr( 0, p ); }

	if ( !Human )
	{
		swprintf( NumString, 64, L"%s bytes", str.c_str() );
	}
	else
	{
		swprintf( NumString, 64, L"%s %cB", str.c_str(), Humans[s] );
	}

	SendMessage( hDlgItem, WM_SETTEXT, 0, (LPARAM) NumString );
}

void ConfigureSpaceProps( HWND hwndDlg )
{
	hSpaceWnd = hwndDlg;

	/* Create brushes for the legend boxes */
	LOGBRUSH brsh;
	brsh.lbStyle = BS_SOLID;
	brsh.lbColor = RGB( 255, 0, 255 );

	if ( MagBrush == NULL ) { MagBrush = CreateBrushIndirect( &brsh ); }

	brsh.lbColor = RGB( 0, 0, 255 );

	if ( BluBrush == NULL ) { BluBrush = CreateBrushIndirect( &brsh ); }

	brsh.lbColor = RGB( 255, 0, 0 );

	if ( RedBrush == NULL ) { RedBrush = CreateBrushIndirect( &brsh ); }

	/* Filename edit box */
	pFilenameEdit = new EncodingEdit( hwndDlg, 70, 34, 232, true );

	if ( IsMultiProps )
	{
		pFilenameEdit->Disabled = true;
		pFilenameEdit->Encoding = pHostFS->GetEncoding();
	}
	else if ( CurrentAction.Selection.size() == 0 )
	{
		pFilenameEdit->Disabled = true;

		if ( pTargetFS->FSID != FS_Root )
		{
			pFilenameEdit->SetText( pTargetFS->pParentFS->pDirectory->Files[ pTargetFS->pParentFS->EnterIndex ].Filename );
			pFilenameEdit->Encoding = pTargetFS->pParentFS->GetEncoding();
		}
	}
	else
	{
		if ( CurrentAction.Selection[ 0 ].Flags && FF_Pseudo )
		{
			pFilenameEdit->Disabled = true;
		}
		else
		{
			pFilenameEdit->Disabled = false;
		}

		pFilenameEdit->SetText( CurrentAction.Selection[ 0 ].Filename );
		pFilenameEdit->Encoding = pHostFS->GetEncoding();
	}

	if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_SupportFreeSpace ) )
	{
		/* Pre-create this at 0. The thread will update this */
		FreeSpacePieChart.CreatePieChart( 0, hSpaceWnd );
	}

	if ( pTargetFS != nullptr )
	{
		std::wstring name = FSPlugins.FSName( pTargetFS->FSID );

		::SendMessage( GetDlgItem( hwndDlg, IDC_FSTYPE ), WM_SETTEXT, 0, (LPARAM) name.c_str() );
	}
	else
	{
		ShowWindow( GetDlgItem( hwndDlg, IDC_FSTYPE ), SW_HIDE );
		ShowWindow( GetDlgItem( hwndDlg, IDC_FSPROMPT ), SW_HIDE );
	}

	if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_SupportFreeSpace ) && (!IsMultiProps) )
	{
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_FREE ), SW_SHOW );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_FREE_BYTES ), SW_SHOW );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_FREE_HUMAN ), SW_SHOW );

		ShowWindow( GetDlgItem( hSpaceWnd, IDC_USED ), SW_SHOW );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_USED_BYTES ), SW_SHOW );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_USED_HUMAN ), SW_SHOW );
	}
	else
	{
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_FREE ), SW_HIDE );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_FREE_BYTES ), SW_HIDE );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_FREE_HUMAN ), SW_HIDE );

		ShowWindow( GetDlgItem( hSpaceWnd, IDC_USED ), SW_HIDE );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_USED_BYTES ), SW_HIDE );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_USED_HUMAN ), SW_HIDE );
	}

	if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_Capacity ) && (!IsMultiProps) )
	{
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_CAPACITY ), SW_SHOW );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_CAPACITY_BYTES ), SW_SHOW );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_CAPACITY_HUMAN ), SW_SHOW );
	}
	else
	{
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_CAPACITY ), SW_HIDE );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_CAPACITY_BYTES ), SW_HIDE );
		ShowWindow( GetDlgItem( hSpaceWnd, IDC_CAPACITY_HUMAN ), SW_HIDE );
	}

	if ( IsMultiProps )
	{
		std::vector<NativeFile>::iterator iFile;

		QWORD SizeBytes = 0ULL;

		for ( iFile = CurrentAction.Selection.begin(); iFile != CurrentAction.Selection.end(); iFile ++ )
		{
			SizeBytes += iFile->Length;
		}

		DisplayNumber( GetDlgItem( hSpaceWnd, IDC_SIZE_BYTES ), SizeBytes, false );
		DisplayNumber( GetDlgItem( hSpaceWnd, IDC_SIZE_HUMAN ), SizeBytes, true );

		if ( SizeBytes < 1024 )
		{
			ShowWindow( GetDlgItem( hSpaceWnd, IDC_SIZE_HUMAN ), SW_HIDE );
		}

		::SendMessage( GetDlgItem( hSpaceWnd, IDC_FILETYPE ), WM_SETTEXT, 0, (LPARAM) L"Multiple files" );
	}
	else
	{
		if ( CurrentAction.Selection.size() == 1 )
		{
			QWORD SizeBytes = CurrentAction.Selection[ 0 ].Length;

			DisplayNumber( GetDlgItem( hSpaceWnd, IDC_SIZE_BYTES ), SizeBytes, false );
			DisplayNumber( GetDlgItem( hSpaceWnd, IDC_SIZE_HUMAN ), SizeBytes, true );

			if ( SizeBytes < 1024ULL )
			{
				ShowWindow( GetDlgItem( hSpaceWnd, IDC_SIZE_HUMAN ), SW_HIDE );
			}

			::SendMessage( GetDlgItem( hSpaceWnd, IDC_FILETYPE ), WM_SETTEXT, 0, (LPARAM) pHostFS->Identify( CurrentAction.Selection[ 0 ].fileID ) );
		}
		else
		{
			if ( pHostFS->FSID != FS_Root )
			{
				NativeFile *pFile = &pHostFS->pParentFS->pDirectory->Files[ pHostFS->pParentFS->EnterIndex ];

				QWORD SizeBytes = pFile->Length;

				DisplayNumber( GetDlgItem( hSpaceWnd, IDC_SIZE_BYTES ), SizeBytes, false );
				DisplayNumber( GetDlgItem( hSpaceWnd, IDC_SIZE_HUMAN ), SizeBytes, true );

				if ( SizeBytes < 1024ULL )
				{
					ShowWindow( GetDlgItem( hSpaceWnd, IDC_SIZE_HUMAN ), SW_HIDE );
				}

				::SendMessage( GetDlgItem( hSpaceWnd, IDC_FILETYPE ), WM_SETTEXT, 0, (LPARAM) pHostFS->pParentFS->Identify( pFile->fileID ) );
			}
		}
	}
}

unsigned int __stdcall SpaceThread(void *param) {

	if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_SupportFreeSpace ) )
	{
		pTargetFS->CalculateSpaceUsage( hSpaceWnd, hBlocksWnd );
	}

	return 0;
}

void StopSpaceThread( void )
{
	pTargetFS->CancelSpace();

	if ( WaitForSingleObject( hSpaceThread, 500 ) != WAIT_OBJECT_0 )
	{
		TerminateThread( hSpaceThread, 500 );
	}

	CloseHandle( hSpaceThread );
}

INT_PTR CALLBACK FreeSpaceWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static WCHAR pc[4];

	switch ( uMsg )
	{

	case WM_CLOSE:
		StopSpaceThread();

		return TRUE;

	case WM_PAINT:
		DefWindowProc( hwndDlg, uMsg, wParam, lParam );

		DrawLayout();

		return TRUE;

	case WM_INITDIALOG:
		{
			ConfigureSpaceProps( hwndDlg );

			if ( hSpaceThread == NULL )
			{
				hSpaceThread = (HANDLE) _beginthreadex(NULL, NULL, SpaceThread, NULL, NULL, (unsigned int *) &dwthreadid);
			}
		}

		return FALSE;

	case WM_NOTIFY_FREE:
		{
			FSSpace *Map = (FSSpace *) wParam;

			DWORD FreeSpace = Map->Capacity - Map->UsedBytes;

			double p = ( (double) Map->UsedBytes / (double) Map->Capacity ) * 100.0;

			FreeSpacePieChart.CreatePieChart( (int) p, hSpaceWnd );

			DrawLayout();

			DisplayNumber( GetDlgItem( hSpaceWnd, IDC_CAPACITY_BYTES ), Map->Capacity, false );
			DisplayNumber( GetDlgItem( hSpaceWnd, IDC_CAPACITY_HUMAN ), Map->Capacity, true );

			DisplayNumber( GetDlgItem( hSpaceWnd, IDC_USED_BYTES ), Map->UsedBytes, false );
			DisplayNumber( GetDlgItem( hSpaceWnd, IDC_USED_HUMAN ), Map->UsedBytes, true );

			DisplayNumber( GetDlgItem( hSpaceWnd, IDC_FREE_BYTES ), FreeSpace, false );
			DisplayNumber( GetDlgItem( hSpaceWnd, IDC_FREE_HUMAN ), FreeSpace, true );

			BlockAllocations.CreateBlockMap( hSpaceWnd, Map->pBlockMap );
		}
	}

	return FALSE;
}

INT_PTR CALLBACK BlocksWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static WCHAR pc[4];

	switch ( uMsg )
	{

	case WM_PAINT:
		DefWindowProc( hwndDlg, uMsg, wParam, lParam );
		
		DrawLayout();

		return TRUE;

	case WM_INITDIALOG:
		{
			hBlocksWnd = hwndDlg;

			if ( hSpaceThread == NULL )
			{
				hSpaceThread = (HANDLE) _beginthreadex(NULL, NULL, SpaceThread, NULL, NULL, (unsigned int *) &dwthreadid);
			}

			DrawLayout();
		}
		return FALSE;

	case WM_CLOSE:
		{
			if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_SupportFreeSpace ) )
			{
				pTargetFS->CancelSpace();
			}
		}
		return FALSE;

	case WM_NOTIFY_FREE:
		{
			FSSpace *Map = (FSSpace *) wParam;

			BlockAllocations.CreateBlockMap( hSpaceWnd, Map->pBlockMap );

			DrawLayout();
		}
		
		return FALSE;
	}

	return FALSE;
}

bool AggregateValue( BYTE Index, DWORD *val )
{
	if ( CurrentAction.Selection.size() == 1 )
	{
		*val = CurrentAction.Selection[ 0 ].Attributes[ Index ];

		return true;
	}

	NativeFileIterator iFile;
	bool Result = true;

	for ( iFile = CurrentAction.Selection.begin(); iFile != CurrentAction.Selection.end(); iFile++ )
	{
		if ( iFile->fileID == 0 )
		{
			*val = iFile->Attributes[ Index ];
		}
		else
		{
			if ( iFile->Attributes[ Index ] != *val )
			{
				Result = false;

				*val = 0x88888888;
			}
		}
	}

	return Result;
}

void SetupFSAttributes( HWND hWnd, bool CreateWindows )
{
	if ( CreateWindows )
	{
		FSAttrWnds.clear();
	}

	static AttrDescriptors Attrs;
		
	Attrs = pTargetFS->GetFSAttributeDescriptions();

	if ( pTargetFS != nullptr )
	{
		AttrDesc_iter iAttr;

		DWORD CY = 20;
		BYTE  NumBuf[ 32 ];

		for ( iAttr = Attrs.begin(); iAttr != Attrs.end(); iAttr++ )
		{
			AttrWnd A;
			DWORD   val;

			A.pEditClass = nullptr;

			if ( !CreateWindows )
			{
				A = FSAttrWnds[ iAttr->Index ];
			}

			A.Changed    = false;
			A.SourceAttr = & (*iAttr);

			A.StartingValue = pFSPropsFile->Attributes[ iAttr->Index ];
			A.CurrentValue  = A.StartingValue;

			val = A.StartingValue;

			if ( ! (iAttr->Type & AttrVisible ) )
			{
				continue;
			}

			DWORD Styles = WS_VISIBLE | WS_CHILD;

			if ( ! (iAttr->Type & AttrEnabled ) )
			{
				Styles |= WS_DISABLED;
			}

			if ( CreateWindows )
			{
				A.PromptWnd  = CreateWindowEx( NULL, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, CY, 120, 20, hWnd, NULL, hInst, NULL );

				Windows.push_back( A.PromptWnd );
			}

			::SendMessage( A.PromptWnd, WM_SETFONT, (WPARAM) hFont, (LPARAM) TRUE );

			::SendMessage( A.PromptWnd, WM_SETTEXT, 0, (LPARAM) std::wstring( iAttr->Name + (std::wstring( L":") ) ).c_str() );

			if ( iAttr->Type & AttrNumeric )
			{
				EncodingEdit *pEdit;

				if ( CreateWindows )
				{
					pEdit = new EncodingEdit( hWnd, 150, CY - 2, 150, false ); 

					Edits.push_back( pEdit );

					A.ControlWnd = pEdit->hWnd;

					if ( ! ( iAttr->Type & AttrEnabled ) )
					{
						pEdit->Disabled = true;
					}

					A.pEditClass = pEdit;

					if ( iAttr->Type & AttrDec ) { pEdit->AllowedChars = EncodingEdit::textInputDec; }
					if ( iAttr->Type & AttrOct ) { pEdit->AllowedChars = EncodingEdit::textInputOct; }
					if ( iAttr->Type & AttrHex ) { pEdit->AllowedChars = EncodingEdit::textInputHex; }

					pEdit->MaxLen = 8;

					NumBuf[0] = 0;

					if ( AggregateValue( iAttr->Index, &val ) )
					{
						if ( iAttr->Type & AttrHex )
						{
							rsprintf( NumBuf, "%08X", val );
						}

						if ( iAttr->Type & AttrDec )
						{
							rsprintf( NumBuf, "%08d", val );
						}

						pEdit->SetText( NumBuf );
					}
				}
			}

			if ( iAttr->Type & AttrString )
			{
				EncodingEdit *pEdit;

				if ( CreateWindows )
				{
					pEdit = new EncodingEdit( hWnd, 150, CY - 2, 150, false ); 

					A.ControlWnd = pEdit->hWnd;

					Edits.push_back( pEdit );

					if ( ! ( iAttr->Type & AttrEnabled ) )
					{
						pEdit->Disabled = true;
					}

					A.pEditClass = pEdit;

					pEdit->MaxLen   = iAttr->MaxStringLength;
					pEdit->Encoding = pTargetFS->GetEncoding();
					pEdit->SetText( iAttr->pStringVal );
				}
				else
				{
					free( A.pNewStringValue );
				}

				A.pNewStringValue = rstrndup( iAttr->pStringVal, 256 );
			}

			if ( iAttr->Type & AttrBool )
			{
				if ( CreateWindows )
				{
					A.ControlWnd = CreateWindowEx( NULL, L"BUTTON", L"", Styles | BS_CHECKBOX, 150, CY, 150, 20, hWnd, NULL, hInst, NULL );

					Windows.push_back( A.ControlWnd );
				}

				if ( AggregateValue( iAttr->Index, &val ) )
				{
					if ( CurrentAction.Selection[ 0 ].Attributes[ iAttr->Index ] != 0U )
					{
						::SendMessage( A.ControlWnd, BM_SETCHECK, (WPARAM) BST_CHECKED, 0 );
					}
					else
					{
						::SendMessage( A.ControlWnd, BM_SETCHECK, (WPARAM) BST_UNCHECKED, 0 );
					}
				}
				else
				{
					::SendMessage( A.ControlWnd, BM_SETCHECK, (WPARAM) BST_INDETERMINATE, 0 );
				}
			}

			if ( ( iAttr->Type & AttrSelect ) || ( iAttr->Type & AttrCombo ) )
			{
				DWORD ExStyle = CBS_DROPDOWNLIST | WS_BORDER;

				if ( iAttr->Type & AttrCombo )
				{
					ExStyle = CBS_DROPDOWN | WS_BORDER;
				}

				if ( CreateWindows )
				{
					A.pComboClass = new EncodingComboBox( hWnd, 150, CY, 150 );

					Combos.push_back( A.pComboClass );

					A.ControlWnd = A.pComboClass->hWnd;

					if ( iAttr->Type & AttrDec ) { A.pComboClass->SetAllowed( EncodingEdit::textInputDec ); }
					if ( iAttr->Type & AttrOct ) { A.pComboClass->SetAllowed( EncodingEdit::textInputOct ); }
					if ( iAttr->Type & AttrHex ) { A.pComboClass->SetAllowed( EncodingEdit::textInputHex ); }

					if ( iAttr->Type & AttrCombo )
					{
						A.pComboClass->SetMaxLen( 8 );
					}
				}
				
				AttrOpt_iter iOption;
				BYTE         iIndex = 0;

				for ( iOption = iAttr->Options.begin(); iOption != iAttr->Options.end(); iOption++ )
				{
					A.pComboClass->AddTextItem( (BYTE *) AString( (WCHAR *) iOption->Name.c_str() ) );

					if ( val == iOption->EquivalentValue )
					{
						if ( iAttr->Type & AttrCombo )
						{
							if ( iAttr->Type & AttrHex )
							{
								rsprintf( NumBuf, "%08X", val );
							}

							if ( iAttr->Type & AttrDec )
							{
								rsprintf( NumBuf, "%08d", val );
							}

							A.pComboClass->SetSelText( NumBuf );
						}
						else
						{
							A.pComboClass->SetSel( iIndex );
						}
					}

					iIndex++;
				}

				if ( iAttr->Type & AttrCombo )
				{
					A.pComboClass->EnableCombo();
				}
			}

			if ( iAttr->Type & AttrTime )
			{
				DWORD ExStyle = WS_BORDER | DTS_SHORTDATEFORMAT | DTS_TIMEFORMAT;

				if ( CreateWindows )
				{
					A.ControlWnd = CreateWindowEx( NULL, DATETIMEPICK_CLASS, L"DateTime", Styles | ExStyle, 150, CY, 150, 20, hWnd, NULL, hInst, NULL );

					Windows.push_back( A.ControlWnd );

					::SendMessage( A.ControlWnd, DTM_SETFORMAT, 0, (LPARAM) L"dd/MM/yyyy hh:mm:ss" );

					time_t tTime     = time_t( A.StartingValue );
					struct tm *pTime = localtime( &tTime );
					SYSTEMTIME sTime;

					/* Ever hear of standards, Microsoft? */
					sTime.wYear   = pTime->tm_year + 1900;
					sTime.wMonth  = pTime->tm_mon + 1;
					sTime.wDay    = pTime->tm_mday;
					sTime.wHour   = pTime->tm_hour;
					sTime.wMinute = pTime->tm_min;
					sTime.wSecond = pTime->tm_sec;

					sTime.wMilliseconds = 0;

					::SendMessage( A.ControlWnd, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM) &sTime );
				}				
			}

			FSAttrWnds[ iAttr->Index ] = A;

			CY += 25;
		}
	}
}

bool FileDirValid( AttrDesc *pAttr )
{
	NativeFileIterator iFile;

	bool HasDirs = false;
	bool HasFiles = false;

	for ( iFile = CurrentAction.Selection.begin(); iFile != CurrentAction.Selection.end(); iFile++ )
	{
		if ( iFile->Flags & FF_Directory )
		{
			HasDirs = true;
		}
		else
		{
			HasFiles = true;
		}
	}

	if ( ( pAttr->Type & AttrDir ) && ( HasDirs ) )
	{
		return true;
	}

	if ( ( pAttr->Type & AttrFile ) && ( HasFiles ) )
	{
		return true;
	}

	return false;
}

void SetupAttributes( HWND hWnd, bool CreateWindows )
{
	if ( CreateWindows )
	{
		AttrWnds.clear();
	}

	static AttrDescriptors Attrs;
		
	Attrs = pHostFS->GetAttributeDescriptions();

	if ( ( CurrentAction.Selection.size() == 0 ) && ( pHostFS->FSID != FS_Root ) )
	{
		Attrs = pHostFS->pParentFS->GetAttributeDescriptions();
	}

	if ( pHostFS != nullptr )
	{
		AttrDesc_iter iAttr;

		DWORD CY = 20;
		BYTE  NumBuf[ 32 ];

		for ( iAttr = Attrs.begin(); iAttr != Attrs.end(); iAttr++ )
		{
			AttrWnd A;
			DWORD   val;

			A.pEditClass  = nullptr;
			A.pComboClass = nullptr;

			if ( !CreateWindows )
			{
				A = AttrWnds[ iAttr->Index ];
			}

			A.Changed    = false;
			A.SourceAttr = & (*iAttr);

			(void) AggregateValue( iAttr->Index, &A.StartingValue );

			A.CurrentValue = A.StartingValue;

			if ( ! (iAttr->Type & AttrVisible ) )
			{
				continue;
			}

			if ( !FileDirValid( &*iAttr ) )
			{
				continue;
			}

			DWORD Styles = WS_VISIBLE | WS_CHILD;

			if ( ! (iAttr->Type & AttrEnabled ) )
			{
				Styles |= WS_DISABLED;
			}

			if ( CreateWindows )
			{
				A.PromptWnd  = CreateWindowEx( NULL, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, CY, 120, 20, hWnd, NULL, hInst, NULL );

				Windows.push_back( A.PromptWnd );
			}

			::SendMessage( A.PromptWnd, WM_SETFONT, (WPARAM) hFont, (LPARAM) TRUE );

			::SendMessage( A.PromptWnd, WM_SETTEXT, 0, (LPARAM) std::wstring( iAttr->Name + (std::wstring( L":") ) ).c_str() );

			if ( iAttr->Type & AttrNumeric )
			{
				EncodingEdit *pEdit;

				if ( CreateWindows )
				{
					pEdit = new EncodingEdit( hWnd, 150, CY - 2, 150, false ); 

					Edits.push_back( pEdit );

					A.ControlWnd = pEdit->hWnd;

					if ( ! ( iAttr->Type & AttrEnabled ) )
					{
						pEdit->Disabled = true;
					}

					A.pEditClass = pEdit;

					if ( iAttr->Type & AttrDec ) { pEdit->AllowedChars = EncodingEdit::textInputDec; }
					if ( iAttr->Type & AttrOct ) { pEdit->AllowedChars = EncodingEdit::textInputOct; }
					if ( iAttr->Type & AttrHex ) { pEdit->AllowedChars = EncodingEdit::textInputHex; }

					pEdit->MaxLen = 8;

					NumBuf[0] = 0;

					if ( AggregateValue( iAttr->Index, &val ) )
					{
						if ( iAttr->Type & AttrHex )
						{
							rsprintf( NumBuf, "%08X", val );
						}

						if ( iAttr->Type & AttrDec )
						{
							rsprintf( NumBuf, "%08d", val );
						}

						pEdit->SetText( NumBuf );
					}
				}
			}

			if ( iAttr->Type & AttrBool )
			{
				if ( CreateWindows )
				{
					A.ControlWnd = CreateWindowEx( NULL, L"BUTTON", L"", Styles | BS_CHECKBOX, 150, CY, 150, 20, hWnd, NULL, hInst, NULL );

					Windows.push_back( A.ControlWnd );
				}

				if ( AggregateValue( iAttr->Index, &val ) )
				{
					if ( CurrentAction.Selection[ 0 ].Attributes[ iAttr->Index ] != 0U )
					{
						::SendMessage( A.ControlWnd, BM_SETCHECK, (WPARAM) BST_CHECKED, 0 );
					}
					else
					{
						::SendMessage( A.ControlWnd, BM_SETCHECK, (WPARAM) BST_UNCHECKED, 0 );
					}
				}
				else
				{
					::SendMessage( A.ControlWnd, BM_SETCHECK, (WPARAM) BST_INDETERMINATE, 0 );
				}
			}

			if ( ( iAttr->Type & AttrSelect ) || ( iAttr->Type & AttrCombo ) )
			{
				DWORD ExStyle = CBS_DROPDOWNLIST | WS_BORDER;

				if ( iAttr->Type & AttrCombo )
				{
					ExStyle = CBS_DROPDOWN | WS_BORDER;
				}

				if ( CreateWindows )
				{
					A.pComboClass = new EncodingComboBox( hWnd, 150, CY, 150 );

					Combos.push_back( A.pComboClass );

					A.ControlWnd = A.pComboClass->hWnd;

					if ( iAttr->Type & AttrDec ) { A.pComboClass->SetAllowed( EncodingEdit::textInputDec ); }
					if ( iAttr->Type & AttrOct ) { A.pComboClass->SetAllowed( EncodingEdit::textInputOct ); }
					if ( iAttr->Type & AttrHex ) { A.pComboClass->SetAllowed( EncodingEdit::textInputHex ); }

					if ( iAttr->Type & AttrCombo )
					{
						A.pComboClass->SetMaxLen( 8 );
					}
				}
				
				AttrOpt_iter iOption;
				BYTE         iIndex = 0;

				for ( iOption = iAttr->Options.begin(); iOption != iAttr->Options.end(); iOption++ )
				{
					A.pComboClass->AddTextItem( (BYTE *) AString( (WCHAR *) iOption->Name.c_str() ) );

					if ( AggregateValue( iAttr->Index, &val ) )
					{
						if ( val == iOption->EquivalentValue )
						{
							if ( iAttr->Type & AttrCombo )
							{
								if ( iAttr->Type & AttrHex )
								{
									rsprintf( NumBuf, "%08X", val );
								}

								if ( iAttr->Type & AttrDec )
								{
									rsprintf( NumBuf, "%08d", val );
								}

								A.pComboClass->SetSelText( NumBuf );
							}
							else
							{
								A.pComboClass->SetSel( iIndex );
							}
						}
					}

					iIndex++;
				}

				if ( iAttr->Type & AttrCombo )
				{
					A.pComboClass->EnableCombo();
				}
			}

			if ( iAttr->Type & AttrTime )
			{
				DWORD ExStyle = WS_BORDER;

				if ( CreateWindows )
				{
					A.ControlWnd = CreateWindowEx( NULL, DATETIMEPICK_CLASS, L"DateTime", Styles | ExStyle, 150, CY, 150, 20, hWnd, NULL, hInst, NULL );

					Windows.push_back( A.ControlWnd );

					::SendMessage( A.ControlWnd, DTM_SETFORMAT, 0, (LPARAM) L"dd/MM/yyyy hh:mm:ss" );

					time_t tTime     = time_t( A.StartingValue );
					struct tm *pTime = localtime( &tTime );
					SYSTEMTIME sTime;

					/* Ever hear of standards, Microsoft? */
					sTime.wYear   = pTime->tm_year + 1900;
					sTime.wMonth  = pTime->tm_mon + 1;
					sTime.wDay    = pTime->tm_mday;
					sTime.wHour   = pTime->tm_hour;
					sTime.wMinute = pTime->tm_min;
					sTime.wSecond = pTime->tm_sec;

					sTime.wMilliseconds = 0;

					::SendMessage( A.ControlWnd, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM) &sTime );
				}				
			}

			AttrWnds[ iAttr->Index ] = A;

			CY += 25;
		}
	}
}

void HandleAttributeMessage( HWND hSourceWnd, UINT uMsg, UINT v )
{
	AttrWnd_iter iAttr;

	if ( hSourceWnd == GetDlgItem( hAttrsWnd, IDC_RESET ) )
	{
		for ( iAttr = AttrWnds.begin(); iAttr != AttrWnds.end(); iAttr++ )
		{
			iAttr->second.Changed      = false;
			iAttr->second.CurrentValue = iAttr->second.StartingValue;
		}

		SetupAttributes( hAttrsWnd, false );

		PropSheet_UnChanged( hPropSheet, hAttrsWnd );

		return;
	}

	for ( iAttr = AttrWnds.begin(); iAttr != AttrWnds.end(); iAttr++ )
	{
		BYTE    Index     = iAttr->first;
		AttrWnd *pControl = &iAttr->second;
		bool SheetChanged = false;

		if ( hSourceWnd != pControl->ControlWnd )
		{
			continue;
		}

		switch ( uMsg )
		{
		case BN_CLICKED:
			if ( pControl->SourceAttr->Type & AttrBool )
			{
				if ( pControl->CurrentValue == 0U )
				{
					pControl->CurrentValue = 0xFFFFFFFF;

					::SendMessage( pControl->ControlWnd, BM_SETCHECK, (WPARAM) BST_CHECKED, 0 );
				}
				else
				{
					pControl->CurrentValue = 0x00000000;

					::SendMessage( pControl->ControlWnd, BM_SETCHECK, (WPARAM) BST_UNCHECKED, 0 );
				}
			}

			SheetChanged = true;

			break;

		case CBN_SELCHANGE:
			if ( ( pControl->SourceAttr->Type & AttrCombo ) || ( pControl->SourceAttr->Type & AttrSelect ) )
			{
				WORD SelIndex = (WORD) v;

				if ( pControl->SourceAttr->Type & AttrCombo )
				{
					BYTE NumBuf[ 32 ];

					if ( pControl->SourceAttr->Type & AttrHex )
					{
						rsprintf( NumBuf, "%08X", pControl->SourceAttr->Options[ SelIndex ].EquivalentValue );
					}

					if ( pControl->SourceAttr->Type & AttrDec )
					{
						rsprintf( NumBuf, "%d", pControl->SourceAttr->Options[ SelIndex ].EquivalentValue );
					}

					pControl->pComboClass->SetSelText( NumBuf );
				}
				else
				{
					pControl->CurrentValue = pControl->SourceAttr->Options[ SelIndex ].EquivalentValue;
				}
			}

			SheetChanged = true;

			break;

		case EN_CHANGE:
			if ( ( pControl->SourceAttr->Type & AttrNumeric ) || ( pControl->SourceAttr->Type & AttrCombo ) )
			{
				DWORD AttrVal = 0;

				if ( pControl->SourceAttr->Type & AttrHex ) { AttrVal = (DWORD) pControl->pEditClass->GetHexText(); }
				if ( pControl->SourceAttr->Type & AttrDec ) { AttrVal = (DWORD) pControl->pEditClass->GetDecText(); }
				if ( pControl->SourceAttr->Type & AttrOct ) { AttrVal = (DWORD) pControl->pEditClass->GetOctText(); }

				pControl->Changed      = true;
				pControl->CurrentValue = AttrVal;
			}

			SheetChanged = true;

			break;
		}

		if ( SheetChanged )
		{
			pControl->Changed = true;

			PropSheet_Changed( hPropSheet, hAttrsWnd );
		}
	}
}

void HandleAttributeNotify( WPARAM wParam, LPARAM lParam )
{
	if ( ((LPNMHDR)lParam)->code != DTN_DATETIMECHANGE )
	{
		return;
	}

	LPNMDATETIMECHANGE pChange = (LPNMDATETIMECHANGE) lParam;

	HWND hControl = pChange->nmhdr.hwndFrom;

	SYSTEMTIME *pTime = &pChange->st;

	AttrWnd_iter iAttr;

	for ( iAttr = AttrWnds.begin(); iAttr != AttrWnds.end(); iAttr++ )
	{
		if ( iAttr->second.SourceAttr->Type & AttrTime )
		{
			if ( hControl == iAttr->second.ControlWnd )
			{
				struct tm lTime;

				lTime.tm_year = pTime->wYear - 1900;
				lTime.tm_mon  = pTime->wMonth - 1;
				lTime.tm_mday = pTime->wDay;
				lTime.tm_hour = pTime->wHour;
				lTime.tm_min  = pTime->wMinute;
				lTime.tm_sec  = pTime->wSecond;

				time_t tTime = mktime( &lTime );

				iAttr->second.CurrentValue = (DWORD) tTime;
			}
		}
	}
}

INT_PTR CALLBACK AttrsWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static WCHAR pc[4];

	switch ( uMsg )
	{
		case WM_INITDIALOG:
			{
				hAttrsWnd = hwndDlg;

				if ( !hFont ) {
					hFont	= CreateFont( 16,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
				}

				SetupAttributes( hwndDlg, true );
			}
			break;

		case WM_COMMAND:
			{
				HandleAttributeMessage( (HWND) lParam, (UINT) HIWORD( wParam ), (UINT) LOWORD( wParam ) );

				return FALSE;
			}
			break;

		case WM_NOTIFY:
			{
				HandleAttributeNotify( wParam, lParam );
			}
			break;
	}

	return FALSE;
}

void HandleFSAttributeMessage( HWND hSourceWnd, UINT uMsg, UINT v )
{
	AttrWnd_iter iAttr;

	if ( hSourceWnd == GetDlgItem( hFSAttrsWnd, IDC_FSRESET ) )
	{
		for ( iAttr = FSAttrWnds.begin(); iAttr != FSAttrWnds.end(); iAttr++ )
		{
			iAttr->second.Changed      = false;
			iAttr->second.CurrentValue = iAttr->second.StartingValue;
		}

		SetupAttributes( hFSAttrsWnd, false );

		PropSheet_UnChanged( hPropSheet, hFSAttrsWnd );

		return;
	}

	for ( iAttr = FSAttrWnds.begin(); iAttr != FSAttrWnds.end(); iAttr++ )
	{
		BYTE    Index     = iAttr->first;
		AttrWnd *pControl = &iAttr->second;
		bool SheetChanged = false;

		if ( hSourceWnd != pControl->ControlWnd )
		{
			continue;
		}

		switch ( uMsg )
		{
		case BN_CLICKED:
			if ( pControl->SourceAttr->Type & AttrBool )
			{
				if ( pControl->CurrentValue == 0U )
				{
					pControl->CurrentValue = 0xFFFFFFFF;

					::SendMessage( pControl->ControlWnd, BM_SETCHECK, (WPARAM) BST_CHECKED, 0 );
				}
				else
				{
					pControl->CurrentValue = 0x00000000;

					::SendMessage( pControl->ControlWnd, BM_SETCHECK, (WPARAM) BST_UNCHECKED, 0 );
				}
			}

			SheetChanged = true;

			break;

		case CBN_SELCHANGE:
			if ( ( pControl->SourceAttr->Type & AttrCombo ) || ( pControl->SourceAttr->Type & AttrSelect ) )
			{
				WORD SelIndex = (WORD) v;

				if ( pControl->SourceAttr->Type & AttrCombo )
				{
					BYTE NumBuf[ 32 ];

					if ( pControl->SourceAttr->Type & AttrHex )
					{
						rsprintf( NumBuf, "%08X", pControl->SourceAttr->Options[ SelIndex ].EquivalentValue );
					}

					if ( pControl->SourceAttr->Type & AttrDec )
					{
						rsprintf( NumBuf, "%d", pControl->SourceAttr->Options[ SelIndex ].EquivalentValue );
					}

					pControl->pComboClass->SetSelText( NumBuf );
				}
				else
				{
					pControl->CurrentValue = pControl->SourceAttr->Options[ SelIndex ].EquivalentValue;
				}
			}

			SheetChanged = true;

			break;

		case EN_CHANGE:
			if ( ( pControl->SourceAttr->Type & AttrNumeric ) || ( pControl->SourceAttr->Type & AttrCombo ) )
			{
				DWORD AttrVal = 0;

				if ( pControl->SourceAttr->Type & AttrHex ) { AttrVal = (DWORD) pControl->pEditClass->GetHexText(); }
				if ( pControl->SourceAttr->Type & AttrDec ) { AttrVal = (DWORD) pControl->pEditClass->GetDecText(); }
				if ( pControl->SourceAttr->Type & AttrOct ) { AttrVal = (DWORD) pControl->pEditClass->GetOctText(); }

				pControl->Changed      = true;
				pControl->CurrentValue = AttrVal;
			}

			if ( pControl->SourceAttr->Type & AttrString )
			{
				free( pControl->pNewStringValue );

				pControl->pNewStringValue = rstrndup( pControl->pEditClass->GetText(), pControl->SourceAttr->MaxStringLength );
			}

			SheetChanged = true;

			break;
		}

		if ( SheetChanged )
		{
			pControl->Changed = true;

			PropSheet_Changed( hPropSheet, hFSAttrsWnd );
		}
	}
}

void HandleFSAttributeNotify( WPARAM wParam, LPARAM lParam )
{
	if ( ((LPNMHDR)lParam)->code != DTN_DATETIMECHANGE )
	{
		return;
	}

	LPNMDATETIMECHANGE pChange = (LPNMDATETIMECHANGE) lParam;

	HWND hControl = pChange->nmhdr.hwndFrom;

	SYSTEMTIME *pTime = &pChange->st;

	AttrWnd_iter iAttr;

	for ( iAttr = FSAttrWnds.begin(); iAttr != FSAttrWnds.end(); iAttr++ )
	{
		if ( iAttr->second.SourceAttr->Type & AttrTime )
		{
			if ( hControl == iAttr->second.ControlWnd )
			{
				struct tm lTime;

				lTime.tm_year = pTime->wYear - 1900;
				lTime.tm_mon  = pTime->wMonth - 1;
				lTime.tm_mday = pTime->wDay;
				lTime.tm_hour = pTime->wHour;
				lTime.tm_min  = pTime->wMinute;
				lTime.tm_sec  = pTime->wSecond;

				time_t tTime = mktime( &lTime );

				iAttr->second.CurrentValue = (DWORD) tTime;
			}
		}
	}
}

INT_PTR CALLBACK FSAttrsWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			{
				hFSAttrsWnd = hwndDlg;

				if ( !hFont ) {
					hFont	= CreateFont( 16,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
				}

				SetupFSAttributes( hwndDlg, true );
			}
			break;

		case WM_COMMAND:
			{
				HandleFSAttributeMessage( (HWND) lParam, (UINT) HIWORD( wParam ), (UINT) LOWORD( wParam ) );

				return FALSE;
			}
			break;

		case WM_NOTIFY:
			{
				HandleFSAttributeNotify( wParam, lParam );
			}
			break;
	}

	return FALSE;
}

void SetupToolList( void )
{
	FSToolList tools = pTargetFS->GetToolsList();

	FSToolIterator iTool;

	DWORD CY = 16;

	for ( iTool = tools.begin(); iTool != tools.end(); iTool++ )
	{
		HWND hFrame = CreateWindowEx(
			NULL,
			L"BUTTON",
			iTool->ToolName.c_str(),
			WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
			8, CY, 300, 64,
			hToolsWnd, NULL, hInst, NULL
		);

		HWND hToolIcon = CreateWindowEx(
			NULL,
			L"STATIC",
			L"",
			WS_VISIBLE | WS_CHILD | SS_BITMAP,
			14, 20 + CY, 34, 34,
			hToolsWnd, NULL, hInst, NULL
		);

		HWND hToolDesc = CreateWindowEx(
			NULL,
			L"STATIC",
			iTool->ToolDesc.c_str(),
			WS_VISIBLE | WS_CHILD,
			54, 18 + CY, 204, 42,
			hToolsWnd, NULL, hInst, NULL
		);

		HWND hRunButton = CreateWindowEx(
			NULL,
			L"BUTTON",
			L"Run",
			WS_VISIBLE | WS_CHILD,
			262, 24 + CY, 40, 24,
			hToolsWnd, NULL, hInst, NULL
		);

		Windows.push_back( hFrame );
		Windows.push_back( hToolIcon );
		Windows.push_back( hToolDesc );
		Windows.push_back( hRunButton );

		RunButtons.push_back( hRunButton);

		CY += 72;

		::SendMessage( hFrame,     WM_SETFONT,   (WPARAM) hFont,        (LPARAM) TRUE );
		::SendMessage( hRunButton, WM_SETFONT,   (WPARAM) hFont,        (LPARAM) TRUE );
		::SendMessage( hToolDesc,  WM_SETFONT,   (WPARAM) hToolFont,    (LPARAM) TRUE );
		::SendMessage( hToolIcon,  STM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) iTool->ToolIcon );
	}
}

INT_PTR CALLBACK ToolDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch ( message )
	{
	case WM_INITDIALOG:
		{
			hToolDlg = hDlg;

			FSToolList tools = pTargetFS->GetToolsList();

			::SendMessage( GetDlgItem( hDlg, IDC_TOOL_ICON ), STM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) tools[ RunningTool ].ToolIcon );
			::SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) tools[ RunningTool ].ToolName.c_str() );

			SetEvent( hToolWindowEvent );
		}
		break;

	case WM_COMMAND:
		{
			if ( LOWORD(wParam) == IDCANCEL )
			{
				pTargetFS->CancelTool();

				if ( hToolThread )
				{
					if ( WaitForSingleObject( hToolThread, 2000 ) == WAIT_TIMEOUT )
					{
						TerminateThread( hToolThread, 500 );
					}

					CloseHandle( hToolThread );
				}

				EndDialog( hDlg, 0 );
			}
		}
		break;

	case WM_FSTOOL_SETDESC:
		::SendMessage( GetDlgItem( hDlg, IDC_TOOL_DESC ), WM_SETTEXT, 0, lParam );
		break;

	case WM_FSTOOL_PROGLIMIT:
		::SendMessage( GetDlgItem( hDlg, IDC_TOOL_PROGRESS ), PBM_SETRANGE32, wParam, lParam );
		break;

	case WM_FSTOOL_PROGRESS:
		::SendMessage( GetDlgItem(hwndDlg, IDC_TOOL_PROGRESS), PBM_SETPOS, wParam, 0 );
		break;

	}

	return FALSE;
}

unsigned int __stdcall ToolThread(void *param) {

	if ( WaitForSingleObject( hToolWindowEvent, 5000 ) == WAIT_TIMEOUT )
	{
		MessageBox( hToolsWnd, L"The tool could not be started. NUTS may be in an unstable state. Restarting the application is recommended.", L"Tool Launch Failure", MB_ICONEXCLAMATION | MB_OK );

		return 0;
	}

	if ( pTargetFS != nullptr )
	{
		pTargetFS->RunTool( RunningTool, hToolDlg );

		CloseHandle( hToolThread );

		hToolThread = NULL;
	}

	return 0;
}

void DoTool( BYTE t )
{
	hToolWindowEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	RunningTool = t;

	/* This sets up a window */
	if ( pTargetFS->PreRunTool( t ) != USE_CUSTOM_WND )
	{
		hToolThread = (HANDLE) _beginthreadex(NULL, NULL, ToolThread, NULL, NULL, (unsigned int *) &dwthreadid);

		DialogBox( hInst, MAKEINTRESOURCE( IDD_TOOL_WINDOW ), hToolsWnd, ToolDialogProc );
	}
	else
	{
		hToolDlg = NULL;

		pTargetFS->RunTool( t, NULL );
	}
}

INT_PTR CALLBACK ToolsWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			{
				hToolsWnd = hwndDlg;

				if ( !hToolFont ) {
					hToolFont = CreateFont( 12,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
				}

				if ( !hFont ) {
					hFont = CreateFont( 16,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
				}

				SetupToolList();
			}
			break;

		case WM_COMMAND:
			{
				BYTE t = 0;

				for ( std::vector<HWND>::iterator i=RunButtons.begin(); i!=RunButtons.end(); i++ )
				{
					if ( *i == (HWND) lParam )
					{
						DoTool( t );
					}

					t++;
				}
			}
			break;

	}

	return FALSE;
}

int CALLBACK PropsPageProc( HWND hwndDlg, UINT uMsg, LPARAM lParam )
{
	hPropSheet = hwndDlg;

	return 0;
}

FileSystem *GetFSHandler()
{
	void *Ptr = pHostFS->FileFilesystem( CurrentAction.Selection[ 0 ].fileID );

	if ( Ptr == nullptr )
	{
		// It didn't return a file system. Guess we'll ask the plugin handler.
		DataSource *pSource = pHostFS->FileDataSource( CurrentAction.Selection[ 0 ].fileID );

		Ptr = FSPlugins.FindAndLoadFS( pSource, &CurrentAction.Selection[ 0 ] );
	}

	return (FileSystem *) Ptr;
}

int PropsPage_Handler( AppAction Action )
{
	CurrentAction = Action;

	pHostFS = (FileSystem *) CurrentAction.FS;

	IsMultiProps = false;

	if ( Action.Selection.size() == 0 )
	{
		IsMultiProps = false;

		pTargetFS = (FileSystem *) Action.FS;
	}
	else if ( Action.Selection.size() == 1 )
	{
		IsMultiProps = false;

		pTargetFS    = nullptr;

		if ( Action.Selection[ 0 ].Type == FT_MiscImage )
		{
			pTargetFS = GetFSHandler();

			if ( pTargetFS != nullptr )
			{
				pTargetFS->Init();
			}
		}
	}
	else
	{
		IsMultiProps = true;
	}

	hSpaceThread = NULL;
	hBlocksWnd   = NULL;
	hSpaceWnd    = NULL;

	PROPSHEETPAGE psp[ 5 ];

	int NumPages = 1;

	AttrWnds.clear();
	FSAttrWnds.clear();

	PROPSHEETHEADER psh;

	psp[0].dwSize      = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags     = PSP_USETITLE;
	psp[0].hInstance   = hInst;
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_SPACE_DIALOG);
	psp[0].pszIcon     = 0;
	psp[0].pfnDlgProc  = FreeSpaceWindowProc;
	psp[0].lParam      = 0;
	psp[0].pfnCallback = NULL;

	if ( pTargetFS != nullptr )
	{
		if ( ( pTargetFS->Flags & FSF_SupportFreeSpace ) && ( pTargetFS->Flags & FSF_Size ) )
		{
			psp[0].pszTitle = (LPWSTR) L"Free Space and File Size";
		}
		else if ( pTargetFS->Flags & FSF_SupportFreeSpace )
		{
			psp[0].pszTitle = (LPWSTR) L"Free Space";
		}
		else if ( pTargetFS->Flags & FSF_Size )
		{
			psp[0].pszTitle = (LPWSTR) L"File Size";
		}
	}
	else
	{
		psp[0].pszTitle = (LPWSTR) L"File Size";
	}

	if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_SupportBlocks ) )
	{
		psp[1].dwSize      = sizeof(PROPSHEETPAGE);
		psp[1].dwFlags     = PSP_USETITLE;
		psp[1].hInstance   = hInst;
		psp[1].pszTemplate = MAKEINTRESOURCE(IDD_BLOCKS_DIALOG);
		psp[1].pszIcon     = 0;
		psp[1].pfnDlgProc  = BlocksWindowProc;
		psp[1].pszTitle    = (LPWSTR) L"Block Usage";
		psp[1].lParam      = 0;
		psp[1].pfnCallback = NULL;

		NumPages++;
	}
    
	psp[ NumPages ].dwSize      = sizeof(PROPSHEETPAGE);
	psp[ NumPages ].dwFlags     = PSP_USETITLE;
	psp[ NumPages ].hInstance   = hInst;
	psp[ NumPages ].pszTemplate = MAKEINTRESOURCE(IDD_ATTRS);
	psp[ NumPages ].pszIcon     = 0;
	psp[ NumPages ].pfnDlgProc  = AttrsWindowProc;
	psp[ NumPages ].pszTitle    = (LPWSTR) L"File Attributes";
	psp[ NumPages ].lParam      = 0;
	psp[ NumPages ].pfnCallback = NULL;

	NumPages++;

	bool FSPropsPage = false;

	if ( Action.Selection.size() == 0 )
	{
		FSPropsPage = true;

		pFSPropsFile = &pHostFS->pParentFS->pDirectory->Files[ pHostFS->pParentFS->EnterIndex ];
	}
	else
	{
		if ( Action.Selection.size() == 1 )
		{
			if ( Action.Selection[ 0 ].Type == FT_MiscImage )
			{
				FSPropsPage = true;

				pFSPropsFile = &Action.Selection[ 0 ];
			}
		}
	}

	if ( FSPropsPage )
	{
		psp[ NumPages ].dwSize      = sizeof(PROPSHEETPAGE);
		psp[ NumPages ].dwFlags     = PSP_USETITLE;
		psp[ NumPages ].hInstance   = hInst;
		psp[ NumPages ].pszTemplate = MAKEINTRESOURCE(IDD_FSATTRS);
		psp[ NumPages ].pszIcon     = 0;
		psp[ NumPages ].pfnDlgProc  = FSAttrsWindowProc;
		psp[ NumPages ].pszTitle    = (LPWSTR) L"File System Attributes";
		psp[ NumPages ].lParam      = 0;
		psp[ NumPages ].pfnCallback = NULL;

		NumPages++;
		
		FSToolList tools = pTargetFS->GetToolsList();

		if ( tools.size() > 0 )
		{
			psp[ NumPages ].dwSize      = sizeof(PROPSHEETPAGE);
			psp[ NumPages ].dwFlags     = PSP_USETITLE;
			psp[ NumPages ].hInstance   = hInst;
			psp[ NumPages ].pszTemplate = MAKEINTRESOURCE(IDD_TOOLS);
			psp[ NumPages ].pszIcon     = 0;
			psp[ NumPages ].pfnDlgProc  = ToolsWindowProc;
			psp[ NumPages ].pszTitle    = (LPWSTR) L"Tools";
			psp[ NumPages ].lParam      = 0;
			psp[ NumPages ].pfnCallback = NULL;

			NumPages++;
		}
	}
    
	psh.dwSize      = sizeof(PROPSHEETHEADER);
	psh.dwFlags     = PSH_USEICONID | PSH_PROPSHEETPAGE | PSH_USECALLBACK;
	psh.hwndParent  = Action.hWnd;
	psh.hInstance   = hInst;
	psh.pszIcon     = MAKEINTRESOURCE(IDI_SMALL);
	psh.pszCaption  = (LPWSTR) L"Properties";
	psh.nPages      = NumPages;
	psh.nStartPage  = 0;
	psh.ppsp        = (LPCPROPSHEETPAGE) &psp;
	psh.pfnCallback = PropsPageProc;
	
	if ( PropertySheet( &psh ) >= 1 )
	{
		AttrWnd_iter iAttr;

		for ( iAttr = FSAttrWnds.begin(); iAttr != FSAttrWnds.end(); iAttr++ )
		{
			if ( iAttr->second.Changed )
			{
				pTargetFS->SetFSProp( iAttr->first, iAttr->second.CurrentValue, iAttr->second.pNewStringValue );

				if ( iAttr->second.SourceAttr->Type & AttrString )
				{
					free( iAttr->second.pNewStringValue );
				}
			}
		}

		static std::map<DWORD, DWORD> Changes;

		for ( iAttr = AttrWnds.begin(); iAttr != AttrWnds.end(); iAttr++ )
		{
			if ( iAttr->second.Changed )
			{
				Changes[ iAttr->first ] = iAttr->second.CurrentValue;
			}
		}

		if ( Changes.size() > 0 )
		{
			AppAction Action;

			Action.Action    = AA_SET_PROPS;
			Action.pData     = (void *) &Changes;
			Action.FS        = CurrentAction.FS;
			Action.hWnd      = CurrentAction.hWnd;
			Action.Pane      = CurrentAction.Pane;
			Action.Selection = CurrentAction.Selection;

			QueueAction( Action );
		}
	}

	/* Free the source strings */
	AttrWnd_iter iAttr;

	for ( iAttr = FSAttrWnds.begin(); iAttr != FSAttrWnds.end(); iAttr++ )
	{
		if ( iAttr->second.SourceAttr->Type & AttrString )
		{
			free( iAttr->second.SourceAttr->pStringVal );
		}
	}

	/* Delete objects */
	for ( std::vector<HWND>::iterator i=Windows.begin(); i != Windows.end(); i++ ) { DestroyWindow( *i ); }
	for ( std::vector<EncodingEdit *>::iterator i=Edits.begin(); i != Edits.end(); i++ ) { delete *i; }
	for ( std::vector<EncodingComboBox *>::iterator i=Combos.begin(); i != Combos.end(); i++ ) { delete *i; }

	Windows.clear();
	Edits.clear();
	Combos.clear();
	
	RunButtons.clear();

	return 0;
}