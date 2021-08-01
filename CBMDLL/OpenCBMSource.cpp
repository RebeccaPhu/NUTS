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

int OpenCBMSource::ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	TSLink TS = LinkForSector( Sector );

	if ( !OpenCBMLoaded )
	{
		return NUTSError( 0x93, L"OpenCBM not found" );
	}

	return OpenCBM_ReadBlock( Drive, TS, (BYTE *) pSectorBuf );
}

int OpenCBMSource::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	TSLink TS = LinkForSector( Sector );

	if ( !OpenCBMLoaded )
	{
		return NUTSError( 0x93, L"OpenCBM not found" );
	}

	return OpenCBM_WriteBlock( Drive, TS, (BYTE *) pSectorBuf );
}

int OpenCBMSource::ReadSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	TSLink TS;

	TS.Track  = Track;
	TS.Sector = Sector;

	if ( !OpenCBMLoaded )
	{
		return NUTSError( 0x93, L"OpenCBM not found" );
	}

	return OpenCBM_ReadBlock( Drive, TS, (BYTE *) pSectorBuf );
}

int OpenCBMSource::WriteSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	TSLink TS;

	TS.Track  = Track;
	TS.Sector = Sector;

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