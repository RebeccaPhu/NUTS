#ifndef ISODEFS_H
#define ISODEFS_H

#include "Defs.h"

typedef struct _ISOVolDesc
{
	BYTE SysID[ 33 ];
	BYTE VolID[ 33 ];
	
	DWORD VolSize;
	WORD  VolSetSize; // Unused
	WORD  SeqNum;
	WORD  SectorSize;
	DWORD PathTableSize;
	DWORD LTableLoc;
	DWORD LTableLocOpt;
	DWORD MTableLoc;
	DWORD MTableLocOpt;

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

#endif