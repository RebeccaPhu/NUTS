#pragma once

#include "DataSource.h"

class OffsetDataSource : public DataSource
{
public:
	OffsetDataSource( DWORD Offset, DataSource *pSource );
	~OffsetDataSource(void);

public:
	virtual	int	ReadSector(long Sector, void *pSectorBuf, long SectorSize);
	virtual	int	WriteSector(long Sector, void *pSectorBuf, long SectorSize);

	virtual int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	virtual int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	virtual int Truncate( QWORD Length );

	DataSource *pSrc;

private:
	DWORD SourceOffset;
};

