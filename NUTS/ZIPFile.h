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

		pDir = new ZIPDirectory( pSrc );

		pDirectory = (Directory *) pDir;

		rstrncpy( cpath, (BYTE *) "", 2 );

		Flags = FSF_Supports_Dirs | FSF_Creates_Image | FSF_ArbitrarySize | FSF_DynamicSize;
	}

	~ZIPFile( void )
	{
		delete pDir;
	}

public:
	BYTE *GetTitleString( NativeFile *pFile = nullptr );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *DescribeFile( DWORD FileIndex );

	int  ReadFile(DWORD FileID, CTempFile &store);
	int  WriteFile(NativeFile *pFile, CTempFile &store);
	int  DeleteFile( NativeFile *pFile, int FileOp );
	int  Rename( DWORD FileID, BYTE *NewName );

	int  ReplaceFile(NativeFile *pFile, CTempFile &store);

	int  ChangeDirectory( DWORD FileID );
	int	 CreateDirectory( BYTE *Filename, bool EnterAfter );
	int  Parent();
	bool IsRoot();

	DataSource *GetSource()
	{
		return pSource;
	}

private:
	ZIPDirectory *pDir;

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