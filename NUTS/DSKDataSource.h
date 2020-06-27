#pragma once
#include "datasource.h"

#include <vector>
#include <map>

typedef struct _DSK_DiskInfo
{
	BYTE Tracks;
	BYTE Sides;
	WORD TrackSize;

	WORD TrackSizes[ 80 ];

	BYTE Identifier[ 0x22 ];
	BYTE Creator[ 0x0E ];
} DSK_DiskInfo;

typedef struct _DSK_SectorInfo
{
	BYTE TrackID;
	BYTE SideID;
	BYTE SectorID;
	WORD SectorSize;
	BYTE FDC1;
	BYTE FDC2;

	DWORD ImageOffset;
} DSK_SectorInfo;

typedef struct _DSK_TrackInfo
{
	BYTE TrackNum;
	BYTE SideNum;
	WORD SecSize;
	BYTE NumSectors;
	BYTE GAP3;
	BYTE Filler;

	DWORD ImageOffset;

	std::map< BYTE, DSK_SectorInfo > Sectors;
} DSK_TrackInfo;

typedef struct _DSK
{
	DSK_DiskInfo DiskInfo;
	
	std::map<WORD, DSK_TrackInfo> Tracks;
} DSK;

class DSKDataSource :
	public DataSource
{
public:
	DSKDataSource( DataSource *pSrc )
	{
		pSource = pSrc;

		pSource->Retain();

		ReadDiskData();
	}

	~DSKDataSource(void)
	{
		pSource->Release();
	}

public:
	int ReadSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	int WriteSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );

	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	int Truncate( QWORD Length );

	WORD GetSectorID( WORD Track, WORD Head, WORD Sector );
	WORD GetSectorID( DWORD Sector );

private:
	DataSource *pSource;

	void ReadDiskData( void );

	DWORD OffsetForSector( DWORD Sector );
	DWORD OffsetForOffset( DWORD Offset );

private:
	DSK  DiskData;

	bool ValidDisk;
	bool Extended;
};

