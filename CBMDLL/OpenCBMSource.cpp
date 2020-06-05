#include "StdAfx.h"
#include "OpenCBMSource.h"
#include "../NUTS/NUTSError.h"
#include "CBMFunctions.h"
#include "OpenCBMPlugin.h"

OpenCBMSource::OpenCBMSource( DWORD DriveNum )
{
	Drive = DriveNum;

	OpenCBM_OpenDrive( Drive );
}


OpenCBMSource::~OpenCBMSource(void)
{
	OpenCBM_CloseDrive( Drive );
}

int OpenCBMSource::ReadSector(long Sector, void *pSectorBuf, long SectorSize)
{
	TSLink TS = LinkForSector( Sector );

	if ( !OpenCBMLoaded )
	{
		return NUTSError( 0x93, L"OpenCBM not found" );
	}

	return OpenCBM_ReadBlock( Drive, TS, (BYTE *) pSectorBuf );
}

int OpenCBMSource::WriteSector(long Sector, void *pSectorBuf, long SectorSize)
{
	TSLink TS = LinkForSector( Sector );

	if ( !OpenCBMLoaded )
	{
		return NUTSError( 0x93, L"OpenCBM not found" );
	}

	return OpenCBM_WriteBlock( Drive, TS, (BYTE *) pSectorBuf );
}

int OpenCBMSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	return NUTSError( 0x91, L"Attempt to use RAW command with BLOCK based OpenCBM source (Software Bug)" );
}

int OpenCBMSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	return NUTSError( 0x91, L"Attempt to use RAW command with BLOCK based OpenCBM source (Software Bug)" );
}

int OpenCBMSource::Truncate( QWORD Length )
{
	return 0;
}