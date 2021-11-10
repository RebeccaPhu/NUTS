#include "stdafx.h"

// This code originally written by Arun at
// https://stackoverflow.com/questions/327718/how-to-list-physical-disks#18183115

#include <Windows.h>
#include <Setupapi.h>
#include <WinIoCtl.h>

#include "Defs.h"
#include "libfuncs.h"

#pragma comment( lib, "setupapi.lib" )

#include <iostream>

#include "RawDevices.h"

BYTEString ReadDeviceProductID( BYTE drive )
{
	BYTEString ProdID( (BYTE *) "" );

	HANDLE hPhysicalDriveIOCTL = 0;

	BYTE driveName [256];

	rsprintf (driveName, (BYTE *) "\\\\.\\PhysicalDrive%d", (int) drive );

	hPhysicalDriveIOCTL = CreateFileA(
		(char *) driveName, 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, NULL
	);

	if (hPhysicalDriveIOCTL != INVALID_HANDLE_VALUE)
    {
		STORAGE_PROPERTY_QUERY query;
        DWORD cbBytesReturned = 0;
		
		AutoBuffer buffer( 10000 );

        memset ( (void *) &query, 0, sizeof( query ) );

		query.PropertyId = StorageDeviceProperty;
		query.QueryType  = PropertyStandardQuery;

		memset( (BYTE *) buffer, 0, sizeof( 10000 ) );

		if ( DeviceIoControl( hPhysicalDriveIOCTL, IOCTL_STORAGE_QUERY_PROPERTY,
			&query, sizeof (query), (void *) (BYTE *) buffer, 10000, &cbBytesReturned, NULL ) )
		{         
			STORAGE_DEVICE_DESCRIPTOR * descrip = (STORAGE_DEVICE_DESCRIPTOR *) (BYTE *) buffer;

			ProdID = BYTEString( (BYTE *)  &buffer[ descrip->ProductIdOffset ] );
		}

		CloseHandle (hPhysicalDriveIOCTL);
	}

	return ProdID;
}

RawPaths GetRawDevices( )
{
	RawPaths devs;
	
	devs.clear();

	HDEVINFO diskClassDevices;
	GUID     diskClassDeviceInterfaceGuid = GUID_DEVINTERFACE_DISK;

	SP_DEVICE_INTERFACE_DATA         deviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData;

	DWORD requiredSize;
	DWORD deviceIndex;
	DWORD bytesReturned;

	STORAGE_DEVICE_NUMBER diskNumber;

	HANDLE disk = INVALID_HANDLE_VALUE;

	//
	// Get the handle to the device information set for installed
	// disk class devices. Returns only devices that are currently
	// present in the system and have an enabled disk device
	// interface.
	//

	diskClassDevices = SetupDiGetClassDevs( &diskClassDeviceInterfaceGuid,
                                            NULL,
                                            NULL,
                                            DIGCF_PRESENT |
                                            DIGCF_DEVICEINTERFACE );

	if ( diskClassDevices == INVALID_HANDLE_VALUE )
	{
		return devs;
	}

	deviceInterfaceDetailData = nullptr;

	ZeroMemory( &deviceInterfaceData, sizeof( SP_DEVICE_INTERFACE_DATA ) );

	deviceInterfaceData.cbSize = sizeof( SP_DEVICE_INTERFACE_DATA );

	deviceIndex = 0;

    while ( SetupDiEnumDeviceInterfaces( diskClassDevices,
                                         NULL,
                                         &diskClassDeviceInterfaceGuid,
                                         deviceIndex,
                                         &deviceInterfaceData ) ) {

        ++deviceIndex;

        SetupDiGetDeviceInterfaceDetail( diskClassDevices,
                                         &deviceInterfaceData,
                                         NULL,
                                         0,
                                         &requiredSize,
                                         NULL );
		
		if ( GetLastError() != ERROR_INSUFFICIENT_BUFFER )
		{
			continue;
		}

		if ( deviceInterfaceDetailData != nullptr )
		{
			free( deviceInterfaceDetailData );

			deviceInterfaceDetailData = nullptr;
		}

		deviceInterfaceDetailData = ( PSP_DEVICE_INTERFACE_DETAIL_DATA ) malloc( requiredSize );

		if ( deviceInterfaceDetailData == NULL )
		{
			continue;
		}

		ZeroMemory( deviceInterfaceDetailData, requiredSize );

		deviceInterfaceDetailData->cbSize = sizeof( SP_DEVICE_INTERFACE_DETAIL_DATA );

		if ( !SetupDiGetDeviceInterfaceDetail( diskClassDevices,
                                               &deviceInterfaceData,
                                               deviceInterfaceDetailData,
                                               requiredSize,
                                               NULL,
                                               NULL ) )
		{
			continue;
		}

		disk = CreateFile( deviceInterfaceDetailData->DevicePath,
							0,
							0,
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL );

		if ( disk == INVALID_HANDLE_VALUE )
		{
			continue;
		}

		if ( !DeviceIoControl( disk,
                              IOCTL_STORAGE_GET_DEVICE_NUMBER,
                              NULL,
                              0,
                              &diskNumber,
                              sizeof( STORAGE_DEVICE_NUMBER ),
                              &bytesReturned,
                              NULL ) )
		{
		}

        CloseHandle( disk );

        disk = INVALID_HANDLE_VALUE;

		std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring( (long long) diskNumber.DeviceNumber );

		devs.push_back( path );        
    }
    
    if ( diskClassDevices != INVALID_HANDLE_VALUE )
	{
        SetupDiDestroyDeviceInfoList( diskClassDevices );
    }

    if ( disk != INVALID_HANDLE_VALUE )
	{
        CloseHandle( disk );
    }

	if ( deviceInterfaceDetailData != nullptr )
	{
		free( deviceInterfaceDetailData );

		deviceInterfaceDetailData = nullptr;
	}

	return devs;
}