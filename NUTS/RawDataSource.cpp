#include "StdAfx.h"
#include "RawDataSource.h"
#include "NUTSError.h"

#include <assert.h>

int	RawDataSource::ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize ) {
	QWORD RealOffset = (QWORD) Sector * (QWORD) SectorSize;

	return ReadRaw( RealOffset, SectorSize, pSectorBuf );
}

int RawDataSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	if ( hDevice == INVALID_HANDLE_VALUE )
	{
		return NUTSError( 0x30, L"No handle for device" );
	}

	LARGE_INTEGER p;

	p.QuadPart = Offset;

	if ( !SetFilePointerEx( hDevice, p, NULL, FILE_BEGIN ) )
	{
		return NUTSError( 0x3F, L"Unable to set file pointer on raw device" );
	}

	if ( !ReadFile( hDevice, (LPVOID) pBuffer, Length, NULL, NULL ) )
	{
		DWORD x = GetLastError();

		return NUTSError( 0x3E, L"Unable to read data from raw device" );
	}

	return 0;
}

int RawDataSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	if ( hDevice == INVALID_HANDLE_VALUE )
	{
		return NUTSError( 0x30, L"No handle for device" );
	}

	LARGE_INTEGER p;

	p.QuadPart = Offset;

	if ( !SetFilePointerEx( hDevice, p, NULL, FILE_BEGIN ) )
	{
		return NUTSError( 0x3F, L"Unable to set file pointer on raw device" );
	}

	if ( !WriteFile( hDevice, (LPVOID) pBuffer, Length, NULL, NULL ) )
	{
		return NUTSError( 0x3E, L"Unable to write data to raw device" );
	}

	return 0;
}

int	RawDataSource::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize ) {
	QWORD RealOffset = (QWORD) Sector * (QWORD) SectorSize;

	return WriteRaw( RealOffset, SectorSize, pSectorBuf );
}

int RawDataSource::PrepareFormat()
{
	DWORD returned;

	if ( IsLocked )
	{
		return 0;
	}

	if ( hDevice == INVALID_HANDLE_VALUE )
	{
		return NUTSError( 0x30, L"No handle for device" );
	}

	if ( !DeviceIoControl( hDevice, (DWORD) FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &returned, NULL ) )
	{
		return NUTSError( 0x3D, L"Unable to lock raw device" );
	}

	// Note we don't check the return result here, as the volume may not be mounted
	(void) DeviceIoControl( hDevice, (DWORD) FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &returned, NULL );

	IsLocked = true;

	return 0;
}

int RawDataSource::CleanupFormat()
{
	DWORD returned;

	// Note we don't check the return result here, as the volume may not be mounted
	(void) DeviceIoControl( hDevice, (DWORD) FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &returned, NULL );

	if ( !IsLocked )
	{
		return 0;
	}

	if ( hDevice == INVALID_HANDLE_VALUE )
	{
		return NUTSError( 0x30, L"No handle for device" );
	}

	if ( !DeviceIoControl( hDevice, (DWORD) FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &returned, NULL ) )
	{
		return NUTSError( 0x3D, L"Unable to lock raw device" );
	}

	IsLocked = false;

	return 0;
}
