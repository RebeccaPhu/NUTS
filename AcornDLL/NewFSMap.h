#pragma once

#include "../NUTS/DataSource.h"
#include "Defs.h"

class NewFSMap
{
public:
	NewFSMap(DataSource *pDataSource) {
		pSource	= pDataSource;

		RootLoc = 0;
	}

	int	ReadFSMap();
	int	WriteFSMap();

	friend class ADFSEFileSystem;

	FileFragments GetFileFragments( DWORD DiscAddr );

	BYTE ZoneCheck( BYTE *map_base );

	DWORD RootLoc;
	BYTE DiscName[ 11 ];

protected:
	DataSource	*pSource;


private:
	BYTE  IDLen;
	BYTE  Skew;
	BYTE  BPMB;
	WORD  Zones;
	WORD  ZoneSpare;

	FragmentList Fragments;

private:
	DWORD ReadBits( BYTE *map, DWORD offset, BYTE length );

	int  ReadSubMap( BYTE *pMap, DWORD NumBits, WORD zone, DWORD BitsOffset );
};

