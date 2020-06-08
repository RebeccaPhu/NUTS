#pragma once

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

		rstrncpy( cpath, (BYTE *) "/", 2 );
	}

	~ZIPFile( void )
	{
		delete pDir;
	}

public:
	BYTE *GetTitleString( NativeFile *pFile = nullptr );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *DescribeFile( DWORD FileIndex );

private:
	ZIPDirectory *pDir;

	BYTE cpath[ 256 ];
};