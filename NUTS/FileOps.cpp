#include "stdafx.h"
#include "AppAction.h"
#include "FileOps.h"
#include "FontBitmap.h"
#include "Defs.h"
#include "BitmapCache.h"
#include "Preference.h"
#include "Plugins.h"

#include <CommCtrl.h>
#include "resource.h"

#define FNTop     46
#define FNLeft    65
#define FNRight   301
#define FNBottom  62
#define FNHeight ( FNBottom - FNTop )
#define FNWidth  ( FNRight - FNLeft )

#define XtraHeight 64

typedef enum _FileOp {
	Op_Delete     = 1,
	Op_Copy       = 2,
	Op_Directory  = 3,
	Op_Parent     = 4,
	Op_Refresh    = 5,
	Op_Set_Props  = 6,
	Op_Enter_FS   = 7,
	Op_Leave_FS   = 8,
	Op_CDirectory = 9,
	Op_CParent    = 10,
} FileOp;

typedef struct _FileOpStep {
	FileOp     Step;
	NativeFile Object;
	FileSystem *FS;
} FileOpStep;

HDC     hIconDC     = NULL;
HDC     hNameDC     = NULL;
HBITMAP hNameBMP    = NULL;
HWND    hFileWnd    = NULL;
HANDLE  hFileThread = NULL;
HANDLE  hFileCancel = NULL;
bool    Confirm     = true;
bool    YesToAll    = false;
bool    YesOnce     = false;
bool    NoOnce      = false;
bool    NoToAll     = false;
bool    Paused      = false;

static DWORD dwthreadid  = 0;

std::vector<FileOpStep> OpSteps;

DWORD TotalOps;
DWORD CurrentOp;

CRITICAL_SECTION RedrawLock;

NativeFile CurrentObject;

AttrDescriptors Attrs;
std::map<DWORD, DWORD> Changes;

FileSystem *SaveFS;

void CreateOpSteps( std::vector<NativeFile> Selection )
{
	CFileViewer *pPane    = (CFileViewer *) CurrentAction.Pane;
	FileSystem *pSourceFS = (FileSystem *) CurrentAction.FS;

	Attrs   = pSourceFS->GetAttributeDescriptions();

	if ( CurrentAction.Action == AA_SET_PROPS )
	{
		Changes = * (std::map<DWORD, DWORD> *) CurrentAction.pData;
	}

	/* Find directories first */

	std::vector<NativeFile>::iterator iFile;

	for ( iFile = Selection.begin(); iFile != Selection.end(); iFile++ )
	{
		if ( iFile->Flags & FF_Directory )
		{
			/* Found a directory. Recurse it. */
			FileOpStep step;

			step.Object = *iFile;
			step.Step   = Op_Directory;

			OpSteps.push_back( step );

			TotalOps++;

			pSourceFS->ChangeDirectory( iFile->fileID );

			CreateOpSteps( pSourceFS->pDirectory->Files );

			FileOpStep ParentStep;
			
			ParentStep.Step = Op_Parent;

			OpSteps.push_back( ParentStep );

			pSourceFS->Parent();
		}
	}

	/* Re-iterate the selection and add files */
	for ( iFile = Selection.begin(); iFile != Selection.end(); iFile++ )
	{
		if ( ( ! (iFile->Flags & FF_Directory) ) || ( CurrentAction.Action == AA_DELETE ) || ( CurrentAction.Action == AA_SET_PROPS ) )
		{
			FileOpStep step;

			step.Object = *iFile;
			
			if ( CurrentAction.Action == AA_COPY )
			{
				step.Step = Op_Copy;
			}
			else if ( CurrentAction.Action == AA_INSTALL )
			{
				step.Step = Op_Copy;
			}
			else if ( CurrentAction.Action == AA_DELETE )
			{
				step.Step = Op_Delete;
			}
			else if ( CurrentAction.Action == AA_SET_PROPS )
			{
				step.Step = Op_Set_Props;
			}

			OpSteps.push_back( step );

			TotalOps++;

			FileOpStep Final;

			Final.Step = Op_Refresh;
			OpSteps.push_back( Final );

			TotalOps++;
		}
	}
}

void CreateOpStepsByFS( std::vector<NativeFile> Selection )
{
	/* This assumes each item in the selection is a file system that
	   can be copied. If it is not, just skip the item. */

	NativeFileIterator iFile;

	for ( iFile = Selection.begin(); iFile != Selection.end(); iFile++ )
	{
		FileOpStep step;

		CFileViewer *pPane = (CFileViewer *) CurrentAction.Pane;

		FileSystem *pFS = pPane->FS->FileFilesystem( iFile->fileID );

		if ( pFS != nullptr )
		{
			step.Object = *iFile;
			step.Step   = Op_CDirectory;

			OpSteps.push_back( step );

			step.Step = Op_Refresh;

			OpSteps.push_back( step );

			step.Step = Op_Enter_FS;
			step.FS   = pFS;

			OpSteps.push_back( step );

			pFS->Init();

			pFS->pParentFS = pPane->FS;

			SaveFS = (FileSystem *) CurrentAction.FS;

			CurrentAction.FS = pFS;

			CreateOpSteps( pFS->pDirectory->Files );

			CurrentAction.FS = SaveFS;

			step.Step = Op_Leave_FS;

			OpSteps.push_back( step );

			step.Step = Op_CParent;

			OpSteps.push_back( step );

			step.Step = Op_Refresh;

			OpSteps.push_back( step );
		}
		else
		{
			DataSource *pSource = pPane->FS->FileDataSource( iFile->fileID );

			if ( pSource != nullptr )
			{
				pFS = FSPlugins.FindAndLoadFS( pSource, &*iFile );

				pSource->Release(); // The FS has this now.

				if ( pFS != nullptr )
				{
					step.Object = *iFile;
					step.Step   = Op_CDirectory;

					OpSteps.push_back( step );

					step.Step = Op_Refresh;

					OpSteps.push_back( step );

					step.Step = Op_Enter_FS;
					step.FS   = pFS;

					OpSteps.push_back( step );

					pFS->Init();

					pFS->pParentFS = pPane->FS;

					SaveFS = (FileSystem *) CurrentAction.FS;

					CurrentAction.FS = pFS;

					CreateOpSteps( pFS->pDirectory->Files );

					CurrentAction.FS = SaveFS;

					step.Step = Op_Leave_FS;

					OpSteps.push_back( step );

					step.Step = Op_CParent;

					OpSteps.push_back( step );

					step.Step = Op_Refresh;

					OpSteps.push_back( step );
				}
			}
		}
	}
}

unsigned int __stdcall FileOpThread(void *param) {
	CFileViewer *pSourcePane = (CFileViewer *) CurrentAction.Pane;
	FileSystem  *pSourceFS   = (FileSystem *)  CurrentAction.FS;
	CFileViewer *pTargetPane = (CFileViewer *) CurrentAction.pData;
	FileSystem  *pTargetFS   = nullptr;

	if ( CurrentAction.Action == AA_SET_PROPS )
	{
		pTargetPane = nullptr;
		pTargetFS   = nullptr;
	}
	
	if ( pTargetPane != nullptr )
	{
		pTargetFS = pTargetPane->FS;
	}

	OpSteps.clear();

	TotalOps  = 0;
	CurrentOp = 0;

	YesOnce  = false;
	NoOnce   = false;
	NoToAll  = false;
	YesToAll = false;

	FSChangeLock = true;

	if ( CurrentAction.Action == AA_INSTALL )
	{
		CreateOpStepsByFS( CurrentAction.Selection );
	}
	else
	{
		CreateOpSteps( CurrentAction.Selection );
	}

	std::vector<FileOpStep>::iterator iStep;

	for ( iStep = OpSteps.begin(); iStep != OpSteps.end(); iStep++ )
	{
		bool Redraw = false;

		::PostMessage( hFileWnd, WM_FILEOP_PROGRESS, (WPARAM) Percent( 0, 1, CurrentOp, TotalOps, true ), 0 );

		switch ( iStep->Step )
		{
		case Op_Enter_FS:
			SaveFS    = pSourceFS;
			pSourceFS = iStep->FS;

			CurrentAction.FS = pSourceFS;

			break;

		case Op_Leave_FS:
			pSourceFS = SaveFS;

			delete iStep->FS;

			CurrentAction.FS = pSourceFS;
			
			break;

		case Op_Directory:
			pSourceFS->ChangeDirectory( iStep->Object.fileID );

		case Op_CDirectory:
			if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_Supports_Dirs ) )
			{
				pTargetFS->CreateDirectory( iStep->Object.Filename, true );
			}

			Redraw = true;

			break;

		case Op_Parent:
			pSourceFS->Parent();

		case Op_CParent:
			if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_Supports_Dirs ) )
			{
				pTargetFS->Parent();
			}

			Redraw = true;

			break;

		case Op_Refresh:
			if ( pTargetPane != nullptr )
			{
				pTargetPane->Updated = true;
				pTargetPane->Redraw();
			}
			
			if ( ( CurrentAction.Action == AA_DELETE ) || ( CurrentAction.Action == AA_SET_PROPS ) )
			{
				pSourcePane->Updated = true;
				pSourcePane->Redraw();
			}

			break;

		case Op_Copy:
			{
				CTempFile FileObj;

				pSourceFS->ReadFile( iStep->Object.fileID, FileObj );
				
				int FileResult = pTargetFS->WriteFile( &iStep->Object, FileObj );

				/* If the target FS doesn't understand the source encoding, then the source needs to translate the filename */
				if ( FileResult == FILEOP_NEEDS_ASCII )
				{
					pSourceFS->MakeASCIIFilename( &iStep->Object );

					FileResult = pTargetFS->WriteFile( &iStep->Object, FileObj );
				}

				if ( FileResult == FILEOP_EXISTS )
				{
					if ( !Confirm )
					{
						/* File exists and the user doesn't care */
						pTargetFS->DeleteFile( &iStep->Object, FILEOP_COPY_FILE );
						pTargetFS->WriteFile( &iStep->Object, FileObj );
					}
					else
					{
						if ( YesToAll )
						{
							/* User already agreed to this */
							pTargetFS->DeleteFile( &iStep->Object, FILEOP_COPY_FILE );
							pTargetFS->WriteFile( &iStep->Object, FileObj );
						}
						else if ( !NoToAll )
						{
							/* FIle exists and the user wants to confirm this action */
							::PostMessage( hFileWnd, WM_FILEOP_SUSPEND, FILEOP_EXISTS, 0 );

							EnterCriticalSection( &RedrawLock );

							CurrentObject = iStep->Object;

							LeaveCriticalSection( &RedrawLock );

							::PostMessage( hFileWnd, WM_FILEOP_REDRAW, 0, 0 );

							SuspendThread( hFileThread );

							/* Thread resumes here */
							if ( ( YesOnce ) || ( YesToAll ) )
							{
								pTargetFS->DeleteFile( &iStep->Object, FILEOP_COPY_FILE );
								pTargetFS->WriteFile( &iStep->Object, FileObj );
							}
						}
					}

					YesOnce = false;
					NoOnce  = false;
				}

				if ( FileResult == FILEOP_ISDIR )
				{
					/* Attempt to overwrite directory with file. That ain't working. */
					::PostMessageA( hFileWnd, WM_FILEOP_SUSPEND, FILEOP_ISDIR, 0 );

					return 0;
				}

				Redraw = true;
			}
			break;

		case Op_Delete:	
			{
				if ( !Confirm )
				{
					/* File exists and the user doesn't care */
					pSourceFS->DeleteFile( &iStep->Object, FILEOP_DELETE_FILE );
				}
				else
				{
					if ( YesToAll )
					{
						/* User already agreed to this */
						pSourceFS->DeleteFile( &iStep->Object, FILEOP_DELETE_FILE );
					}
					else if ( !NoToAll )
					{
						/* FIle exists and the user wants to confirm this action */
						::PostMessage( hFileWnd, WM_FILEOP_SUSPEND, FILEOP_DELETE_FILE, 0 );

						EnterCriticalSection( &RedrawLock );

						CurrentObject = iStep->Object;

						LeaveCriticalSection( &RedrawLock );

						::PostMessage( hFileWnd, WM_FILEOP_REDRAW, 0, 0 );

						SuspendThread( hFileThread );

						/* Thread resumes here */
						if ( ( YesOnce ) || ( YesToAll ) )
						{
							pSourceFS->DeleteFile( &iStep->Object, FILEOP_DELETE_FILE );
						}
					}
				}

				YesOnce = false;
				NoOnce  = false;
			}

			Redraw = true;

			break;

		case Op_Set_Props:
			{
				NativeFile Rework = iStep->Object;

				bool Dangerous = false;
				bool Warning   = false;
				
				for ( AttrDesc_iter iAttr = Attrs.begin(); iAttr != Attrs.end(); iAttr++ )
				{
					for ( std::map<DWORD,DWORD>::iterator iChange = Changes.begin(); iChange != Changes.end(); iChange++ )
					{
						if ( iChange->first == iAttr->Index )
						{
							if ( iAttr->Type & AttrDanger )
							{
								Dangerous = true;
							}

							if ( iAttr->Type & AttrWarning )
							{
								Warning = true;
							}

							Rework.Attributes[ iChange->first ] = iChange->second;
						}
					}
				}

				if ( ( !Dangerous ) && ( !Warning ) )
				{
					break;
				}

				if ( !Confirm )
				{
					/* File exists and the user doesn't care */
					pSourceFS->SetProps( iStep->Object.fileID, &Rework );
				}
				else
				{
					if ( YesToAll )
					{
						/* User already agreed to this */
						pSourceFS->SetProps( iStep->Object.fileID, &Rework );
					}
					else if ( !NoToAll )
					{
						/* FIle exists and the user wants to confirm this action */
						::PostMessage( hFileWnd, WM_FILEOP_SUSPEND, FILEOP_WARN_ATTR, 0 );

						EnterCriticalSection( &RedrawLock );

						CurrentObject = iStep->Object;

						LeaveCriticalSection( &RedrawLock );

						::PostMessage( hFileWnd, WM_FILEOP_REDRAW, 0, 0 );

						SuspendThread( hFileThread );

						/* Thread resumes here */
						if ( ( YesOnce ) || ( YesToAll ) )
						{
							pSourceFS->SetProps( iStep->Object.fileID, &Rework );
						}
					}
				}

				YesOnce = false;
				NoOnce  = false;
			}

			Redraw = true;

			break;
		}

		if ( Redraw )
		{
			EnterCriticalSection( &RedrawLock );

			CurrentObject = iStep->Object;

			LeaveCriticalSection( &RedrawLock );

			::PostMessage( hFileWnd, WM_FILEOP_REDRAW, 0, 0 );
		}

		CurrentOp++;
	}

	FSChangeLock = false;

	::PostMessage( hFileWnd, WM_CLOSE, 0, 0 );

	return 0;
}

void DrawClippedTitleStack( std::vector<TitleComponent> *pStack, HDC hWindowDC, NativeFile *pFile )
{
	CFileViewer *pSourcePane = (CFileViewer *) CurrentAction.Pane;
	FileSystem *pSourceFS = (FileSystem *) CurrentAction.FS;

	std::vector<TitleComponent>::iterator iStack;

	DWORD FullWidth = 0;
	DWORD MaxWidth  = FNWidth / 8;

	BYTE i;

	for ( i =1, iStack = pStack->begin(); iStack != pStack->end(); iStack++, i++ )
	{
		BYTE *pString = iStack->String;

		if ( i == pStack->size() )
		{
			pString = pSourceFS->GetTitleString( pFile );
		}

		FullWidth += strlen( (char *) pString );
	}

	int offset = 0;

	if ( MaxWidth < FullWidth )
	{
		offset = ( MaxWidth - FullWidth ) * 8;
	}

	int coffset = offset;

	if ( hNameDC == NULL )
	{
		hNameDC = CreateCompatibleDC( hWindowDC );
	}

	if ( hNameBMP == NULL )
	{
		hNameBMP = CreateCompatibleBitmap( hWindowDC, FNWidth, FNHeight );

		SelectObject( hNameDC, hNameBMP );
	}

	RECT blank;
	blank.left   = 0;
	blank.top    = 0;
	blank.right  = FNWidth;
	blank.bottom = FNHeight;

	FillRect( hNameDC, &blank, (HBRUSH) GetStockObject( WHITE_BRUSH ) );

	for ( i = 1, iStack = pStack->begin(); iStack != pStack->end(); iStack++, i++ )
	{
		BYTE *pString = iStack->String;

		if ( i == pStack->size() )
		{
			pString = pSourceFS->GetTitleString( pFile );
		}

		FontBitmap TitleString(
			FSPlugins.FindFont( iStack->Encoding, pSourcePane->PaneIndex ),
			pString, strlen( ( char * ) pString ), false, false
		);

		TitleString.DrawText( hNameDC, coffset, 0, DT_TOP | DT_LEFT );

		coffset += ( strlen( (char *) iStack->String ) * 8 );
	}

	BitBlt( hWindowDC, FNLeft, FNTop, FNWidth, FNHeight, hNameDC, 0, 0, SRCCOPY );
}

void DrawFilename( HWND hWnd, NativeFile *pFile )
{
	HDC hDC = GetDC( hWnd );

	CFileViewer *pSourcePane = (CFileViewer *) CurrentAction.Pane;

	if ( hIconDC == NULL )
	{
		hIconDC = CreateCompatibleDC( hDC );
	}

	if ( pFile != nullptr )
	{
		HBITMAP hBitmap = BitmapCache.GetBitmap( pFile->Type );

		HGDIOBJ hO = SelectObject( hIconDC, hBitmap );

		BitBlt( hDC, 24, 38, 34, 34, hIconDC, 0, 0, SRCCOPY );

		SelectObject( hIconDC, hO );

		RECT rect;

		rect.left   = FNLeft - 2;
		rect.right  = FNRight + 2;
		rect.top    = FNTop - 2;
		rect.bottom = FNBottom + 2;

		FillRect( hDC, &rect, (HBRUSH) GetStockObject( WHITE_BRUSH ) );
		FrameRect( hDC, &rect, (HBRUSH) GetStockObject( BLACK_BRUSH ) );

		std::vector<TitleComponent> Stack = pSourcePane->GetTitleStack();

		if ( CurrentAction.Action == AA_INSTALL )
		{
			TitleComponent title;

			FileSystem *pFS = (FileSystem *) CurrentAction.FS;

			rstrncpy( title.String, pFS->GetTitleString( pFile ), 512 );
			
			title.Encoding = pFS->GetEncoding();
			
			Stack.push_back( title );
		}

		DrawClippedTitleStack( &Stack, hDC, pFile );
	}

	ReleaseDC( hWnd, hDC );
}

WCHAR *ExplainAttrs( void )
{
	static WCHAR explain[ 512 ];

	std::vector<std::wstring> Warnings;
	std::vector<std::wstring> Dangers;

	std::vector<std::pair<std::wstring,std::wstring>> DangerousOptions;

	std::wstring Explanation;

	/* Look for dangerous attributes */
	for ( std::map<DWORD, DWORD>::iterator iChange = Changes.begin(); iChange != Changes.end(); iChange++ )
	{
		BYTE Index = (BYTE) iChange->first;

		for ( AttrDesc_iter iAttr = Attrs.begin(); iAttr != Attrs.end(); iAttr++ )
		{
			if ( Index == iAttr->Index )
			{
				if ( iAttr->Type & AttrWarning )
				{
					Warnings.push_back( iAttr->Name );
				}
				else if ( iAttr->Type & AttrDanger )
				{
					Dangers.push_back( iAttr->Name );
				}
				else
				{
					for ( AttrOpt_iter iOpt = iAttr->Options.begin(); iOpt != iAttr->Options.end(); iOpt++ )
					{
						if ( iOpt->Dangerous )
						{
							DangerousOptions.push_back( std::make_pair( iAttr->Name, iOpt->Name ) );
						}
					}
				}
			}
		}
	}

	BYTE WarnIndex = 0;

	if ( Warnings.size() > 0 )
	{
		Explanation = L"Changing ";

		for ( std::vector<std::wstring>::iterator iWarn = Warnings.begin(); iWarn != Warnings.end(); iWarn++ )
		{
			Explanation += L"'" + *iWarn + L"'";;

			if ( WarnIndex < Warnings.size() - 1 ) { Explanation += L", "; }

			if ( ( WarnIndex == Warnings.size() - 1 ) && ( Warnings.size() > 1 ) )
			{
				Explanation += L" or ";
			}

			WarnIndex++;
		}

		Explanation += L" may cause the file to behave incorrectly. Continue? ";
	}

	WarnIndex = 0;

	if ( Dangers.size() > 0 )
	{
		Explanation = L"Changing ";

		for ( std::vector<std::wstring>::iterator iWarn = Dangers.begin(); iWarn != Dangers.end(); iWarn++ )
		{
			Explanation += L"'" + *iWarn + L"'";;

			if ( WarnIndex < Warnings.size() - 1 ) { Explanation += L", "; }

			if ( ( WarnIndex == Warnings.size() - 1 ) && ( Warnings.size() > 1 ) )
			{
				Explanation += L" or ";
			}

			WarnIndex++;
		}

		Explanation += L" may cause serious file system malfunction. Continue? ";
	}

	WarnIndex = 0;

	if ( DangerousOptions.size() > 0 )
	{
		Explanation = L"Changing ";

		for ( std::vector<std::pair<std::wstring,std::wstring>>::iterator iWarn = DangerousOptions.begin(); iWarn != DangerousOptions.end(); iWarn++ )
		{
			Explanation += L"'" + iWarn->first + L"' to '" + iWarn->second + L"'";
			
			if ( WarnIndex < Warnings.size() - 1 ) { Explanation += L", "; }

			if ( ( WarnIndex == Warnings.size() - 1 ) && ( Warnings.size() > 1 ) )
			{
				Explanation += L" or ";
			}

			WarnIndex++;
		}

		Explanation += L" may cause serious file system malfunction. Continue? ";
	}

	swprintf( explain, 512, Explanation.c_str() );

	return explain;
}

INT_PTR CALLBACK FileWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HFONT hFont = NULL;
	static WCHAR *title;
	static const WCHAR *CopyTitle   = L" Copy item";
	static const WCHAR *DeleteTitle = L" Delete item";
	static const WCHAR *SetProps    = L" Set File Properties";

	switch (uMsg) {
		case WM_CTLCOLORSTATIC:
			{
				if ((HWND) lParam == GetDlgItem(hwndDlg, IDC_FILEOP_TITLE)) {
					HDC	hDC	= (HDC) wParam;

					SetBkColor(hDC, 0xFFFFFF);

					return (INT_PTR) GetStockObject(WHITE_BRUSH);
				}

				if ((HWND) lParam == GetDlgItem(hwndDlg, IDC_SUSPEND_MSG)) {
					HDC	hDC	= (HDC) wParam;

					SetBkColor(hDC, 0xFFFFFF);

					return (INT_PTR) GetStockObject(WHITE_BRUSH);
				}
			}

			break;

		case WM_COMMAND:
			if ( HIWORD( wParam ) == BN_CLICKED )
			{
				DWORD Btn = LOWORD( wParam );

				if ( Btn == IDC_FILE_CANCEL )
				{
					// TODO: Stop file ops
					EndDialog( hwndDlg, 0 );
				}

				bool DidFileQuery = false;

				if ( Btn == IDC_FILE_YES ) { YesOnce  = true; DidFileQuery = true; }
				if ( Btn == IDC_FILE_NO )  { NoOnce   = true; DidFileQuery = true; }
				if ( Btn == IDC_YES_ALL )  { YesToAll = true; DidFileQuery = true; }
				if ( Btn == IDC_NO_ALL )   { NoToAll  = true; DidFileQuery = true; }

				if ( DidFileQuery )
				{
					RECT InitRect;

					::GetWindowRect( hFileWnd, &InitRect );

					InitRect.bottom -= XtraHeight;

					::SetWindowPos( hFileWnd, HWND_TOPMOST, 0, 0, InitRect.right - InitRect.left, InitRect.bottom - InitRect.top, SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOZORDER );

					EnableWindow( GetDlgItem( hFileWnd, IDC_FILE_YES ), FALSE );
					EnableWindow( GetDlgItem( hFileWnd, IDC_FILE_NO  ), FALSE );
					EnableWindow( GetDlgItem( hFileWnd, IDC_YES_ALL  ), FALSE );
					EnableWindow( GetDlgItem( hFileWnd, IDC_NO_ALL   ), FALSE );

					EnableWindow( GetDlgItem( hFileWnd, IDC_FILE_PAUSE ), TRUE );

					ResumeThread( hFileThread );
				}

				if ( Btn == IDC_FILE_PAUSE )
				{
					static WCHAR *pPaused = L"Pause";
					static WCHAR *pResume = L"Resume";

					if ( Paused )
					{
						::SendMessage( GetDlgItem( hFileWnd, IDC_FILE_PAUSE ), WM_SETTEXT, 0, (LPARAM) pPaused );

						ResumeThread( hFileThread );

						Paused = false;
					}
					else
					{
						::SendMessage( GetDlgItem( hFileWnd, IDC_FILE_PAUSE ), WM_SETTEXT, 0, (LPARAM) pResume );

						SuspendThread( hFileThread );

						Paused = true;
					}
				}
			}

			break;

		case WM_INITDIALOG:
			InitializeCriticalSection( &RedrawLock );

			hFileWnd = hwndDlg;

			if (!hFont) {
				hFont	= CreateFont(24,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
			}

			::SendMessage( GetDlgItem( hwndDlg, IDC_FILEOP_TITLE ), WM_SETFONT, (WPARAM) hFont, 0 );

			if ( CurrentAction.Action == AA_COPY )
			{
				::SendMessageW( GetDlgItem( hwndDlg, IDC_FILEOP_TITLE ), WM_SETTEXT, 0, (LPARAM) CopyTitle );
			}

			if ( CurrentAction.Action == AA_DELETE )
			{
				::SendMessageW( GetDlgItem( hwndDlg, IDC_FILEOP_TITLE ), WM_SETTEXT, 0, (LPARAM) DeleteTitle );
			}

			if ( CurrentAction.Action == AA_SET_PROPS )
			{
				::SendMessageW( GetDlgItem( hwndDlg, IDC_FILEOP_TITLE ), WM_SETTEXT, 0, (LPARAM) SetProps );
			}

			OpSteps.clear();

			CurrentObject = CurrentAction.Selection[ 0 ];

			SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_PROGRESS), PBM_SETRANGE32, 0, 100);
			SendMessage(GetDlgItem(hwndDlg, IDC_FORMAT_PROGRESS), PBM_SETPOS, 0, 0);

			EnableWindow( GetDlgItem( hFileWnd, IDC_FILE_YES ), FALSE );
			EnableWindow( GetDlgItem( hFileWnd, IDC_FILE_NO  ), FALSE );
			EnableWindow( GetDlgItem( hFileWnd, IDC_YES_ALL  ), FALSE );
			EnableWindow( GetDlgItem( hFileWnd, IDC_NO_ALL   ), FALSE );

			EnableWindow( GetDlgItem( hFileWnd, IDC_FILE_PAUSE ), TRUE );
			
			RECT InitRect;

			::GetWindowRect( hFileWnd, &InitRect );

			InitRect.bottom -= XtraHeight;

			::SetWindowPos( hFileWnd, HWND_TOPMOST, 0, 0, InitRect.right - InitRect.left, InitRect.bottom - InitRect.top, SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOZORDER );

			Paused   = false;
			YesOnce  = false;
			NoOnce   = false;
			YesToAll = false;
			NoToAll  = false;

			hFileCancel = CreateEvent( NULL, TRUE, FALSE, NULL );
			hFileThread = (HANDLE) _beginthreadex(NULL, NULL, FileOpThread, NULL, NULL, (unsigned int *) &dwthreadid);

			return FALSE;

		case WM_CLOSE:
			SetEvent( hFileCancel );

			if ( WaitForSingleObject( hFileThread, 500 ) == WAIT_TIMEOUT )
			{
				TerminateThread( hFileThread, 500 );
			}

			EndDialog(hwndDlg,0);

			DeleteCriticalSection( &RedrawLock );

			return TRUE;

		case WM_PAINT:
			DrawFilename( hwndDlg, &CurrentObject );

			return FALSE;

		case WM_FILEOP_PROGRESS:
			SendMessage( GetDlgItem( hwndDlg, IDC_FILE_PROGRESS ), PBM_SETPOS, wParam, 0 );

			return FALSE;

		case WM_FILEOP_REDRAW:
			{
				EnterCriticalSection( &RedrawLock );

				DrawFilename( hwndDlg, &CurrentObject );

				LeaveCriticalSection( &RedrawLock );
			}

			return FALSE;

		case WM_FILEOP_SUSPEND:
			{
				RECT InitRect;

				::GetWindowRect( hFileWnd, &InitRect );

				InitRect.bottom += XtraHeight;

				::SetWindowPos( hFileWnd, HWND_TOPMOST, 0, 0, InitRect.right - InitRect.left, InitRect.bottom - InitRect.top, SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOZORDER );

				static WCHAR *pExists = L"An object in the destination with the same filename already exists. Overwrite?";
				static WCHAR *pIsDir  = L"An object in the destination with the same filename already exists, and is a directory.";
				static WCHAR *pDelete = L"Are you sure you want to delete these items? This operation cannot be undone.";

				if ( ( wParam == FILEOP_EXISTS ) || ( wParam == FILEOP_DELETE_FILE ) || ( wParam == FILEOP_WARN_ATTR ) )
				{
					::SendMessage( GetDlgItem( hFileWnd, IDC_OHNOES ), STM_SETICON, (WPARAM) LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_WARNING2) ), 0 );
					
					if ( wParam == FILEOP_EXISTS )
					{
						::SendMessage( GetDlgItem( hFileWnd, IDC_SUSPEND_MSG ), WM_SETTEXT, 0, (LPARAM) pExists );
					}
					else if ( wParam == FILEOP_DELETE_FILE )
					{
						::SendMessage( GetDlgItem( hFileWnd, IDC_SUSPEND_MSG ), WM_SETTEXT, 0, (LPARAM) pDelete );
					}
					else if ( wParam == FILEOP_WARN_ATTR )
					{
						::SendMessage( GetDlgItem( hFileWnd, IDC_SUSPEND_MSG ), WM_SETTEXT, 0, (LPARAM) ExplainAttrs() );
					}

					EnableWindow( GetDlgItem( hFileWnd, IDC_FILE_YES ), TRUE );
					EnableWindow( GetDlgItem( hFileWnd, IDC_FILE_NO  ), TRUE );
					EnableWindow( GetDlgItem( hFileWnd, IDC_YES_ALL  ), TRUE );
					EnableWindow( GetDlgItem( hFileWnd, IDC_NO_ALL   ), TRUE );

					EnableWindow( GetDlgItem( hFileWnd, IDC_FILE_PAUSE ), FALSE );
				}

				if ( wParam == FILEOP_ISDIR )
				{
					::SendMessage( GetDlgItem( hFileWnd, IDC_OHNOES ), STM_SETICON, (WPARAM) LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_JOSE) ), 0 );
					::SendMessage( GetDlgItem( hFileWnd, IDC_SUSPEND_MSG ), WM_SETTEXT, 0, (LPARAM) pIsDir );

					EnableWindow( GetDlgItem( hFileWnd, IDC_FILE_PAUSE ), FALSE );
				}
			}

			return FALSE;

		default:
			break;
	}

	return FALSE;
}

int	FileOP_Handler(AppAction &Action) {

	CurrentAction	= Action;

	Confirm = Preference( L"Confirm", true );

	DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_FILEOPS), Action.hWnd, FileWindowProc, NULL);

	return 0;
}