#include "StdAfx.h"
#include "RootFileSystem.h"
#include "ImageDataSource.h"
#include "RawDataSource.h"
#include "FloppyDataSource.h"
#include "MemorySource.h"
#include "WindowsFileSystem.h"
#include "ROMDisk.h"
#include "libfuncs.h"
#include "RawDevices.h"

#include <ShlObj.h>

BYTE *RootFileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE status[64];

	if ( pDirectory->Files[ FileIndex ].Icon == FT_Floppy )
	{
		rsprintf( status, "Removable disk drive or memory card" );
	}
	else if ( pDirectory->Files[ FileIndex ].Icon == FT_CDROM )
	{
		rsprintf( status, "Optical drive" );
	}
	else if ( pDirectory->Files[ FileIndex ].Icon == FT_Directory )
	{
		rsprintf( status, "Library folder" );
	}
	else if ( pDirectory->Files[ FileIndex ].Icon == FT_ROMDisk )
	{
		rsprintf( status, "ROM Disk Scratch Storage" );
	}
	else
	{
		if ( pDirectory->Files[ FileIndex ].Attributes[ 2 ] == ROOT_OBJECT_WINDOWS_VOLUME )
		{
			rsprintf( status, "Hard Disk, Windows Volume" );
		}
		else
		{
			rsprintf( status, "Hard Disk: %s", (BYTE *) ReadDeviceProductID( (BYTE) pDirectory->Files[ FileIndex ].Attributes[ 1 ] ) );
		}
	}

	return status;
}

BYTE *RootFileSystem::GetStatusString( int FileIndex, int SelectedItems )
{
	static BYTE status[64];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "N.U.T.S." );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d Items Selected", SelectedItems );
	}
	else if ( pDirectory->Files[ FileIndex ].Icon == FT_Floppy )
	{
		rsprintf( status, "Removable disk drive or memory card" );
	}
	else if ( pDirectory->Files[ FileIndex ].Icon == FT_CDROM )
	{
		rsprintf( status, "Optical drive" );
	}
	else if ( pDirectory->Files[ FileIndex ].Icon == FT_Directory )
	{
		rsprintf( status, "Library folder" );
	}
	else if ( pDirectory->Files[ FileIndex ].Icon == FT_ROMDisk )
	{
		rsprintf( status, "ROM Disk Scratch Storage" );
	}
	else
	{
		if ( pDirectory->Files[ FileIndex ].Attributes[ 2 ] == ROOT_OBJECT_WINDOWS_VOLUME )
		{
			rsprintf( status, "Hard Disk, Windows Volume" );
		}
		else
		{
			rsprintf( status, "Hard Disk: %s", (BYTE *) ReadDeviceProductID( (BYTE) pDirectory->Files[ FileIndex ].Attributes[ 1 ] ) );
		}
	}

	return status;
}

DataSource *RootFileSystem::FileDataSource( DWORD FileID )
{
	if ( pDirectory->Files[ FileID ].Icon == FT_Directory )
	{
		return nullptr;
	}

	DataSource *pDataSource;

	BYTE DPath[64];

	if ( pDirectory->Files[ FileID ].Attributes[ 1 ] = ROOT_OBJECT_HOOK )
	{
		RootHook hook = pRootDirectory->HookPairs[ FileID ];

		for ( RootHookInvocations::iterator iInvoke = hook.Invocations.begin(); iInvoke != hook.Invocations.end(); iInvoke++ )
		{
			if ( ! ( iInvoke->Flags & RHF_CreatesDataSource ) )
			{
				continue;
			}

			DataSource *pSource = new MemorySource( iInvoke->HookData, 32 );

			// TODO: Load data source from plugin

			DS_RELEASE( pSource );

			return pSource;
		}
	}

	if ( pDirectory->Files[ FileID ].Attributes[ 2 ] == ROOT_OBJECT_RAW_DEVICE )
	{
		rsprintf( DPath, "\\\\.\\PhysicalDrive%d", pDirectory->Files[ FileID ].Attributes[ 1 ] );

		BYTEString DL( (BYTE *) pDirectory->Files[ FileID ].Filename, 6 );

		DL[ 3 ] = ' ';
		DL[ 4 ] = 254;
		DL[ 5 ] = ' ';

		pDataSource = new RawDataSource( std::wstring( UString( (char *) DPath ) ), DL );

		return pSource;
	}

	bool IsRaw = IsRawFS( UString( (char *) pDirectory->Files[ FileID ].Filename ) );

	OutputDebugString(L"RAW FS\n");

	int		PDN	= PhysicalDrive( (char *) pDirectory->Files[ FileID ].Filename );

	if (PDN == -1) {
		MessageBox( hPaneWindow,
			L"Unable to access raw file system.\n\nAre you running as Administrator?",
			L"NUTS FileSystem Probe", MB_OK|MB_ICONSTOP
		);

		return nullptr;
	}

	memset(DPath, 0, 64);

	SetErrorMode( SEM_FAILCRITICALERRORS );

	if (PDN == -2)
	{
		rsprintf( DPath, "\\\\.\\A:" );

		pDataSource = new FloppyDataSource( std::wstring( UString( (char *) DPath ) ) );
	}
	else if (PDN == -3)
	{
		rsprintf( DPath, "\\\\.\\B:" );

		pDataSource = new FloppyDataSource( std::wstring( UString( (char *) DPath ) ) );
	}
	else
	{
		rsprintf( DPath, "\\\\.\\PhysicalDrive%d", PDN );

		BYTEString DL( (BYTE *) pDirectory->Files[ FileID ].Filename, 6 );

		DL[ 3 ] = ' ';
		DL[ 4 ] = 254;
		DL[ 5 ] = ' ';

		pDataSource = new RawDataSource( std::wstring( UString( (char *) DPath ) ), DL );
	}

	OutputDebugStringA( (char *) DPath );

	if ( pSource != nullptr )
	{
		pDirectory->Files[ FileID ].Length = pSource->PhysicalDiskSize;
	}

	return pDataSource;
}

FileSystem *RootFileSystem::FileFilesystem( DWORD FileID )
{
	if ( pDirectory->Files[ FileID ].Attributes[ 2 ] == ROOT_OBJECT_ROMDISK )
	{
		FileSystem *pFS = (FileSystem *) new ROMDisk( nullptr );

		return pFS;
	}

	if ( pDirectory->Files[ FileID ].Attributes[ 2 ] == ROOT_OBJECT_SPECIAL_FOLDER )
	{
		WindowsFileSystem *pWFS = nullptr;

		int fIndex = pDirectory->Files[ FileID].Attributes[ 0 ];

		WCHAR path[ MAX_PATH + 1];

		if ( SHGetFolderPath( NULL, RootDirectory::Folders[ fIndex ].FolderID, NULL, SHGFP_TYPE_CURRENT, path ) == S_OK )
		{
			pWFS = new WindowsFileSystem( std::wstring( path ) );
		}

		return pWFS;
	}

	if ( pDirectory->Files[ FileID ].Attributes[ 2 ] == ROOT_OBJECT_HOOK )
	{
		RootHook hook = pRootDirectory->HookPairs[ FileID ];

		for ( RootHookInvocations::iterator iInvoke = hook.Invocations.begin(); iInvoke != hook.Invocations.end(); iInvoke++ )
		{
			if ( ! ( iInvoke->Flags & RHF_CreatesFileSystem ) )
			{
				continue;
			}

			DataSource *pSource = new MemorySource( iInvoke->HookData, 32 );

			FileSystem *newFS = FSPlugins.LoadFS( iInvoke->HookFSID, pSource );

			DS_RELEASE( pSource );

			return newFS;
		}
	}

	bool IsRaw = IsRawFS( UString( (char *) pDirectory->Files[ FileID ].Filename ) );

	if (!IsRaw) {
		OutputDebugString(L"Entering Windows FS\n");

		FileSystem	*newFS	= new WindowsFileSystem( UString( (char *) pDirectory->Files[ FileID ].Filename ) );

		newFS->IsRaw = false;

		return newFS;
	}

	return nullptr;
}
