#pragma once

#define DS_SUCCESS 0x00000000

#include "Defs.h"

class DataSource {
public:
	DataSource(void);
	virtual ~DataSource(void);

	virtual	int	ReadSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize ) = 0;
	virtual	int	WriteSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize ) = 0;

	virtual int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer ) = 0;
	virtual int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer ) = 0;

	virtual int Truncate( QWORD Length ) = 0;

	/* Physical routines. Most sources ignore these */
	virtual void StartFormat( DiskShape &shape ) { };
	virtual WORD GetSectorID( WORD Track, WORD Head, WORD Sector ) { return 0x00; }
	virtual WORD GetSectorID( DWORD Sector ) { return 0x00; }
	virtual int  SeekTrack( WORD Track ) { return 0; }
	virtual int  WriteTrack( TrackDefinition track ) { return 0; }
	virtual void EndFormat( void ) { };

	__int64	PhysicalDiskSize;

	int RefCount;

	DWORD Flags;

public:
	virtual void Retain( void )
	{
		EnterCriticalSection(&RefLock);
		RefCount++;
		LeaveCriticalSection(&RefLock);
	}

	virtual void Release (void )
	{
		EnterCriticalSection(&RefLock);
		if ( RefCount > 0 )
		{
			RefCount--;
		}
		LeaveCriticalSection(&RefLock);
	}
private:
	CRITICAL_SECTION RefLock;
};
