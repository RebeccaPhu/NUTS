#pragma once

#include "datasource.h"

#include "ISOSectorTypes.h"

#define RAW_ISO_SECTOR_SIZE 0x930

typedef struct _ISOWriteHint
{
	bool IsEOR;
	bool IsEOF;
} ISOWriteHint;

class ISORawSectorSource :
	public DataSource
{
public:
	ISORawSectorSource( DataSource *pRoot );
	~ISORawSectorSource(void);

	int ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	int WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );

	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	int Truncate( QWORD Length )
	{
		// Can't actually truncate, so just return success;
		return DS_SUCCESS;
	}

	void SetSectorType( ISOSectorType SType )
	{
		SectorType = SType;
	}

private:
	ISOSectorType SectorType;

	DataSource *pUpperSource;

	BYTE RawSector[ RAW_ISO_SECTOR_SIZE ];
	BYTE CheckSector[ RAW_ISO_SECTOR_SIZE ]; // used for sector verification

private:
	void AutoDetectType( void );
};

