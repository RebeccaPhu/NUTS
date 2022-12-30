#pragma once

#include "..\nuts\datasource.h"

class ORICDSKSource :
	public DataSource
{
public:
	ORICDSKSource( DataSource *pRawSrc );
	~ORICDSKSource(void);

	int	ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	int	WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );

	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	int Truncate( QWORD Length );

private:
	DataSource *pRawSource;
};

