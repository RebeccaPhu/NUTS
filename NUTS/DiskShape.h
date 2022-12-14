#ifndef DISKSHAPE_H
#define DISKSHAPE_H

#include "stdafx.h"

#include <vector>

#include "NUTSTypes.h"

/* Physical disk definitions */
typedef struct _DiskShape {
	DWORD Tracks;
	WORD  Sectors;
	WORD  Heads;
	WORD  LowestSector;
	WORD  SectorSize;
	bool  InterleavedHeads;
} DiskShape;

typedef struct _TrackSection {
	BYTE  Value;
	BYTE  Repeats;
} TrackSection;

typedef struct _SectorDefinition {
	TrackSection GAP2;
	TrackSection GAP2PLL;
	TrackSection GAP2SYNC;
	BYTE         IAM;
	BYTE         Track;
	BYTE         Side;
	BYTE         SectorID;
	BYTE         SectorLength;
	BYTE         IDCRC;
	TrackSection GAP3;
	TrackSection GAP3PLL;
	TrackSection GAP3SYNC;
	BYTE         DAM;
	BYTE         Data[1024];
	BYTE         DATACRC;
	TrackSection GAP4;
	BYTE         TagData[ 32 ];
} SectorDefinition;

typedef enum _DiskDensity {
	SingleDensity  = 1,
	DoubleDensity  = 2,
	QuadDesnity    = 3,
	OctalDensity   = 4,
	CBM_GCR        = 5,
	Apple_GCR      = 6,
	UnknownDensity = 0xFF
} DiskDensity;

typedef struct _TrackDefinition {
	DiskDensity  Density;

	TrackSection GAP1;

	std::vector<SectorDefinition> Sectors;

	BYTE         GAP5;
} TrackDefinition;

typedef struct _DS_TrackDef {
	WORD  HeadID;
	DWORD TrackID;
	WORD  Sector1;
	std::vector< WORD > SectorSizes;
} DS_TrackDef;

typedef struct _DS_ComplexShape {
	DWORD Heads;
	DWORD Head1;
	DWORD Tracks;
	DWORD Track1;
	bool  Interleave;

	std::vector< DS_TrackDef > TrackDefs;
} DS_ComplexShape;

typedef std::vector<BYTE> SectorIDSet;

#endif