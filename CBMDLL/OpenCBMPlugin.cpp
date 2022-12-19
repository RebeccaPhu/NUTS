#include "stdafx.h"

#include "OpenCBMPlugin.h"
#include "CBMFunctions.h"
#include "../NUTS/Preference.h"
#include "../NUTS/NUTSError.h"

#include <time.h>

opencbm_plugin_exec_command_t   *opencbm_exec_command;
opencbm_plugin_identify_t       *opencbm_identify;
opencbm_plugin_driver_open_ex_t *opencbm_driver_open_ex;
opencbm_plugin_driver_close_t   *opencbm_driver_close;
opencbm_plugin_raw_write_t      *opencbm_raw_write;
opencbm_plugin_raw_read_t       *opencbm_raw_read;
opencbm_plugin_open_t           *opencbm_open;
opencbm_plugin_close_t          *opencbm_close;
opencbm_plugin_listen_t         *opencbm_listen;
opencbm_plugin_talk_t           *opencbm_talk;
opencbm_plugin_unlisten_t       *opencbm_unlisten;
opencbm_plugin_untalk_t         *opencbm_untalk;
opencbm_plugin_device_status_t  *opencbm_device_status;
opencbm_plugin_reset_t          *opencbm_reset;

bool OpenCBMLoaded = false;

HMODULE hCBM       = NULL;
CBM_FILE cbm_fd    = NULL;
WORD CBMDeviceBits = 0;

int DriveOpen[17];

CRITICAL_SECTION DLock;

time_t LoadTime = 0;

void LoadOpenCBM()
{
	/* Don't do this every time, as this is called from the plugin loader, and just slows things down */
	time_t now = time(NULL);

	if ( ( now - LoadTime ) <= 5 )
	{
		return;
	}

	LoadTime = now;

	OpenCBMLoaded = false;

	HMODULE hCBM = NULL;

	std::wstring FullModulePath = Preference( L"OpenCBMPath", L"C:\\Program Files\\opencbm" );

	FullModulePath += L"\\opencbm.dll";

	// Try the user supplied path first
	hCBM = LoadLibrary( FullModulePath.c_str() );

	// Try generically?
	if ( hCBM == NULL )
	{
		hCBM = LoadLibraryA( "opencbm.dll" );
	}

	if ( hCBM != NULL )
	{
		OutputDebugStringA( "Loading OpenCBM plugin\n" );
	}
	else
	{
		return;
	}

	OpenCBMLoaded = true;

	if ( ( opencbm_exec_command   = (opencbm_plugin_exec_command_t *)   GetProcAddress( hCBM, "cbm_exec_command" ) )   == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_identify       = (opencbm_plugin_identify_t *)       GetProcAddress( hCBM, "cbm_identify" ) )       == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_driver_open_ex = (opencbm_plugin_driver_open_ex_t *) GetProcAddress( hCBM, "cbm_driver_open_ex" ) ) == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_driver_close   = (opencbm_plugin_driver_close_t *)   GetProcAddress( hCBM, "cbm_driver_close" ) )   == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_raw_write      = (opencbm_plugin_raw_write_t *)      GetProcAddress( hCBM, "cbm_raw_write" ) )      == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_raw_read       = (opencbm_plugin_raw_read_t *)       GetProcAddress( hCBM, "cbm_raw_read" ) )       == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_open           = (opencbm_plugin_open_t *)           GetProcAddress( hCBM, "cbm_open" ) )           == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_close          = (opencbm_plugin_close_t *)          GetProcAddress( hCBM, "cbm_close" ) )          == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_listen         = (opencbm_plugin_listen_t *)         GetProcAddress( hCBM, "cbm_listen" ) )         == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_talk           = (opencbm_plugin_talk_t *)           GetProcAddress( hCBM, "cbm_talk" ) )           == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_unlisten       = (opencbm_plugin_unlisten_t *)       GetProcAddress( hCBM, "cbm_unlisten" ) )       == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_untalk         = (opencbm_plugin_untalk_t *)         GetProcAddress( hCBM, "cbm_untalk" ) )         == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_device_status  = (opencbm_plugin_device_status_t *)  GetProcAddress( hCBM, "cbm_device_status" ) )  == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_reset          = (opencbm_plugin_reset_t *)          GetProcAddress( hCBM, "cbm_reset" ) )          == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }

	if ( !OpenCBMLoaded )
	{
		return;
	}

	CBMDeviceBits = 0;

	if ( opencbm_driver_open_ex( &cbm_fd, NULL ) == 0 )
	{
		opencbm_reset( cbm_fd );

		for( BYTE device = 8; device < 16; device++ )
		{
			enum cbm_device_type_e device_type;

			const char *type_str;

			if( opencbm_identify( cbm_fd, device, &device_type, &type_str ) == 0 )
			{
				CBMDeviceBits |= 1 << device;
			}
		}
	}
	else
	{
		OpenCBMLoaded = false;
	}

	for ( BYTE d=0; d<17; d++ )
	{
		DriveOpen[ d ] = 0;
	}

	InitializeCriticalSection( &DLock );
}

void UnloadOpenCBM( void )
{
	if ( OpenCBMLoaded )
	{
		opencbm_driver_close( cbm_fd );
	}

	FreeLibrary( hCBM );

	DeleteCriticalSection( &DLock );
}

int OpenCBM_ReadBlock( BYTE Drive, TSLink TS, BYTE *BlockData )
{
	DriveLock Lock( &DLock );

	if ( !DriveOpen[ Drive ] )
	{
		OpenCBM_OpenDrive( Drive );
	}

	/* This is pretty much copied from OpenCBM */
	BYTE cmd[48];
	int rv = -1;

	rsprintf( cmd, "U1:2 0 %d %d", TS.Track, TS.Sector );

	if ( opencbm_exec_command( cbm_fd, Drive, cmd, 0) == 0)
	{
		rv = opencbm_device_status( cbm_fd, Drive, cmd, sizeof(cmd) );

		if ( rv == 0 )
		{
			if ( opencbm_exec_command( cbm_fd, Drive, "B-P2 0", 0 ) == 0 )
			{
				if ( opencbm_talk( cbm_fd, Drive, 2 ) == 0 )
				{
					rv = opencbm_raw_read( cbm_fd, BlockData, 256 );

					opencbm_untalk( cbm_fd );
				}
			}
		}
	}

	if ( rv != 256 )
	{
//		opencbm_reset( cbm_fd );

//		return NUTSError( 0x98, L"OpenCBM Error" );
	}

    return 0;
}

int OpenCBM_WriteBlock( BYTE Drive, TSLink TS, BYTE *BlockData )
{
	DriveLock Lock( &DLock );

	if ( !DriveOpen[ Drive ] )
	{
		OpenCBM_OpenDrive( Drive );
	}

    BYTE cmd[48];
    int  rv = 1;

    if( opencbm_exec_command( cbm_fd, Drive, "B-P2 0", 0 ) == 0 )
    {
        if( opencbm_listen( cbm_fd, Drive, 2 ) == 0 )
        {
            rv = opencbm_raw_write( cbm_fd, BlockData, 256 ) != 256;

			opencbm_unlisten( cbm_fd );

            if ( rv == 0 )
            {
				rsprintf( cmd, "U2:2 0 %d %d", TS.Track, TS.Sector );

                opencbm_exec_command( cbm_fd, Drive, cmd, 0 );

                rv = opencbm_device_status( cbm_fd, Drive, cmd, sizeof(cmd) );
            }
        }
    }

	if ( rv != 0 )
	{
		opencbm_reset( cbm_fd );

		return NUTSError( 0x98, L"OpenCBM Error" );
	}

    return rv;
}



int OpenCBM_OpenDrive( DWORD Drive )
{
	DriveLock Lock( &DLock );

	if ( DriveOpen[ Drive ] > 0 )
	{
		DriveOpen[ Drive ]++;

		return 0;
	}

	if ( cbm_fd == NULL )
	{
//		return NUTSError( 0x9A, L"OpenCBM unusable" );
	}

	char buf[ 255 ];

	opencbm_open( cbm_fd, (BYTE) Drive, 2, "#", 1 );

	int rv = opencbm_device_status( cbm_fd, (BYTE) Drive, buf, sizeof(buf) );

	if ( rv != 0 )
	{
		opencbm_reset( cbm_fd );

		return NUTSError( 0x99, L"OpenCBM Error" );
	}

	return 0;
}

void OpenCBM_CloseDrive( DWORD Drive )
{
	DriveLock Lock( &DLock );

	if ( DriveOpen[ Drive ] > 0 )
	{
		DriveOpen[ Drive ]--;
	}

	if ( DriveOpen[ Drive ] == 0 )
	{
//		if (cbm_fd != NULL )
		{
			opencbm_close( cbm_fd, (BYTE) Drive, 2 );

			opencbm_reset( cbm_fd );
		}
	}
}
