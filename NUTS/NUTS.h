#pragma once

#include "resource.h"
#include "FileViewer.h"
#include "BitmapCache.h"
#include "IDE8Source.h"
#include "ImageDataSource.h"
#include "RootFileSystem.h"
#include "RootDirectory.h"
#include "WindowsFileSystem.h"

#include "defs.h"

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

typedef enum _ActionType
{
	ActionDoEnter   = 1,
	ActionDoEnterAs = 2,
	ActionDoBack    = 3,
	ActionDoRoot    = 4,
	ActionDoRefresh = 5,
	ActionDoRename  = 6,
	ActionDoNewDir  = 7,
} ActionType;

typedef struct _FSAction
{
	ActionType                  Type;
	CFileViewer                 *pane;
	std::vector<FileSystem *>   *pStack;
	std::vector<TitleComponent> *pTitleStack;
	int                         EnterIndex;
	FileSystem                  *FS;
	DWORD                       FSID;
} FSAction;


/* Action Thread */
void DoAction( ActionType t, CFileViewer *p, std::vector<FileSystem *> *s, std::vector<TitleComponent> *pT, int i, DWORD TargetFSID = 0 );
unsigned int __stdcall FSActionThread(void *param);
