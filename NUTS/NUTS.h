#pragma once

#include "resource.h"
#include "FileViewer.h"
#include "BitmapCache.h"
#include "IDE8Source.h"
#include "ImageDataSource.h"
#include "RootFileSystem.h"
#include "RootDirectory.h"
#include "WindowsFileSystem.h"

#include <commctrl.h>

//extern  CFileViewer	*leftPane;
//extern  CFileViewer *rightPane;

extern  std::vector<FileSystem *> leftFS;
extern  std::vector<FileSystem *> rightFS;

extern std::vector<TitleComponent> leftTitles;
extern std::vector<TitleComponent> rightTitles;

extern  HWND hStatusWnd;

#define EXTRA_MENU_BASE     43900
#define EXTRA_MENU_END      ( EXTRA_MENU_BASE + 199 )

typedef struct _FSAction
{
	ActionType                  Type;
	CFileViewer                 *pane;
	std::vector<FileSystem *>   *pStack;
	std::vector<TitleComponent> *pTitleStack;
	int                         EnterIndex;
	FileSystem                  *FS;
	FSIdentifier                FSID;
} FSAction;

/* Action Thread */
void DoAction( ActionType t, CFileViewer *p, std::vector<FileSystem *> *s, std::vector<TitleComponent> *pT, int i, FSIdentifier TargetFSID = FS_Null );
unsigned int __stdcall FSActionThread(void *param);
