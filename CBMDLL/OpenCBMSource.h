#pragma once
#include "e:\nuts\nuts\datasource.h"
class OpenCBMSource :
	public DataSource
{
public:
	OpenCBMSource( DWORD DriveNum );
	~OpenCBMSource(void);

public:
	int ReadSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	int WriteSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );

	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	int Truncate( QWORD Length );

private:
	DWORD Drive;
};

