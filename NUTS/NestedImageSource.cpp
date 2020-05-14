#include "StdAfx.h"
#include "NestedImageSource.h"
#include "FileSystem.h"

NestedImageSource::NestedImageSource( void *pFS, NativeFile *pSource, std::wstring Path ) : ImageDataSource( Path )
{
	pSourceFS    = pFS;
	TempPath     = Path;
	SourceObject = *pSource;
}

NestedImageSource::~NestedImageSource(void)
{
}

int NestedImageSource::WriteSector(long Sector, void *pSectorBuf, long SectorSize)
{
	FileSystem *pFS = (FileSystem *) pSourceFS;

	if ( ImageDataSource::WriteSector( Sector, pSectorBuf, SectorSize ) != DS_SUCCESS )
	{
		return -1;
	}

	CTempFile FileObj( TempPath.c_str() );

	FileObj.Keep();

	return pFS->ReplaceFile( &SourceObject, FileObj );
}