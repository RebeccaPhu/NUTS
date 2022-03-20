#ifndef NUTSUI_H
#define NUTSUI_H

#include "stdafx.h"

#include <vector>
#include <map>

#include "NUTSTypes.h"
#include "NUTSAction.h"
#include "NativeFile.h"

typedef struct _AppAction {
	ActionID Action;
	void     *Pane;
	void     *FS;
	void     *pData;
	HWND     hWnd;

	std::vector<NativeFile> Selection;
} AppAction;

typedef struct _Panel {
	DWORD ID;
	BYTE  Text[ 256 ];
	DWORD Width;
	DWORD Flags;
	FontIdentifier FontID;
} Panel;

typedef std::map<DWORD, Panel> PanelList;
typedef PanelList::iterator    PanelIterator;

typedef struct _TitleComponent {
	BYTE String[512];
	EncodingIdentifier Encoding;
} TitleComponent;

#endif