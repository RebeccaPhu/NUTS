#pragma once

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "FileSystem.h"
#include "DataSource.h"
#include "ZIPCommon.h"

#include "ZIPDirectory.h"

class ZIPFile : public FileSystem, ZIPCommon
{
public:
	ZIPFile( DataSource *pSrc ) : FileSystem( pSrc )
	{
		pSource = pSrc;

		pZDir = new ZIPDirectory( pSrc );

		pDirectory = (Directory *) pZDir;

		rstrncpy( cpath, (BYTE *) "", 2 );

		Flags = FSF_Supports_Dirs | FSF_Creates_Image | FSF_ArbitrarySize | FSF_DynamicSize | FSF_Uses_Extensions;

		FSID = FSID_ZIP;

		TopicIcon = FT_Archive;
	}

	~ZIPFile( void )
	{
		delete pZDir;
	}

public:
	BYTE *GetTitleString( NativeFile *pFile = nullptr );
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

	DataSource *GetSource()
	{
		return pSource;
	}

private:
	ZIPDirectory *pZDir;

	BYTE cpath[ 256 ];
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
		pSource->Release();
	}

	DataSource *GetSource()
	{
		return pSource;
	}

private:
	DataSource *pSource;
};