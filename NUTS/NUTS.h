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

extern	CFileViewer	leftPane,rightPane;
extern  std::vector<FileSystem *> leftFS;
extern  std::vector<FileSystem *> rightFS;

extern	HWND	hStatusWnd;

#define EXTRA_MENU_BASE     43900
#define EXTRA_MENU_END      ( EXTRA_MENU_BASE + 199 )
