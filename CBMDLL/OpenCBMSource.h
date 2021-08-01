#pragma once
#include "e:\nuts\nuts\datasource.h"
class OpenCBMSource :
	public DataSource
{
public:
	OpenCBMSource( DWORD DriveNum );
	~OpenCBMSource(void);

public:
	int ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	int WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );

	int ReadSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf );
	int WriteSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf );

	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	int Truncate( QWORD Length );

private:
	DWORD Drive;
};

