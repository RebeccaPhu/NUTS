#pragma once
#include "imagedatasource.h"

#include "FileSystem.h"
#include "Defs.h"

class NestedImageSource :
	public ImageDataSource
{
public:
	NestedImageSource( FileSystem *pFS, NativeFile *pSource, std::wstring Path );
	~NestedImageSource(void);

public:
	int WriteSector(long Sector, void *pSectorBuf, long SectorSize);

private:
	FileSystem *pSourceFS;

	std::wstring TempPath;

	NativeFile SourceObject;
};

