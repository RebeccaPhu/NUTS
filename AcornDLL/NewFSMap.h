#pragma once

#include "../NUTS/DataSource.h"
#include "Defs.h"

class NewFSMap
{
public:
	NewFSMap(DataSource *pDataSource) {
		pSource	= pDataSource;

		pSource->Retain();

		RootLoc = 0;

		SecSize = 1024; // Pre-emptively
		SingleZoneMapSector = 0;
	}

	~NewFSMap( void )
	{
		pSource->Release();
	}

	int	ReadFSMap();
	int	WriteFSMap();

	friend class ADFSEFileSystem;

	FileFragments GetFileFragments( DWORD DiscAddr );

	DWORD GetMatchingFragmentCount( DWORD FragmentID );
	DWORD GetSingleFragmentSize( DWORD FragmentID );
	DWORD SectorForSingleFragment( DWORD FragmentID );

	TargetedFileFragments GetWriteFileFragments( DWORD SecLength );

	void ReleaseFragment( DWORD FragmentID );

	BYTE ZoneCheck( BYTE *map_base );

	DWORD RootLoc;
	BYTE DiscName[ 11 ];

	WORD  SecSize;

protected:
	DataSource	*pSource;

private:
	BYTE  IDLen;
	BYTE  Skew;
	BYTE  BPMB;
	WORD  Zones;
	WORD  ZoneSpare;

	DWORD SingleZoneMapSector;

	FragmentList Fragments;

private:
	DWORD ReadBits( BYTE *map, DWORD offset, BYTE length );
	void  WriteBits( BYTE *map, DWORD offset, BYTE length, DWORD v );

	int   ReadSubMap( BYTE *pMap, DWORD NumBits, WORD zone, DWORD BitsOffset );
	int   WriteSubMap( BYTE *pMap, DWORD NumBits, WORD zone );

	DWORD GetUnusedFragmentID( void );

	void  ClaimFragment( Fragment_iter iFrag, DWORD SecLength, DWORD ProposedFragID );
	void  ClaimFragmentByOffset( DWORD FragOffset, DWORD SecLength, DWORD ProposedFragID );
	DWORD SectorForFragmentOffset( DWORD FragOffset );
};

