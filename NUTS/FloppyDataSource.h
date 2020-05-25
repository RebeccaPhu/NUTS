#pragma once
#include "datasource.h"

#include <stdio.h>
#include <winioctl.h>
#include <string>

class FloppyDataSource :
	public DataSource
{
public:
	FloppyDataSource(std::wstring ImageFileName) : DataSource() {
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

	~FloppyDataSource(void) {
	}

	virtual int ReadSector(long Sector, void *pSectorBuf, long SectorSize);
	virtual int WriteSector(long Sector, void *pSectorBuf, long SectorSize);
	
	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	std::wstring( ImageSource );
};
