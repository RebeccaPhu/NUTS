#pragma once

#include "opencbm-plugin.h"
#include "CBMFunctions.h"

extern opencbm_plugin_exec_command_t   *opencbm_exec_command;
extern opencbm_plugin_identify_t       *opencbm_identify;
extern opencbm_plugin_driver_open_ex_t *opencbm_driver_open_ex;
extern opencbm_plugin_driver_close_t   *opencbm_driver_close;
extern opencbm_plugin_raw_write_t      *opencbm_raw_write;
extern opencbm_plugin_raw_read_t       *opencbm_raw_read;
extern opencbm_plugin_open_t           *opencbm_open;
extern opencbm_plugin_close_t          *opencbm_close;
extern opencbm_plugin_listen_t         *opencbm_listen;
extern opencbm_plugin_talk_t           *opencbm_talk;
extern opencbm_plugin_unlisten_t       *opencbm_unlisten;
extern opencbm_plugin_untalk_t         *opencbm_untalk;
extern opencbm_plugin_device_status_t  *opencbm_device_status;
extern opencbm_plugin_reset_t          *opencbm_reset;

extern bool OpenCBMLoaded;

void LoadOpenCBM();
void UnloadOpenCBM( void );
int OpenCBM_ReadBlock( BYTE Drive, TSLink TS, BYTE *BlockData );
int OpenCBM_WriteBlock( BYTE Drive, TSLink TS, BYTE *BlockData );
int OpenCBM_OpenDrive( DWORD Drive );
void OpenCBM_CloseDrive( DWORD Drive );

extern WORD CBMDeviceBits;

class DriveLock
{
public:
	DriveLock( CRITICAL_SECTION *pLock )
	{
		EnterCriticalSection( pLock );

		pSLock = pLock;
	}

	~DriveLock()
	{
		LeaveCriticalSection( pSLock );
	}

private:
	CRITICAL_SECTION *pSLock;
};

