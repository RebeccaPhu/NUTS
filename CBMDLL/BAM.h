#pragma once

#include "../NUTS/DataSource.h"

typedef struct _TSLink
{
	BYTE Track;
	BYTE Sector;
} TSLink;

class BAM
{
public:
	BAM( DataSource *pSrc )
	{
		pSource = pSrc;

		DS_RETAIN( pSource );

		DiskName[ 0 ] = 0;
		DiskID[ 0 ]  = '0';
		DiskID[ 1 ]  = '0';
		DOSID[ 0 ]   = '2';
		DOSID[ 1 ]   = 'A';
		DosVer       = 'A';

		IsOpenCBM    = false;
		Drive        = 0;

		ZeroMemory( BAMData, sizeof( BAMData ) );
	}

	~BAM(void)
	{
		DS_RELEASE( pSource );
	}

public:
	int    ReadBAM( void );
	TSLink GetFreeTS( void );
	DWORD  CountFreeSectors( void );
	void   ReleaseSector( BYTE Track, BYTE Sector );
	void   OccupySector( BYTE Track, BYTE Sector );
	bool   IsFreeBlock( BYTE Track, BYTE Sector );
	int    WriteBAM( void );

public:
	BYTE DiskName[ 16 ];
	BYTE DiskID[ 2 ];
	BYTE DOSID[ 2 ];

	BYTE DosVer;
	bool IsOpenCBM;
	BYTE Drive;

private:
	DataSource *pSource;

	DWORD BAMData[ 41 ][ 4 ];
};

