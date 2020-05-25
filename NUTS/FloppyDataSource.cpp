#include "StdAfx.h"
#include "FloppyDataSource.h"
#include "Defs.h"

#include <assert.h>

int	FloppyDataSource::ReadSector(long Sector, void *pSectorBuf, long SectorSize) {
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

	return 0;
}

int FloppyDataSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
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

		res	=	::SetFilePointer(hFloppy, (LONG) Offset, NULL, FILE_BEGIN);
	
		DWORD	BytesRead	= 0;

		res	=	::ReadFile(hFloppy, pBuffer, Length, &BytesRead, NULL);

		DWORD	le	= GetLastError();

		res	=	::CloseHandle(hFloppy);
	}

	return 0;
}

int FloppyDataSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
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

		res	=	::SetFilePointer(hFloppy, (LONG) Offset, NULL, FILE_BEGIN);
	
		DWORD	BytesWritten = 0;

		res	=	::WriteFile(hFloppy, pBuffer, Length, &BytesWritten, NULL);

		DWORD	le	= GetLastError();

		res	=	::CloseHandle(hFloppy);
	}

	return 0;
}

int	FloppyDataSource::WriteSector(long Sector, void *pSectorBuf, long SectorSize) {
	FILE	*fFile;

	_wfopen_s( &fFile, ImageSource.c_str(), L"r+b" );

	if (!fFile)
		return -1;

	fflush(fFile);

	fseek(fFile, Sector * SectorSize, SEEK_SET);

	fwrite(pSectorBuf, 1, SectorSize, fFile);

	fclose(fFile);

	return 0;
}
