#pragma once
#include "e:\nuts\nuts\datasource.h"
class OpenCBMSource :
	public DataSource
{
public:
	OpenCBMSource( DWORD DriveNum );
	~OpenCBMSource(void);

public:
	int ReadSector(long Sector, void *pSectorBuf, long SectorSize);
	int WriteSector(long Sector, void *pSectorBuf, long SectorSize);

	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	int Truncate( QWORD Length );

private:
	DWORD Drive;
};

