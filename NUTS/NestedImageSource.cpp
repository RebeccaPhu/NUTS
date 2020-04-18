#include "StdAfx.h"
#include "NestedImageSource.h"


NestedImageSource::NestedImageSource( FileSystem *pFS, NativeFile *pSource, std::wstring Path ) : ImageDataSource( Path )
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
	ImageDataSource::WriteSector( Sector, pSectorBuf, SectorSize );

	CTempFile FileObj( AString( (WCHAR *) TempPath.c_str() ) );

	FileObj.Keep();

	return pSourceFS->ReplaceFile( &SourceObject, FileObj );
}