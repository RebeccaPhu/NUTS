#pragma once
#include "datasource.h"

#include <stdio.h>
#include <winioctl.h>
#include <string>

class RawDataSource :
	public DataSource
{
public:
	RawDataSource(std::wstring RawFileName) : DataSource() {
		ImageSource = RawFileName;
  
		PhysicalDiskSize	= 0;
		LogicalDiskSize		= 0;

		DISK_GEOMETRY	disk;

		HANDLE hDevice = CreateFile(
			RawFileName.c_str(), 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, 0, NULL
		);

		if (hDevice != INVALID_HANDLE_VALUE) {
			DWORD returned;

			BOOL bResult = DeviceIoControl(
				hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
				&disk, sizeof(DISK_GEOMETRY), &returned, NULL
			);

			CloseHandle(hDevice);

			PhysicalDiskSize	 =	disk.BytesPerSector;
			PhysicalDiskSize	*=	disk.SectorsPerTrack;
			PhysicalDiskSize	*=	disk.TracksPerCylinder;
			PhysicalDiskSize	*=	disk.Cylinders.QuadPart;

			LogicalDiskSize		 =	PhysicalDiskSize;
		}
	}

	~RawDataSource(void) {
	}

	virtual int ReadSector(long Sector, void *pSectorBuf, long SectorSize);
	virtual int WriteSector(long Sector, void *pSectorBuf, long SectorSize);
	
	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	std::wstring( ImageSource );
};
