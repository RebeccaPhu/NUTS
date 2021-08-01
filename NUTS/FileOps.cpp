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
//#define FNRight   301
#define FNRight   356
#define FNBottom  62
#define FNHeight ( FNBottom - FNTop )
//#define FNWidth  ( FNRight - FNLeft )

#define XtraHeight 64

static WCHAR *pTYes = L"Yes"; static WCHAR *pTNo = L"No"; static WCHAR *pTYesAll = L"Yes To All"; static WCHAR *pTNoAll = L"No To All";
static WCHAR *pTMerge = L"Merge"; static WCHAR *pTRen = L"Rename"; static WCHAR *pTMergeAll = L"Merge All"; static WCHAR *pTRenAll = L"Rename All";

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
bool    Hunting     = false;
bool    MergeAll    = false;
bool    RenameAll   = false;
bool    MergeOnce   = false;
bool    RenameOnce  = false;
bool    AlwaysRename= false;
bool    AlwaysMerge = false;
bool    DirDialog   = false;

DWORD   HuntCount   = 0;

static DWORD dwthreadid  = 0;

std::vector<FileOpStep> OpSteps;

DWORD TotalOps;
DWORD CurrentOp;

CRITICAL_SECTION RedrawLock;

NativeFile CurrentObject;

AttrDescriptors Attrs;
std::map<DWORD, DWORD> Changes;

FileSystem *SaveFS;

std::vector<NativeFile> IgnoreSidecars;

DWORD RecurseSteps;

void CreateOpSteps( std::vector<NativeFile> Selection )
{
	RecurseSteps++;

	if ( RecurseSteps == 40 )
	{
		MessageBox( hFileWnd, L"Too many levels deep. Some objects have been omitted.", L"NUTS File Operations", MB_ICONWARNING | MB_OK );

		RecurseSteps--;
	}

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
			HuntCount++;

			::PostMessage( hFileWnd, WM_FILEOP_NOTICE, 0, (LPARAM) HuntCount );

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

			FileOpStep Final;

			Final.Step = Op_Refresh;
			OpSteps.push_back( Final );

			TotalOps++;
		}
	}

	/* As we go through the selection and delete files, the subsequent files will shift down in file ID.
	   So, modify the FileID by subtracting this value, then add 1 to this value. */

	DWORD FileIDOffset = 0;

	/* Re-iterate the selection and add files */
	for ( iFile = Selection.begin(); iFile != Selection.end(); iFile++ )
	{
		if ( ( ! (iFile->Flags & FF_Directory) ) || ( CurrentAction.Action == AA_DELETE ) || ( CurrentAction.Action == AA_SET_PROPS ) )
		{
			HuntCount++;

			::PostMessage( hFileWnd, WM_FILEOP_NOTICE, 0, (LPARAM) HuntCount );

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

				step.Object.fileID -= FileIDOffset;

				FileIDOffset++;
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

	RecurseSteps--;
}

void CreateOpStepsByFS( std::vector<NativeFile> Selection )
{
	/* This assumes each item in the selection is a file system that
	   can be copied. If it is not, just skip the item. */

	RecurseSteps = 0;

	NativeFileIterator iFile;

	for ( iFile = Selection.begin(); iFile != Selection.end(); iFile++ )
	{
		HuntCount++;

		::PostMessage( hFileWnd, WM_FILEOP_NOTICE, 0, (LPARAM) HuntCount );

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

void DoSidecar( FileSystem *pSrc, FileSystem *pTrg, NativeFile *pFile, bool PreCopy )
{
	if ( ( pTrg->FSID == FS_Windows ) && ( pSrc->Flags & FSF_Exports_Sidecars ) && ( !PreCopy ) )
	{
		SidecarExport sidecar;

		CTempFile FileObj;

		sidecar.FileObj = &FileObj;

		pSrc->ExportSidecar( pFile, sidecar );

		NativeFile SCFile = *pFile;

		SCFile.Filename = sidecar.Filename;

		SCFile.Flags  = 0;
		SCFile.Length = FileObj.Ext();

		if ( pTrg->WriteFile( &SCFile, FileObj ) != DS_SUCCESS )
		{
			NUTSError::Report( L"Create Sidecar File", hFileWnd );

			/* This is not the end of the world, but might be annoying */
			pGlobalError->GlobalCode = 0;
		}
	}

	if ( ( pSrc->FSID == FS_Windows ) && ( pTrg->Flags & FSF_Exports_Sidecars ) && ( PreCopy ) )
	{
		/* Two stage process. First, get the file we should be looking for. */
		SidecarImport sidecar;

		pTrg->ImportSidecar( pFile, sidecar, nullptr );

		/* Search for this file in the Windows FS */
		NativeFileIterator iFile;

		for ( iFile = pSrc->pDirectory->Files.begin(); iFile != pSrc->pDirectory->Files.end(); iFile++ )
		{
			BYTEString CollatedFilename( iFile->Filename.size() + iFile->Extension.size() );

			rstrncpy( CollatedFilename, iFile->Filename, iFile->Filename.length() );

			if ( iFile->Flags & FF_Extension )
			{
				rstrncat( CollatedFilename, (BYTE *) ".", iFile->Filename.length() + iFile->Extension.length() );
				rstrncat( CollatedFilename, iFile->Extension, iFile->Extension.length() );
			}

			if ( rstrnicmp( CollatedFilename, sidecar.Filename, 256 ) )
			{
				/* Found it. Read it in, and send it to the FS again. */
				CTempFile FileObj;

				if ( pSrc->ReadFile( iFile->fileID, FileObj ) != DS_SUCCESS )
				{
					NUTSError::Report( L"Read sidecar file", hFileWnd );
				}

				pTrg->ImportSidecar( pFile, sidecar, &FileObj );

				/* Push the original native file in the Windows FS to ignore sidecars, we'll - well - ignore it later */
				IgnoreSidecars.push_back( *iFile );
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
	IgnoreSidecars.clear();

	TotalOps  = 0;
	CurrentOp = 0;

	YesOnce  = false;
	NoOnce   = false;
	NoToAll  = false;
	YesToAll = false;

	MergeOnce = false;
	MergeAll  = false;
	RenameOnce= false;
	RenameAll = false;

	RecurseSteps = 0;

	pGlobalError->GlobalCode = 0;

	if ( CurrentAction.Action == AA_INSTALL )
	{
		CreateOpStepsByFS( CurrentAction.Selection );
	}
	else
	{
		CreateOpSteps( CurrentAction.Selection );
	}

	if ( pGlobalError->GlobalCode != 0 )
	{
		NUTSError::Report( L"File Operation Setup", hFileWnd );

		return 0;
	}

	Hunting = false;

	::PostMessage( hFileWnd, WM_FILEOP_NOTICE, 0, (LPARAM) HuntCount );

	std::vector<FileOpStep>::iterator iStep;

	bool IsInstall = false;

	for ( iStep = OpSteps.begin(); iStep != OpSteps.end(); iStep++ )
	{
		if ( pGlobalError->GlobalCode != 0 )
		{
			break;
		}

		bool Redraw = false;

		::PostMessage( hFileWnd, WM_FILEOP_PROGRESS, (WPARAM) Percent( 0, 1, CurrentOp, TotalOps, true ), 0 );

		switch ( iStep->Step )
		{
		case Op_Enter_FS:
			SaveFS    = pSourceFS;
			pSourceFS = iStep->FS;

			CurrentAction.FS = pSourceFS;

			IsInstall = true;

			break;

		case Op_Leave_FS:
			pSourceFS = SaveFS;

			delete iStep->FS;

			CurrentAction.FS = pSourceFS;
			
			break;

		case Op_Directory:
			if ( pSourceFS->ChangeDirectory( iStep->Object.fileID ) != NUTS_SUCCESS )
			{
				NUTSError::Report( L"Change Directory (Source)", hFileWnd );

				break;
			}

		case Op_CDirectory:
			if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_Supports_Dirs ) )
			{
				DWORD CFlags = CDF_ENTER_AFTER;

				if ( IsInstall ) { CFlags |= CDF_INSTALL_OP; }
				
				int FileResult = pTargetFS->CreateDirectory( &iStep->Object, CFlags );

				/* If the target FS doesn't understand the source encoding, then the source needs to translate the filename */
				if ( FileResult == FILEOP_NEEDS_ASCII )
				{
					pSourceFS->MakeASCIIFilename( &iStep->Object );

					FileResult = pTargetFS->CreateDirectory( &iStep->Object, CFlags );
				}

				if ( FileResult == FILEOP_EXISTS )
				{
					if ( AlwaysMerge )
					{
						/* Directory exists and the user would like to merge */
						CFlags |= CDF_MERGE_DIR;

						FileResult = pTargetFS->CreateDirectory( &iStep->Object, CFlags );
					}
					else if ( AlwaysRename )
					{
						/* Directory exists and the user would like to rename */
						CFlags |= CDF_RENAME_DIR;

						FileResult = pTargetFS->CreateDirectory( &iStep->Object, CFlags );
					}
					else
					{
						if ( MergeAll )
						{
							/* User already agreed to this */
							CFlags |= CDF_MERGE_DIR;

							FileResult = pTargetFS->CreateDirectory( &iStep->Object, CFlags );
						}
						else if ( RenameAll )
						{
							CFlags |= CDF_RENAME_DIR;

							FileResult = pTargetFS->CreateDirectory( &iStep->Object, CFlags );
						}
						else
						{
							/* Directory exists and the user wants to confirm this action */
							::PostMessage( hFileWnd, WM_FILEOP_SUSPEND, FILEOP_DIR_EXISTS, 0 );

							EnterCriticalSection( &RedrawLock );

							CurrentObject = iStep->Object;

							LeaveCriticalSection( &RedrawLock );

							::PostMessage( hFileWnd, WM_FILEOP_REDRAW, 0, 0 );

							SuspendThread( hFileThread );

							/* Thread resumes here */
							if ( ( MergeOnce ) || ( MergeAll ) )
							{
								CFlags |= CDF_MERGE_DIR;

								FileResult = pTargetFS->CreateDirectory( &iStep->Object, CFlags );
							}
							else if ( ( RenameOnce ) || ( RenameAll ) )
							{
								CFlags |= CDF_RENAME_DIR;

								FileResult = pTargetFS->CreateDirectory( &iStep->Object, CFlags );
							}
						}
					}

					MergeOnce  = false;
					RenameOnce = false;
				}

				if ( FileResult == FILEOP_ISFILE )
				{
					/* Attempt to overwrite file with directory. That ain't working. */
					::PostMessageA( hFileWnd, WM_FILEOP_SUSPEND, FILEOP_ISFILE, 0 );

					return 0;
				}

				if ( FileResult != NUTS_SUCCESS )
				{
					NUTSError::Report( L"Create Directory (Target)", hFileWnd );

					break;
				}
			}

			Redraw = true;

			break;

		case Op_Parent:
			if ( pSourceFS->Parent() != NUTS_SUCCESS )
			{
				NUTSError::Report( L"Parent Directory (Source)", hFileWnd );

				break;
			}

		case Op_CParent:
			if ( ( pTargetFS != nullptr ) && ( pTargetFS->Flags & FSF_Supports_Dirs ) )
			{
				if ( pTargetFS->Parent() != NUTS_SUCCESS )
				{
					NUTSError::Report( L"Parent Directory (Target)", hFileWnd );

					break;
				}
			}

			Redraw = true;

			break;

		case Op_Refresh:
			if ( pTargetPane != nullptr )
			{
				pTargetPane->Updated = true;
				pTargetPane->Update();
			}
			
			if ( ( CurrentAction.Action == AA_DELETE ) || ( CurrentAction.Action == AA_SET_PROPS ) )
			{
				if ( pSourcePane != nullptr )
				{
					pSourcePane->Updated = true;
					pSourcePane->Update();
				}
			}

			break;

		case Op_Copy:
			{
				/* Check if we already copied this as a sidecar */
				NativeFileIterator iFile;
				bool DoneIt = false;

				for ( iFile = IgnoreSidecars.begin(); iFile != IgnoreSidecars.end(); iFile++ )
				{
					if ( FilenameCmp( &*iFile, &iStep->Object ) )
					{
						DoneIt = true;
					}
				}

				if ( DoneIt )
				{
					break;
				}

				/* Not a sidecar we've previously seen, so go ahead with it */
				CTempFile FileObj;

				if ( pSourceFS->ReadFile( iStep->Object.fileID, FileObj ) != NUTS_SUCCESS )
				{
					NUTSError::Report( L"File Read (Source)", hFileWnd );

					break;
				}
				
				DoSidecar( pSourceFS, pTargetFS, &iStep->Object, true );

				int FileResult = pTargetFS->WriteFile( &iStep->Object, FileObj );

				/* If the target FS doesn't understand the source encoding, then the source needs to translate the filename */
				if ( FileResult == FILEOP_NEEDS_ASCII )
				{
					pSourceFS->MakeASCIIFilename( &iStep->Object );

					DoSidecar( pSourceFS, pTargetFS, &iStep->Object, true );

					FileResult = pTargetFS->WriteFile( &iStep->Object, FileObj );
				}

				if ( FileResult == FILEOP_EXISTS )
				{
					if ( !Confirm )
					{
						/* File exists and the user doesn't care */
						DoSidecar( pSourceFS, pTargetFS, &iStep->Object, true );

						pTargetFS->DeleteFile( iStep->Object.fileID );

						DoSidecar( pSourceFS, pTargetFS, &iStep->Object, true );

						FileResult = pTargetFS->WriteFile( &iStep->Object, FileObj );

						DoSidecar( pSourceFS, pTargetFS, &iStep->Object, false );
					}
					else
					{
						if ( YesToAll )
						{
							/* User already agreed to this */
							DoSidecar( pSourceFS, pTargetFS, &iStep->Object, true );

							pTargetFS->DeleteFile( iStep->Object.fileID );

							DoSidecar( pSourceFS, pTargetFS, &iStep->Object, true );

							FileResult = pTargetFS->WriteFile( &iStep->Object, FileObj );

							DoSidecar( pSourceFS, pTargetFS, &iStep->Object, false );
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
								DoSidecar( pSourceFS, pTargetFS, &iStep->Object, true );

								pTargetFS->DeleteFile( iStep->Object.fileID );

								DoSidecar( pSourceFS, pTargetFS, &iStep->Object, true );

								FileResult = pTargetFS->WriteFile( &iStep->Object, FileObj );

								DoSidecar( pSourceFS, pTargetFS, &iStep->Object, false );
							}
						}
					}

					YesOnce = false;
					NoOnce  = false;
				}
				else
				{
					DoSidecar( pSourceFS, pTargetFS, &iStep->Object, false );
				}


				if ( FileResult == FILEOP_ISDIR )
				{
					/* Attempt to overwrite directory with file. That ain't working. */
					::PostMessageA( hFileWnd, WM_FILEOP_SUSPEND, FILEOP_ISDIR, 0 );

					return 0;
				}

				if ( pGlobalError->GlobalCode != NUTS_SUCCESS )
				{
					NUTSError::Report( L"File Write (Target)", hFileWnd );

					break;
				}

				if ( FileResult != NUTS_SUCCESS )
				{
					NUTSError::Report( L"Copy file", hFileWnd );

					break;
				}

				Redraw = true;
			}
			break;

		case Op_Delete:	
			{
				if ( !Confirm )
				{
					/* File exists and the user doesn't care */
					if ( pSourceFS->DeleteFile( iStep->Object.fileID ) != NUTS_SUCCESS )
					{
						NUTSError::Report( L"File Delete", hFileWnd );

						break;
					}
				}
				else
				{
					if ( YesToAll )
					{
						/* User already agreed to this */
						if ( pSourceFS->DeleteFile( iStep->Object.fileID ) != NUTS_SUCCESS )
						{
							NUTSError::Report( L"File Delete", hFileWnd );

							break;
						}
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
							if ( pSourceFS->DeleteFile( iStep->Object.fileID ) != NUTS_SUCCESS )
							{
								NUTSError::Report( L"Delete File", hFileWnd );

								break;
							}
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
					if ( pSourceFS->SetProps( iStep->Object.fileID, &Rework ) != NUTS_SUCCESS )
					{
						NUTSError::Report( L"Set File Properties", hFileWnd );
					}

					break;
				}

				if ( !Confirm )
				{
					/* User doesn't care */
					if ( pSourceFS->SetProps( iStep->Object.fileID, &Rework ) != NUTS_SUCCESS )
					{
						NUTSError::Report( L"Set File Properties", hFileWnd );

						break;
					}
				}
				else
				{
					if ( YesToAll )
					{
						/* User already agreed to this */
						if ( pSourceFS->SetProps( iStep->Object.fileID, &Rework ) != NUTS_SUCCESS )
						{
							NUTSError::Report( L"Set File Properties", hFileWnd );

							break;
						}
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
							if ( pSourceFS->SetProps( iStep->Object.fileID, &Rework ) != NUTS_SUCCESS )
							{
								NUTSError::Report( L"Set File Properties", hFileWnd );

								break;
							}
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

	::PostMessage( hFileWnd, WM_CLOSE, 0, 0 );

	return 0;
}

void DrawClippedTitleStack( std::vector<TitleComponent> *pStack, HDC hWindowDC, NativeFile *pFile )
{
	CFileViewer *pSourcePane = (CFileViewer *) CurrentAction.Pane;
	FileSystem *pSourceFS = (FileSystem *) CurrentAction.FS;

	if ( pSourcePane == nullptr )
	{
		return;
	}

	std::vector<TitleComponent>::iterator iStack;

	RECT r;

	GetClientRect( GetDlgItem( hFileWnd, IDC_FILE_PROGRESS), &r );

	DWORD FullWidth = 0;
	DWORD FNWidth = ( r.right - r.left ) - 42;
	DWORD MaxWidth  = FNWidth / 8;

	BYTE i;

	for ( i =1, iStack = pStack->begin(); iStack != pStack->end(); iStack++, i++ )
	{
		BYTE *pString = iStack->String;

		if ( i == pStack->size() )
		{
			pString = pSourceFS->GetTitleString( pFile );
		}

		FullWidth += rstrnlen( pString, 8192 );
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
	CFileViewer *pSourcePane = (CFileViewer *) CurrentAction.Pane;

	if ( pSourcePane == nullptr )
	{
		return;
	}

	HDC hDC = GetDC( hWnd );

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

		GetClientRect( GetDlgItem( hFileWnd, IDC_FILE_PROGRESS), &rect );

		DWORD FNWidth = ( rect.right - rect.left ) - 42;

		rect.left   = FNLeft - 2;
		rect.right  = FNLeft + FNWidth + 2;
		rect.top    = FNTop - 2;
		rect.bottom = FNBottom + 2;

		FillRect( hDC, &rect, (HBRUSH) GetStockObject( WHITE_BRUSH ) );
		FrameRect( hDC, &rect, (HBRUSH) GetStockObject( BLACK_BRUSH ) );

		if ( Hunting )
		{
			BYTE HuntingText[ 64 ];

			rsprintf( HuntingText, "Found %d objects...", HuntCount );
		
			FontBitmap HuntText( FONTID_PC437, HuntingText, 63, false, false );

			HuntText.DrawText( hDC, FNLeft, FNTop, DT_LEFT | DT_TOP );
		}
		else
		{
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

void StopOps( void )
{
	SetEvent( hFileCancel );

	if ( WaitForSingleObject( hFileThread, 500 ) == WAIT_TIMEOUT )
	{
		TerminateThread( hFileThread, 500 );
	}

	DeleteCriticalSection( &RedrawLock );
	
	if ( hIconDC != NULL ) { DeleteDC( hIconDC ); }
	if ( hNameDC != NULL ) { DeleteDC( hNameDC ); }

	if ( hNameBMP != NULL ) { DeleteObject( (HGDIOBJ) hNameBMP ); }

	if ( hFileThread != NULL ) { CloseHandle( hFileThread ); }
	if ( hFileCancel != NULL ) { CloseHandle( hFileCancel ); }

	hIconDC     = NULL;
	hNameDC     = NULL;
	hNameBMP    = NULL;
	hFileThread = NULL;
	hFileCancel = NULL;
}

INT_PTR CALLBACK FileWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HFONT hFont = NULL;
	static WCHAR *title;
	static const WCHAR *CopyTitle   = L" Copy item(s)";
	static const WCHAR *DeleteTitle = L" Delete item(s)";
	static const WCHAR *SetProps    = L" Set File Properties";
	static const WCHAR *Install     = L" Install Disk(s)";

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
					StopOps();

					EndDialog( hwndDlg, 0 );

					hFileWnd = NULL;
				}

				bool DidFileQuery = false;

				if ( DirDialog )
				{
					if ( Btn == IDC_FILE_YES ) { MergeOnce  = true; DidFileQuery = true; }
					if ( Btn == IDC_FILE_NO )  { RenameOnce = true; DidFileQuery = true; }
					if ( Btn == IDC_YES_ALL )  { MergeAll   = true; DidFileQuery = true; }
					if ( Btn == IDC_NO_ALL )   { RenameAll  = true; DidFileQuery = true; }
				}
				else
				{
					if ( Btn == IDC_FILE_YES ) { YesOnce  = true; DidFileQuery = true; }
					if ( Btn == IDC_FILE_NO )  { NoOnce   = true; DidFileQuery = true; }
					if ( Btn == IDC_YES_ALL )  { YesToAll = true; DidFileQuery = true; }
					if ( Btn == IDC_NO_ALL )   { NoToAll  = true; DidFileQuery = true; }
				}

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
			Hunting   = true;
			HuntCount = 0;

			InitializeCriticalSection( &RedrawLock );
			
			hFileWnd = hwndDlg;

			if (!hFont) {
				hFont	= CreateFont(24,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("MS Shell Dlg"));
			}

			::SendMessage( GetDlgItem( hwndDlg, IDC_FILEOP_TITLE ), WM_SETFONT, (WPARAM) hFont, 0 );

			if ( CurrentAction.Action == AA_INSTALL )
			{
				::SendMessageW( GetDlgItem( hwndDlg, IDC_FILEOP_TITLE ), WM_SETTEXT, 0, (LPARAM) Install );
			}

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

			::SendMessage( GetDlgItem( hFileWnd, IDC_FILE_YES ), WM_SETTEXT, 0, (LPARAM) pTYes );
			::SendMessage( GetDlgItem( hFileWnd, IDC_FILE_NO ),  WM_SETTEXT, 0, (LPARAM) pTNo );
			::SendMessage( GetDlgItem( hFileWnd, IDC_YES_ALL ),  WM_SETTEXT, 0, (LPARAM) pTYesAll );
			::SendMessage( GetDlgItem( hFileWnd, IDC_NO_ALL ),   WM_SETTEXT, 0, (LPARAM) pTNoAll );

			Paused   = false;
			YesOnce  = false;
			NoOnce   = false;
			YesToAll = false;
			NoToAll  = false;

			MergeOnce = false;
			RenameOnce= false;
			MergeAll  = false;
			RenameAll = false;

			hFileCancel = CreateEvent( NULL, TRUE, FALSE, NULL );
			hFileThread = (HANDLE) _beginthreadex(NULL, NULL, FileOpThread, NULL, NULL, (unsigned int *) &dwthreadid);

			return FALSE;

		case WM_CLOSE:
			StopOps();

			EndDialog(hwndDlg,0);

			hFileWnd = NULL;

			return TRUE;

		case WM_PAINT:
			DrawFilename( hwndDlg, &CurrentObject );

			return FALSE;

		case WM_FILEOP_PROGRESS:
			SendMessage( GetDlgItem( hwndDlg, IDC_FILE_PROGRESS ), PBM_SETPOS, wParam, 0 );

			return FALSE;

		case WM_FILEOP_NOTICE:
			{
				HuntCount = (DWORD) lParam;

				RECT r;

				GetClientRect( hwndDlg, &r );

				InvalidateRect( hwndDlg, &r, FALSE );
			}
			break;

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

				static WCHAR *pExists  = L"An object in the destination with the same filename already exists. Overwrite?";
				static WCHAR *pIsDir   = L"An object in the destination with the same filename already exists, and is a directory.";
				static WCHAR *pIsFile  = L"An object in the destination with the same filename already exists, and is a file.";
				static WCHAR *pDelete  = L"Are you sure you want to delete these items? This operation cannot be undone.";
				static WCHAR *pDExists = L"A directory in the destination with the same name already exists. Merge the directories, or rename the new directory?";

				if ( ( wParam == FILEOP_EXISTS ) || ( wParam == FILEOP_DELETE_FILE ) || ( wParam == FILEOP_WARN_ATTR ) || ( wParam == FILEOP_DIR_EXISTS ) )
				{
					::SendMessage( GetDlgItem( hFileWnd, IDC_OHNOES ), STM_SETICON, (WPARAM) LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_WARNING2) ), 0 );
					
					if ( wParam == FILEOP_EXISTS )
					{
						::SendMessage( GetDlgItem( hFileWnd, IDC_SUSPEND_MSG ), WM_SETTEXT, 0, (LPARAM) pExists );
					}
					else if ( wParam == FILEOP_DIR_EXISTS )
					{
						::SendMessage( GetDlgItem( hFileWnd, IDC_SUSPEND_MSG ), WM_SETTEXT, 0, (LPARAM) pDExists );
					}
					else if ( wParam == FILEOP_DELETE_FILE )
					{
						::SendMessage( GetDlgItem( hFileWnd, IDC_SUSPEND_MSG ), WM_SETTEXT, 0, (LPARAM) pDelete );
					}
					else if ( wParam == FILEOP_WARN_ATTR )
					{
						::SendMessage( GetDlgItem( hFileWnd, IDC_SUSPEND_MSG ), WM_SETTEXT, 0, (LPARAM) ExplainAttrs() );
					}

					if ( wParam == FILEOP_DIR_EXISTS )
					{
						DirDialog = true;

						::SendMessage( GetDlgItem( hFileWnd, IDC_FILE_YES ), WM_SETTEXT, 0, (LPARAM) pTMerge );
						::SendMessage( GetDlgItem( hFileWnd, IDC_FILE_NO ),  WM_SETTEXT, 0, (LPARAM) pTRen );
						::SendMessage( GetDlgItem( hFileWnd, IDC_YES_ALL ),  WM_SETTEXT, 0, (LPARAM) pTMergeAll );
						::SendMessage( GetDlgItem( hFileWnd, IDC_NO_ALL ),   WM_SETTEXT, 0, (LPARAM) pTRenAll );
					}
					else
					{
						DirDialog = false;

						::SendMessage( GetDlgItem( hFileWnd, IDC_FILE_YES ), WM_SETTEXT, 0, (LPARAM) pTYes );
						::SendMessage( GetDlgItem( hFileWnd, IDC_FILE_NO ),  WM_SETTEXT, 0, (LPARAM) pTNo );
						::SendMessage( GetDlgItem( hFileWnd, IDC_YES_ALL ),  WM_SETTEXT, 0, (LPARAM) pTYesAll );
						::SendMessage( GetDlgItem( hFileWnd, IDC_NO_ALL ),   WM_SETTEXT, 0, (LPARAM) pTNoAll );
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

				if ( wParam == FILEOP_ISFILE )
				{
					::SendMessage( GetDlgItem( hFileWnd, IDC_OHNOES ), STM_SETICON, (WPARAM) LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_JOSE) ), 0 );
					::SendMessage( GetDlgItem( hFileWnd, IDC_SUSPEND_MSG ), WM_SETTEXT, 0, (LPARAM) pIsFile );

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