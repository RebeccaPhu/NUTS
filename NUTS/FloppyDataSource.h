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
		FILE	*f;
			
		_wfopen_s(&f, ImageFileName.c_str(), L"rb");

		if (f) {
			fseek(f, 0, SEEK_END);

			PhysicalDiskSize	= _ftelli64(f);

			fclose(f);
		}

		Flags = DS_SlowAccess | DS_SupportsLLF | DS_AlwaysLLF | DS_RawDevice;
	}

	~FloppyDataSource(void) {
	}

	virtual int ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	virtual int WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	
	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	virtual int Truncate( QWORD Length )
	{
		return 0;
	}

	std::wstring( ImageSource );
};
