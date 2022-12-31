#pragma once

#include "..\nuts\datasource.h"

typedef enum _OricGeometry
{
	OricSequentialGeometry  = 1,
	OricInterleavedGeometry = 2,
} OricGeometry;

// Number of "unformatted" bytes in a track
#define MFM_DSK_TRACK_SIZE 6400

class MFMDISKWrapper :
	public DataSource
{
public:
	MFMDISKWrapper( DataSource *pRawSrc );
	~MFMDISKWrapper(void);

	int	ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );
	int	WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize );

	int ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );
	int WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer );

	int ReadSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf );
	int WriteSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf );

	int Truncate( QWORD Length );

	bool Valid() { return ValidDSK; }

	void StartFormat( );
	int  SeekTrack( WORD Track );
	int  WriteTrack( TrackDefinition track );
	void EndFormat( void );

private:
	void WriteGap( BYTE *buf, BYTE val, WORD repeats, DWORD &Index );
	int  FindSectorStart( DWORD Head, DWORD Track, DWORD Sector, DWORD &SectorSize );

private:
	DataSource *pRawSource;

	bool ValidDSK;

	DWORD NumHeads;
	DWORD NumTracks;

	OricGeometry Geometry;

	DWORD FormatTrackPosition;
	WORD  MaxTracks;
};

