#include "stdafx.h"

#ifndef APPLEDEFS_H
#define APPLEDEFS_H

#define FSID_MFS     0x00000000
#define FSID_MFS_HD  0x00000001

extern DWORD FILE_MACINTOSH;
extern DWORD ENCODING_MACINTOSH;

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