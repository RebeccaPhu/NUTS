#pragma once

#define DS_SUCCESS 0x00000000

#include "Defs.h"

#include <vector>

typedef struct _DS_TrackDef {
	DWORD Sectors;
	DWORD Sector1;
} DS_TrackDef;

typedef struct _DS_ComplexShape {
	DWORD Heads;
	DWORD Head1;
	DWORD Tracks;
	DWORD Track1;
	DWORD SectorSize;
	bool  Interleave;

	std::vector< DS_TrackDef > TrackDefs;
} DS_ComplexShape;

typedef std::vector<WORD> SectorIDSet;

class DataSource {
public:
	DataSource(void);
	virtual ~DataSource(void);

	virtual	int	ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize ) = 0;
	virtual	int	WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize ) = 0;

	virtual int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer ) = 0;
	virtual int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer ) = 0;

	virtual int ReadSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf );
	virtual int WriteSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf );

	virtual int SetDiskShape( DiskShape shape );
	virtual int DataSource::SetComplexDiskShape( DS_ComplexShape shape );

	virtual int Truncate( QWORD Length ) = 0;

	/* Physical routines. Most sources ignore these */
	virtual SectorIDSet GetTrackSectorIDs( WORD Head, DWORD Track, bool Sorted );
	virtual void StartFormat( DiskShape &shape ) { };
	virtual int  SeekTrack( WORD Track ) { return 0; }
	virtual int  WriteTrack( TrackDefinition track ) { return 0; }
	virtual void EndFormat( void ) { };

	__int64	PhysicalDiskSize;

	int RefCount;

	DWORD Flags;
	DWORD DataOffset;

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

protected:
	virtual QWORD ResolveSector( DWORD Head, DWORD Track, DWORD Sector );

	DiskShape MediaShape;

	DS_ComplexShape ComplexMediaShape;

	bool DiskShapeSet;
	bool ComplexDiskShape;

private:
	CRITICAL_SECTION RefLock;

};
