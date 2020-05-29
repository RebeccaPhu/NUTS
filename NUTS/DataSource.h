#pragma once

#define DS_SUCCESS 0x00000000

#include "Defs.h"

class DataSource {
public:
	DataSource(void);
	virtual ~DataSource(void);

	virtual	int	ReadSector(long Sector, void *pSectorBuf, long SectorSize) = 0;
	virtual	int	WriteSector(long Sector, void *pSectorBuf, long SectorSize) = 0;

	virtual int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer ) = 0;
	virtual int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer ) = 0;

	virtual char *GetLocation()
	{
		return "";
	}

	__int64	PhysicalDiskSize;
	__int64	LogicalDiskSize;

	int RefCount;

public:
	void Retain( void )
	{
		RefCount++;
	}

	virtual void Release (void )
	{
		if ( RefCount > 0 )
		{
			RefCount--;
		}
	}
};
