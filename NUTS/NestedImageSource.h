#pragma once
#include "imagedatasource.h"

#include "Defs.h"

class NestedImageSource :
	public ImageDataSource
{
public:
	NestedImageSource( void *pFS, NativeFile *pSource, std::wstring Path );
	~NestedImageSource(void);

public:
	int WriteSector(long Sector, void *pSectorBuf, long SectorSize);
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

private:
	void *pSourceFS;

	std::wstring TempPath;

	NativeFile SourceObject;
};

