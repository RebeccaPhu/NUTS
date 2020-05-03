#include "StdAfx.h"
#include "RootFileSystem.h"
#include "ImageDataSource.h"
#include "RawDataSource.h"
#include "FloppyDataSource.h"
#include "WindowsFileSystem.h"
#include "libfuncs.h"

BYTE *RootFileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE status[64];

	if ( pDirectory->Files[ FileIndex ].Type == FT_Floppy )
	{
		rsprintf( status, "Removable disk drive or memory card" );
	}
	else if ( pDirectory->Files[ FileIndex ].Type == FT_CDROM )
	{
		rsprintf( status, "Optical drive" );
	}
	else
	{
		rsprintf( status, "Hard disk drive" );
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
	else if ( pDirectory->Files[ FileIndex ].Type == FT_Floppy )
	{
		rsprintf( status, "Removable disk drive or memory card" );
	}
	else if ( pDirectory->Files[ FileIndex ].Type == FT_CDROM )
	{
		rsprintf( status, "Optical drive" );
	}
	else
	{
		rsprintf( status, "Hard disk drive" );
	}

	return status;
}

DataSource *RootFileSystem::FileDataSource( DWORD FileID )
{
	bool IsRaw = IsRawFS( UString( (char *) pDirectory->Files[ FileID ].Filename ) );

	OutputDebugString(L"RAW FS\n");

	char	DPath[64];
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

	DataSource *pSource = nullptr;

	if (PDN == -2)
	{
		sprintf_s(DPath, 63, "\\\\.\\A:");

		pSource = new FloppyDataSource( std::wstring( UString( DPath ) ) );
	}
	else if (PDN == -3)
	{
		sprintf_s(DPath, 63, "\\\\.\\B:");

		pSource = new FloppyDataSource( std::wstring( UString( DPath ) ) );
	}
	else
	{
		sprintf_s(DPath, 63, "\\\\.\\PhysicalDrive%d", PDN);

		pSource = new RawDataSource( std::wstring( UString( DPath ) ) );
	}

	OutputDebugStringA(DPath);

	if ( pSource != nullptr )
	{
		pDirectory->Files[ FileID ].Length = pSource->PhysicalDiskSize;
	}

	return pSource;
}

FileSystem *RootFileSystem::FileFilesystem( DWORD FileID )
{
	bool IsRaw = IsRawFS( UString( (char *) pDirectory->Files[ FileID ].Filename ) );

	if (!IsRaw) {
		OutputDebugString(L"Entering Windows FS\n");

		FileSystem	*newFS	= new WindowsFileSystem( UString( (char *) pDirectory->Files[ FileID ].Filename ) );

		newFS->EnterIndex       = 0xFFFFFFFF;
		newFS->pParentFS        = this;
		newFS->UseResolvedIcons = UseResolvedIcons;
		newFS->hMainWindow      = hMainWindow;
		newFS->hPaneWindow      = hPaneWindow;
		newFS->IsRaw            = false;

		return newFS;
	}

	return nullptr;
}
