#pragma once

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "FileSystem.h"
#include "DataSource.h"

#include "ZIPDirectory.h"

class ZIPFile : public FileSystem
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

	int  ChangeDirectory( DWORD FileID );
	int  Parent();
	bool IsRoot();

private:
	ZIPDirectory *pDir;

	BYTE cpath[ 256 ];
};