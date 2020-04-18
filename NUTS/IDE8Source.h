#pragma once
#include "datasource.h"

class IDE8Source : public DataSource {
public:
	IDE8Source(DataSource *pDataSource) {
		pSource	= pDataSource;

		PhysicalDiskSize	= pSource->PhysicalDiskSize;
		LogicalDiskSize		= PhysicalDiskSize / 2;
	}

	~IDE8Source(void) {
		delete pSource;
	}

	int	ReadSector(long Sector, void *pSectorBuf, long SectorSize);
	int	WriteSector(long Sector, void *pSectorBuf, long SectorSize);

	virtual char *GetLocation() {
		return pSource->GetLocation();
	}

	DataSource	*pSource;
};
