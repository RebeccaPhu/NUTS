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
	}

	virtual ~IDE8Source(void) {
		if ( pSource != nullptr )
		{
			pSource->Release();
		}
	}

	virtual int ReadSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	virtual int WriteSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	virtual int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	virtual int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	virtual int Truncate( QWORD Length )
	{
		return 0;
	}

	DataSource	*pSource;
};
