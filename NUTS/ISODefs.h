#ifndef ISODEFS_H
#define ISODEFS_H

#include "Defs.h"

typedef struct _ISOVolDesc
{
	DWORD VolumeLBN;
	BYTE SysID[ 33 ];
	BYTE VolID[ 33 ];
	
	DWORD VolSize;
	WORD  VolSetSize; // Unused
	WORD  SeqNum;
	WORD  SectorSize;
	DWORD PathTableSize;
	DWORD LTableLoc;
	DWORD LTableLocOpt;
	DWORD LTableLocOptA;
	DWORD LTableLocOptB;
	DWORD MTableLoc;
	DWORD MTableLocOpt;
	DWORD MTableLocOptA;
	DWORD MTableLocOptB;

	BYTE  DirectoryRecord[ 34 ];

	BYTE  VolSetID[ 129 ];
	BYTE  Publisher[ 129 ];
	BYTE  Preparer[ 129 ];
	BYTE  AppID[ 129 ];
	BYTE  CopyrightFilename[ 38 ];
	BYTE  AbstractFilename[ 38 ];
	BYTE  BibliographicFilename[ 38 ];

	BYTE  CreationTime[ 18 ];
	BYTE  ModTime[ 18 ];
	BYTE  ExpireTime[ 18 ];
	BYTE  EffectiveTime[ 18 ];
	BYTE  ApplicationBytes[ 512 ];
} ISOVolDesc;

#define ISOATTR_START_EXTENT 16
#define ISOATTR_REVISION     17
#define ISOATTR_FORK_EXTENT  18
#define ISOATTR_FORK_LENGTH  19
#define ISOATTR_TIMESTAMP    20
#define ISOATTR_VOLSEQ       21


// Defines a sector to remove and the resulting shift.
typedef struct _ISOSector
{
	DWORD ID;
	DWORD Shift;
} ISOSector;

typedef std::vector<ISOSector> ISOSectorList;

#define ISO_RESTRUCTURE_ADD_SECTORS 0
#define ISO_RESTRUCTURE_REM_SECTORS 1
#define ISO_JOB_REMOVE_JOLIET       2
#define ISO_JOB_ADD_JOLIET          3
#define ISO_JOB_SCAN_SIZE           4

typedef struct _ISOJob
{
	void          *pSource;
	ISOSectorList Sectors;
	BYTE          IsoOp;
	DWORD         SectorSize;
	DWORD         FSID;
	HWND          ProgressWnd;
	int           Result;
	ISOVolDesc    *pVolDesc;
} ISOJob;

#endif