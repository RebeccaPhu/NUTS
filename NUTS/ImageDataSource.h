#pragma once
#include "datasource.h"
#include "libfuncs.h"

#include <stdio.h>
#include <winioctl.h>
#include <string>

class ImageDataSource :
	public DataSource
{
public:
	ImageDataSource(std::wstring ImageFileName) : DataSource() {
		ImageSource = ImageFileName;
  
		PhysicalDiskSize	= 0;

		FILE	*f;
			
		_wfopen_s(&f, ImageFileName.c_str(), L"rb");

		if (f) {
			fseek(f, 0, SEEK_END);

			PhysicalDiskSize	= _ftelli64(f);

			fclose(f);
		}

		Flags = DS_SupportsTruncate;
	}

	~ImageDataSource(void) {
	}

	virtual int ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	virtual int WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	
	virtual int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	virtual int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	virtual int Truncate( QWORD Length );

	std::wstring( ImageSource );

	virtual char *GetLocation()
	{
		return AString( (WCHAR *) ImageSource.c_str() );
	}

private:
	void UpdateSize( void );
};
