#include "stdafx.h"

#include "OpenCBMPlugin.h"

opencbm_plugin_init_t         *opencbm_init;
opencbm_plugin_uninit_t       *opencbm_uninit;
opencbm_plugin_driver_open_t  *opencbm_driver_open;
opencbm_plugin_driver_close_t *opencbm_driver_close;
opencbm_plugin_raw_write_t    *opencbm_raw_write;
opencbm_plugin_raw_read_t     *opencbm_raw_read;
opencbm_plugin_open_t         *opencbm_open;
opencbm_plugin_close_t        *opencbm_close;
opencbm_plugin_listen_t       *opencbm_listen;
opencbm_plugin_talk_t         *opencbm_talk;
opencbm_plugin_unlisten_t     *opencbm_unlisten;
opencbm_plugin_untalk_t       *opencbm_untalk;

bool OpenCBMLoaded = false;

void LoadOpenCBM()
{
	OpenCBMLoaded = false;

	HMODULE hCBM = LoadLibraryA( "opencbm.dll" );

	if ( hCBM != NULL )
	{
		OutputDebugStringA( "Loading OpenCBM plugin" );
	}
	else
	{
		return;
	}

	if ( ( opencbm_init         = (opencbm_plugin_init_t *)         GetProcAddress( hCBM, "opencbm_init" ) )         == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_uninit       = (opencbm_plugin_uninit_t *)       GetProcAddress( hCBM, "opencbm_iminit" ) )       == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_driver_open  = (opencbm_plugin_driver_open_t *)  GetProcAddress( hCBM, "opencbm_driver_open" ) )  == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_driver_close = (opencbm_plugin_driver_close_t *) GetProcAddress( hCBM, "opencbm_driver_close" ) ) == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_raw_write    = (opencbm_plugin_raw_write_t *)    GetProcAddress( hCBM, "opencbm_raw_write" ) )    == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_raw_read     = (opencbm_plugin_raw_read_t *)     GetProcAddress( hCBM, "opencbm_raw_read" ) )     == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_open         = (opencbm_plugin_open_t *)         GetProcAddress( hCBM, "opencbm_open" ) )         == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_close        = (opencbm_plugin_close_t *)        GetProcAddress( hCBM, "opencbm_close" ) )        == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_listen       = (opencbm_plugin_listen_t *)       GetProcAddress( hCBM, "opencbm_listen" ) )       == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_talk         = (opencbm_plugin_talk_t *)         GetProcAddress( hCBM, "opencbm_talk" ) )         == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_unlisten     = (opencbm_plugin_unlisten_t *)     GetProcAddress( hCBM, "opencbm_unlisten" ) )     == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }
	if ( ( opencbm_untalk       = (opencbm_plugin_untalk_t *)       GetProcAddress( hCBM, "opencbm_untalk" ) )       == NULL ) { FreeLibrary( hCBM ); OpenCBMLoaded = false; }

	OpenCBMLoaded = true;
}