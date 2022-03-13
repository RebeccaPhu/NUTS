#pragma once

#include "TranslatedSector.h"
#include "../NUTS/DataSource.h"
#include "Defs.h"

class NewFSMap : public TranslatedSector
{
public:
	NewFSMap(DataSource *pDataSource) {
		pSource	= pDataSource;

		DS_RETAIN( pSource );

		RootLoc = 0;

		SecSize = 1024; // Pre-emptively
		
		ZoneMapSectors.clear();
		UsedExtraSector.clear();
	}

	~NewFSMap( void )
	{
		DS_RELEASE( pSource );
	}

	int	ReadFSMap();
	int	WriteFSMap();

	friend class ADFSEFileSystem;

	FileFragments GetFileFragments( DWORD DiscAddr );

	DWORD GetMatchingFragmentCount( DWORD FragmentID );
	DWORD GetSingleFragmentSize( DWORD FragmentID );
	DWORD SectorForSingleFragment( DWORD FragmentID );

	TargetedFileFragments GetWriteFileFragments( DWORD SecLength, DWORD ExistingFragID, bool UseExistingFragID );

	void ReleaseFragment( DWORD FragmentID );

	static BYTE ZoneCheck( BYTE *map_base, DWORD SSize );
	static BYTE BootBlockCheck( BYTE *block );

	void ConfigureDisk( FSIdentifier FSID );

	DWORD RootLoc;
	BYTE  DiscName[ 11 ];
	BYTE  BootOption;
	WORD  ZoneSpare;
	BYTE  SecsPerTrack;
	BYTE  LogSecSize;

	WORD  SecSize;

	BYTE  IDLen;
	WORD  BPMB;
	BYTE  LogBPMB;
	BYTE  BigFlag;
	DWORD RootSize;
	DWORD FormatVersion;

protected:
	DataSource	*pSource;

private:
	BYTE  Skew;
	WORD  Zones;

	std::map<WORD, DWORD> ZoneMapSectors;
	std::map<WORD, bool>  UsedExtraSector;

	FragmentList Fragments;

	DWORD IDsPerZone;

	/* Extra disc record bits that we don't actually use */
	BYTE  Heads;
	BYTE  Density;
	BYTE  LowSector;
	QWORD DiscSize;
	WORD  CycleID;
	BYTE  DiscType;

	/* These fields are E+/F+/Big Hard Disc */
	BYTE  LogShareSize;

private:
	DWORD ReadBits( BYTE *map, DWORD offset, BYTE length );
	void  WriteBits( BYTE *map, DWORD offset, BYTE length, DWORD v );

	int   ReadSubMap( BYTE *pMap, DWORD NumBits, WORD zone, DWORD BitsOffset );
	int   WriteSubMap( BYTE *pMap, DWORD NumBits, WORD zone );

	DWORD GetUnusedFragmentID( DWORD Zone );

	void  ClaimFragment( Fragment_iter iFrag, DWORD SecLength, DWORD ProposedFragID );
	void  ClaimFragmentByOffset( DWORD FragOffset, DWORD SecLength, DWORD ProposedFragID );
	DWORD SectorForFragmentOffset( DWORD FragOffset );

	void  ReadDiscRecord( BYTE *pRecord );
	void  WriteDiscRecord( BYTE *pRecord, bool Partial );
	void  ReorderWriteFragments( TargetedFileFragments *pFrags );
	void  IDExtend( DWORD *pID );
};

