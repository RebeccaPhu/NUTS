#include "StdAfx.h"
#include "ImageDataSource.h"
#include "NUTSError.h"
#include "Defs.h"

#include <assert.h>

int	ImageDataSource::ReadSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize ) {
	FILE	*fFile;

	_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

	if ( fFile == nullptr )
		return NUTSError( 0x3F, L"Unable to open data source" );

	fflush(fFile);

	fseek(fFile, Sector * SectorSize, SEEK_SET);

	fread(pSectorBuf, 1, SectorSize, fFile);

	fclose(fFile);

	UpdateSize();

	return 0;
}

int ImageDataSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	FILE	*fFile;

	_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

	if ( fFile == nullptr )
		return NUTSError( 0x3F, L"Unable to open data source" );

	fflush(fFile);

	_fseeki64( fFile, Offset, SEEK_SET );

	fread( pBuffer, 1, Length, fFile );

	fclose(fFile);

	UpdateSize();

	return 0;
}

int ImageDataSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	FILE	*fFile;

	_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

	if ( fFile == nullptr )
		return NUTSError( 0x3F, L"Unable to open data source" );

	fflush(fFile);

	_fseeki64( fFile, Offset, SEEK_SET );

	fwrite( pBuffer, 1, Length, fFile );

	fclose(fFile);

	UpdateSize();

	return 0;
}

int	ImageDataSource::WriteSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize ) {
	FILE	*fFile;

	_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

	if ( fFile == nullptr )
		return NUTSError( 0x3F, L"Unable to open data source" );

	fflush(fFile);

	fseek(fFile, Sector * SectorSize, SEEK_SET);

	fwrite(pSectorBuf, 1, SectorSize, fFile);

	fclose(fFile);

	UpdateSize();

	return 0;
}

int ImageDataSource::Truncate( QWORD Length )
{
	HANDLE hFile;

	hFile = ::CreateFile(
		ImageSource.c_str(), 
		GENERIC_READ | GENERIC_WRITE, 
		NULL, 
		NULL, 
		OPEN_EXISTING, 
		FILE_ATTRIBUTE_NORMAL, 
		NULL
	); 

	LONG JesusWindows = (LONG) ( Length >> 32 );

	if ( hFile != INVALID_HANDLE_VALUE )
	{
		if ( SetFilePointer( hFile, (LONG) ( Length & 0xFFFFFFFF ), &JesusWindows, FILE_BEGIN ) == INVALID_SET_FILE_POINTER )
		{
			return NUTSError( 0x43, L"Truncate failed" );
		}

		if ( SetEndOfFile( hFile ) == FALSE )
		{
			return NUTSError( 0x44, L"Truncate failed" );
		}

		CloseHandle( hFile );
	}

	UpdateSize();

	return 0;
}


void ImageDataSource::UpdateSize( void )
{
	FILE	*f;
			
	_wfopen_s(&f, ImageSource.c_str(), L"rb");

	if (f) {
		fseek(f, 0, SEEK_END);

		PhysicalDiskSize	= _ftelli64(f);

		fclose(f);
	}
}