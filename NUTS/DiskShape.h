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
	WORD  TrackInterleave;
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
} SectorDefinition;

typedef enum _DiskDensity {
	SingleDensity = 1,
	DoubleDensity = 2,
	QuadDesnity   = 3,
	OctalDensity  = 4,
} DiskDensity;

typedef struct _TrackDefinition {
	DiskDensity  Density;

	TrackSection GAP1;

	std::vector<SectorDefinition> Sectors;

	BYTE         GAP5;
} TrackDefinition;

#endif