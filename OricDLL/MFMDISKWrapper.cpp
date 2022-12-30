#include "StdAfx.h"
#include "MFMDISKWrapper.h"


MFMDISKWrapper::MFMDISKWrapper( DataSource *pRawSrc ) : DataSource()
{
	pRawSource = pRawSrc;

	Feedback = L"Oric MFM Wrapper";
}


MFMDISKWrapper::~MFMDISKWrapper(void)
{
}

int	MFMDISKWrapper::ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	return 0;
}

int	MFMDISKWrapper::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	return 0;
}

int MFMDISKWrapper::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	return 0;
}

int MFMDISKWrapper::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	return 0;
}


int MFMDISKWrapper::Truncate( QWORD Length )
{
	return 0;
}