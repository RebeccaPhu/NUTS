#pragma once
#include "datasource.h"

class IDE8Source : public DataSource {
public:
	IDE8Source(DataSource *pDataSource) : DataSource() {
		pSource	= pDataSource;

		if ( pSource != nullptr )
		{
			pSource->Retain();
		}

		PhysicalDiskSize	= pSource->PhysicalDiskSize /2;
		LogicalDiskSize		= PhysicalDiskSize;
	}

	virtual ~IDE8Source(void) {
		if ( pSource != nullptr )
		{
			pSource->Release();
		}

		delete pSource;
	}

	int	ReadSector(long Sector, void *pSectorBuf, long SectorSize);
	int	WriteSector(long Sector, void *pSectorBuf, long SectorSize);
	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	virtual char *GetLocation() {
		return pSource->GetLocation();
	}

	DataSource	*pSource;
};
