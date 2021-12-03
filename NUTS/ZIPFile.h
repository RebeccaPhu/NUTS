#pragma once

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "FileSystem.h"
#include "DataSource.h"
#include "ZIPCommon.h"

#include "ZIPDirectory.h"
#include "FOP.h"

class ZIPFile : public FileSystem, ZIPCommon
{
public:
	ZIPFile( DataSource *pSrc ) : FileSystem( pSrc )
	{
		pSource = pSrc;

		pZDir = new ZIPDirectory( pSrc );

		pDirectory = (Directory *) pZDir;

		rstrncpy( cpath, (BYTE *) "", 2 );

		Flags = FSF_Supports_Dirs | FSF_Creates_Image | FSF_ArbitrarySize | FSF_DynamicSize | FSF_Uses_Extensions | FSF_Accepts_Sidecars;

		FSID = FSID_ZIP;

		TopicIcon = FT_Archive;
	}

	~ZIPFile( void )
	{
		delete pZDir;
	}

public:
	BYTE *GetTitleString( NativeFile *pFile, DWORD Flags );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *DescribeFile( DWORD FileIndex );

	int  ReadFile(DWORD FileID, CTempFile &store);
	int  WriteFile(NativeFile *pFile, CTempFile &store);
	int  DeleteFile( DWORD FileID );
	int  Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  );

	int  ReplaceFile(NativeFile *pFile, CTempFile &store);

	int  ChangeDirectory( DWORD FileID );
	int	 CreateDirectory( NativeFile *pDir, DWORD CreateFlags );
	int  Parent();
	bool IsRoot();

	int  Format_Process( DWORD FT, HWND hWnd );

	int Init(void) {
		pZDir->ProcessFOP = ProcessFOP;

		pDirectory->ReadDirectory();

		return 0;
	}

	DataSource *GetSource()
	{
		return pSource;
	}

private:
	ZIPDirectory *pZDir;

	BYTE cpath[ 256 ];

private:
	int RenameIncomingDirectory( NativeFile *pDir, Directory *pDirectory );
};

class ZIPFromTemp : public ZIPCommon
{
public:
	ZIPFromTemp( CTempFile &tmp )
	{
		pSource = new ImageDataSource( tmp.Name() );
	}

	~ZIPFromTemp( )
	{
		DS_RELEASE( pSource );
	}

	DataSource *GetSource()
	{
		return pSource;
	}

private:
	DataSource *pSource;
};