#include "stdafx.h"

#ifndef APPLEDEFS_H
#define APPLEDEFS_H

#define FSID_MFS     L"Macintosh_FileSystem"
#define FSID_MFS_HD  L"Macintosh_Filesystem_OnHD"

extern const FTIdentifier       FILE_MACINTOSH;
extern const EncodingIdentifier ENCODING_MACINTOSH;

typedef struct _MFSVolumeRecord
{
	bool  Valid;
	DWORD InitStamp;
	DWORD ModStamp;
	WORD  VolAttrs;
	WORD  NumFiles;
	WORD  FirstBlock;
	WORD  DirBlockLen;
	WORD  AllocBlocks;
	DWORD AllocSize;
	DWORD AllocBytes;
	WORD  FirstAlloc;
	DWORD NextUnusedFile;
	WORD  UnusuedAllocs;

	BYTEString VolumeName;
} MFSVolumeRecord;

#define APPLE_TIME_OFFSET 2082841200

#endif