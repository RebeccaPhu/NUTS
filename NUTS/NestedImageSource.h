#pragma once
#include "imagedatasource.h"

#include <time.h>

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

	void Release (void )
	{
		FlushNest();

		DataSource::Release();
	}

	void FlushNest( void );

private:
	void *pSourceFS;

	std::wstring TempPath;

	NativeFile SourceObject;

	time_t DirtyTime;
	bool   Dirty;
	
};

