#pragma once
#include "datasource.h"

class MemorySource :
	public DataSource
{
public:
	MemorySource( BYTE *pDataSet, DWORD DataSize );
	~MemorySource(void);

public:
	int ReadSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	int WriteSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );

	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	int Truncate( QWORD Length );

private:
	BYTE *pData;
	DWORD Size;
};

