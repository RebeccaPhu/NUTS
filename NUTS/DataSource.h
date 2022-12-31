#pragma once

#define DS_SUCCESS 0x00000000

#include "NUTSTypes.h"
#include "DiskShape.h"
#include "BYTEString.h"
#include "libfuncs.h"

#include <vector>

#if 1
#define DS_RETAIN( x ) BYTE err[256]; rsprintf( err, "DS %016X Retain  %s:%d\n", x, __FILE__, __LINE__ ); OutputDebugStringA( (char *) err ); x->Retain()
#define DS_RELEASE( x ) BYTE err[256]; rsprintf( err, "DS %016X Release %s:%d\n", x, __FILE__, __LINE__ ); OutputDebugStringA( (char *) err ); x->Release()
#else
#define DS_RETAIN( x )  x->Retain();
#define DS_RELEASE( x ) x->Release();
#endif

typedef struct _DSWriteHint
{
	std::wstring HintName;
	BYTE Hint[ 32 ];
} DSWriteHint;

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
	virtual int SetComplexDiskShape( DS_ComplexShape shape );

	virtual int Truncate( QWORD Length ) = 0;

	/* Physical routines. Most sources ignore these */
	virtual SectorIDSet GetTrackSectorIDs( WORD Head, DWORD Track, bool Sorted );

	virtual int  PrepareFormat() { return 0; }
	virtual void StartFormat( ) { };
	virtual int  SeekTrack( WORD Track ) { return 0; }
	virtual int  SetHead( BYTE Head ) { FormatHead = Head; return 0; }
	virtual int  WriteTrack( TrackDefinition track ) { return 0; }
	virtual void EndFormat( void ) { };
	virtual int  CleanupFormat() { return 0; }

	virtual bool Valid() { return true; }

	__int64	PhysicalDiskSize;

	int RefCount;

	DWORD Flags;
	DWORD DataOffset;

	BYTEString SourceDesc;

	std::wstring Feedback;

	std::vector<DSWriteHint> WriteHints;

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
	virtual DWORD ResolveSectorSize( DWORD Head, DWORD Track, DWORD Sector );

	DiskShape MediaShape;

	DS_ComplexShape ComplexMediaShape;

	bool DiskShapeSet;
	bool ComplexDiskShape;

	BYTE FormatHead;

private:
	CRITICAL_SECTION RefLock;

};
