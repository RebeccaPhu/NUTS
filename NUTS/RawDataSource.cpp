#include "StdAfx.h"
#include "RawDataSource.h"
#include "Defs.h"

#include <assert.h>

int	RawDataSource::ReadSector(long Sector, void *pSectorBuf, long SectorSize) {

	FILE	*fFile;

	_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

	if (!fFile)
		return -1;

	fflush(fFile);

	fseek(fFile, Sector * SectorSize, SEEK_SET);

	fread(pSectorBuf, 1, SectorSize, fFile);

	fclose(fFile);

	return 0;
}

int RawDataSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	FILE	*fFile;

	_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

	if (!fFile)
		return -1;

	fflush(fFile);

	_fseeki64( fFile, Offset, SEEK_SET );

	fread( pBuffer, 1, Length, fFile );

	fclose(fFile);

	return 0;
}

int	RawDataSource::WriteSector(long Sector, void *pSectorBuf, long SectorSize) {
	FILE	*fFile;

	_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

	if (!fFile)
		return -1;

	fflush(fFile);

	fseek(fFile, Sector * SectorSize, SEEK_SET);

	fwrite(pSectorBuf, 1, SectorSize, fFile);

	fclose(fFile);

//	assert(Sector != 0);

	return 0;
}
