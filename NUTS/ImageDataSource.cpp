#include "StdAfx.h"
#include "ImageDataSource.h"
#include "Defs.h"

#include <assert.h>

int	ImageDataSource::ReadSector(long Sector, void *pSectorBuf, long SectorSize) {

	if ( ImageSource.substr( 0, 6 ) != L"\\\\.\\A:" )
	{
		FILE	*fFile;

		_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

		if (!fFile)
			return -1;

		fflush(fFile);

		fseek(fFile, Sector * SectorSize, SEEK_SET);

		fread(pSectorBuf, 1, SectorSize, fFile);

		fclose(fFile);
	} else {
		HANDLE hFloppy;

		hFloppy	= ::CreateFile(
					L"\\\\.\\A:", 
					GENERIC_READ | GENERIC_WRITE, 
					NULL, 
					NULL, 
					OPEN_EXISTING, 
					FILE_ATTRIBUTE_NORMAL, 
					NULL); 

		if (hFloppy != INVALID_HANDLE_VALUE) {
			DWORD	res;

			res	=	::SetFilePointer(hFloppy, Sector * SectorSize, NULL, FILE_BEGIN);
	
			DWORD	BytesRead	= 0;

			res	=	::ReadFile(hFloppy, pSectorBuf, SectorSize, &BytesRead, NULL);

			DWORD	le	= GetLastError();

			res	=	::CloseHandle(hFloppy);
		}
	}

	return 0;
}

int ImageDataSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	if ( ImageSource.substr( 0, 6 ) != L"\\\\.\\A:" )
	{
		FILE	*fFile;

		_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

		if (!fFile)
			return -1;

		fflush(fFile);

		_fseeki64( fFile, Offset, SEEK_SET );

		fread( pBuffer, 1, Length, fFile );

		fclose(fFile);
	}

	return 0;
}

int	ImageDataSource::WriteSector(long Sector, void *pSectorBuf, long SectorSize) {
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
