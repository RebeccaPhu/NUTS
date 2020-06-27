#pragma once

#include "CPMBlockMap.h"
#include "Directory.h"

typedef enum _CPMFlags
{
	CF_UseHeaders = 0x00000001,
	CF_Force8bit  = 0x00000002,
	CF_Force16bit = 0x00000004,
} CPMFlags;

typedef struct _CPMDPB
{
	BYTE  ShortFSName[ 16 ];
	DWORD Encoding;
	DWORD FSFileType;
	BYTE  SysTracks;
	BYTE  SecsPerTrack;
	WORD  SecSize;
	DWORD FSID;
	DWORD DirSecs;
	DWORD ExtentSize;
	DWORD Flags;
	DWORD HeaderSize;
} CPMDPB;

void ImportCPMDirectoryEntry( NativeFile *pFile, BYTE *pEntry );
void ExportCPMDirectoryEntry( NativeFile *pFile, BYTE *pEntry );
bool CPMDirectoryEntryCMP( BYTE *pEntry1, BYTE *pEntry2 );

class CPMDirectory :
	public Directory
{
public:
	CPMDirectory( DataSource *pDataSource, CPMBlockMap *pMap, CPMDPB cpmdpb ) : Directory( pDataSource )
	{
		pBlockMap = pMap;

		dpb       = cpmdpb;

		DirSector = 0;
	}

	~CPMDirectory(void)
	{
	}

	virtual int ReadDirectory(void);
	virtual int WriteDirectory(void);

	virtual void ExtraReadDirectory( NativeFile *pFile ) { return; }

public:
	void LoadDirectory( BYTE *Buffer );
	void SaveDirectory( BYTE *Buffer );

public:
	DWORD       DirSector;

private:
	CPMBlockMap *pBlockMap;

	CPMDPB      dpb;

};

