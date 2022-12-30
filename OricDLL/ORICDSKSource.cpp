#include "StdAfx.h"
#include "ORICDSKSource.h"


ORICDSKSource::ORICDSKSource( DataSource *pRawSrc )
{
	pRawSource = pRawSrc;

	Feedback = L"Oric DSK Wrapper";
}


ORICDSKSource::~ORICDSKSource(void)
{
}

int	ORICDSKSource::ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	return 0;
}

int	ORICDSKSource::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	return 0;
}

int ORICDSKSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	return 0;
}

int ORICDSKSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	return 0;
}


int ORICDSKSource::Truncate( QWORD Length )
{
	return 0;
}


