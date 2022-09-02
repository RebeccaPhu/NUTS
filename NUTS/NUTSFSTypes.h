#ifndef NUTSFSTYPES_H
#define NUTSFSTYPES_H

#include "stdafx.h"

#include <string>
#include <vector>

#include "NUTSTypes.h"
#include "BYTEString.h"

typedef struct _FormatDesc {
	std::wstring Format;
	BYTEString   PreferredExtension;
	FSIdentifier FSID;
	DWORD        Flags;
	QWORD        MaxSize;
	DWORD        SectorSize;
} FormatDesc;

typedef std::vector<FormatDesc> FormatList;
typedef std::vector<FormatDesc>::iterator FormatDesc_iter;

typedef enum _FormatFlags {
	FTF_LLF          = 0x00000001,
	FTF_Blank        = 0x00000002,
	FTF_Initialise   = 0x00000004,
	FTF_Truncate     = 0x00000008
} FormatType;

typedef struct _FSSpace {
	QWORD UsedBytes;
	QWORD Capacity;
	BYTE *pBlockMap;
} FSSpace;

typedef enum _BlockType {
	BlockFree   = 0,
	BlockUsed   = 1,
	BlockFixed  = 2,
	BlockAbsent = 0xFF,
} BlockType;

typedef struct _FSTool {
	HBITMAP  ToolIcon;
	std::wstring ToolName;
	std::wstring ToolDesc;
} FSTool;

typedef struct _SidecarExport {
	BYTEString Filename;
	void *FileObj;
} SidecarExport;

typedef struct _SidecarImport {
	BYTEString Filename;
} SidecarImport;

typedef std::vector<FSTool> FSToolList;
typedef FSToolList::iterator FSToolIterator;

typedef struct _FSHint {
	FSIdentifier FSID;
	WORD  Confidence;
	WrapperIdentifier WrapperID;
} FSHint;

typedef std::vector<FSHint> FSHints;

#endif