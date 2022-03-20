#ifndef NUTSXLAT_H
#define NUTSXLAT_H

#include "stdafx.h"

#include <string>
#include <map>
#include <vector>

#include "NUTSTypes.h"

typedef struct _ScreenMode {
	std::wstring FriendlyName;
	DWORD        ModeID;
} ScreenMode;

typedef std::vector<ScreenMode> ModeList;
typedef ModeList::iterator ModeList_iter;

typedef std::map<WORD,WORD> LogPalette;
typedef std::map<WORD,std::pair<WORD,WORD>> PhysColours;
typedef std::map<WORD,DWORD> PhysPalette;

#ifndef ASPECT_TYPE
#define ASPECT_TYPE
typedef std::pair<WORD, WORD> AspectRatio;
#endif

typedef enum _DisplayPref {
	DisplayScaledScreen = 1,
	DisplayNatural      = 2,
} DisplayPref;

typedef struct _GFXTranslateOptions {
	BITMAPINFO *bmi;
	void **pGraphics;
	LogPalette  *pLogicalPalette;
	PhysPalette *pPhysicalPalette;
	PhysColours *pPhysicalColours;
	DWORD ModeID;
	long long Offset;
	QWORD Length;
	bool  FlashPhase;
	HWND  NotifyWnd;
} GFXTranslateOptions;

typedef struct _TXTTranslateOptions {
	BYTE **pTextBuffer;
	DWORD  TextBodyLength;
	std::vector<DWORD> LinePointers;
	EncodingIdentifier EncodingID;
	HWND  ProgressWnd;
	HANDLE hStop;
	WORD  CharacterWidth;
	bool  RetranslateOnResize;
} TXTTranslateOptions;

#endif