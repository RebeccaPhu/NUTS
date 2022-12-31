#pragma once

#include "..\nuts\datasource.h"

class ORICDSKSource :
	public DataSource
{
public:
	ORICDSKSource( DataSource *pRawSrc );
	~ORICDSKSource(void);

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
	DataSource *pRawSource;

	bool ValidDSK;

	DWORD NumSides;
	DWORD NumTracks;
	DWORD NumSectors;

	DWORD FormatTrackPosition;
};

