#include "StdAfx.h"
#include "ImageDataSource.h"
#include "NUTSError.h"
#include "Defs.h"

#include <assert.h>

int	ImageDataSource::ReadSector(long Sector, void *pSectorBuf, long SectorSize) {
	FILE	*fFile;

	_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

	if ( fFile == nullptr )
		return NUTSError( 0x3F, L"Unable to open data source" );

	fflush(fFile);

	fseek(fFile, Sector * SectorSize, SEEK_SET);

	fread(pSectorBuf, 1, SectorSize, fFile);

	fclose(fFile);

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

	return 0;
}

int	ImageDataSource::WriteSector(long Sector, void *pSectorBuf, long SectorSize) {
	FILE	*fFile;

	_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

	if ( fFile == nullptr )
		return NUTSError( 0x3F, L"Unable to open data source" );

	fflush(fFile);

	fseek(fFile, Sector * SectorSize, SEEK_SET);

	fwrite(pSectorBuf, 1, SectorSize, fFile);

	fclose(fFile);

	return 0;
}
