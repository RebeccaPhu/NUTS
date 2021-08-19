#include "StdAfx.h"
#include "NestedImageSource.h"
#include "FileSystem.h"

#include <process.h>

NestedImageSource::NestedImageSource( void *pFS, NativeFile *pSource, std::wstring Path ) : ImageDataSource( Path )
{
	pSourceFS    = pFS;
	TempPath     = Path;
	Dirty        = false;

	if ( pSource != nullptr )
	{
		SourceObject = *pSource;
	}
}

NestedImageSource::~NestedImageSource(void)
{
	FlushNest();

	/* This will delete the unwanted temp file */
	CTempFile FileObj( TempPath.c_str() );
}

void NestedImageSource::FlushNest( void )
{
	time_t Now     = time( NULL );

	if ( ( ( Now - DirtyTime ) > 1 ) && ( Dirty ) )
	{
		if ( pSourceFS != nullptr )
		{
			FileSystem *pFS = (FileSystem *) pSourceFS;

			CTempFile FileObj( TempPath.c_str() );

			FileObj.Keep();

			pFS->ReplaceFile( &SourceObject, FileObj );

			DirtyTime = time( NULL );
			Dirty     = false;
		}
	}
}

int NestedImageSource::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	if ( ImageDataSource::WriteSectorLBA( Sector, pSectorBuf, SectorSize ) != DS_SUCCESS )
	{
		return -1;
	}

	FlushNest();

	DirtyTime = time( NULL );
	Dirty     = true;

	return 0;
}

int NestedImageSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	if ( ImageDataSource::WriteRaw( Offset, Length, pBuffer ) != DS_SUCCESS )
	{
		return -1;
	}

	FlushNest();

	DirtyTime = time( NULL );
	Dirty     = true;

	return 0;
}

int NestedImageSource::Truncate( QWORD Length )
{
	ImageDataSource::Truncate( Length );

	FlushNest();

	DirtyTime = time( NULL );
	Dirty     = true;

	return 0;
}

