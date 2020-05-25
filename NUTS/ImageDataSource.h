#pragma once
#include "datasource.h"

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
		LogicalDiskSize		= 0;

		FILE	*f;
			
		_wfopen_s(&f, ImageFileName.c_str(), L"rb");

		if (f) {
			fseek(f, 0, SEEK_END);

			PhysicalDiskSize	= _ftelli64(f);

			LogicalDiskSize		= PhysicalDiskSize;

			fclose(f);
		}
	}

	~ImageDataSource(void) {
	}

	virtual int ReadSector(long Sector, void *pSectorBuf, long SectorSize);
	virtual int WriteSector(long Sector, void *pSectorBuf, long SectorSize);
	
	virtual int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	virtual int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	std::wstring( ImageSource );
};
