#pragma once
#include "datasource.h"

class IDE8Source : public DataSource {
public:
	IDE8Source(DataSource *pDataSource) : DataSource() {
		pSource	= pDataSource;

		if ( pSource != nullptr )
		{
			DS_RETAIN( pSource );

			Flags = pSource->Flags;
		}

		PhysicalDiskSize	= pSource->PhysicalDiskSize /2;
	}

	virtual ~IDE8Source(void) {
		if ( pSource != nullptr )
		{
			DS_RELEASE( pSource );
		}
	}

	virtual int ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	virtual int WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	virtual int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	virtual int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	virtual int Truncate( QWORD Length )
	{
		return 0;
	}

	DataSource	*pSource;
};
