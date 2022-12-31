#include "StdAfx.h"
#include "MFMDISKWrapper.h"
#include "..\NUTS\NUTSError.h"
#include "..\NUTS\NUTSFlags.h"

MFMDISKWrapper::MFMDISKWrapper( DataSource *pRawSrc ) : DataSource()
{
	pRawSource = pRawSrc;

	Feedback = L"Oric MFM Wrapper";

	Flags = DS_SupportsTruncate | DS_SupportsLLF | DS_AlwaysLLF;

	ValidDSK = false;

	if ( pRawSource != nullptr )
	{
		DS_RETAIN( pRawSource );

		BYTE Buf[ 256 ];

		if ( pRawSource->ReadRaw( 0, 0x100, Buf ) == DS_SUCCESS )
		{
			if ( memcmp( (void *) Buf, (void *) "MFM_DISK", 8 ) == 0 )
			{
				ValidDSK = true;

				NumHeads    = LEDWORD( &Buf[ 0x08 ] );
				NumTracks   = LEDWORD( &Buf[ 0x0C ] );

				DWORD dwGeo = LEDWORD( &Buf[ 0x10 ] );

				PhysicalDiskSize = NumHeads * NumTracks * 20 * 0x100; // Ish

				if ( dwGeo == 0x0000001 )
				{
					Geometry = OricSequentialGeometry;
				}
				else if ( dwGeo == 0x00000002 )
				{
					Geometry = OricInterleavedGeometry;
				}
				else
				{
					ValidDSK = false;
				}
			}
		}
	}
}


MFMDISKWrapper::~MFMDISKWrapper(void)
{
	if ( pRawSource != nullptr )
	{
		DS_RELEASE( pRawSource );
	}
}

int	MFMDISKWrapper::ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	return 0;
}

int	MFMDISKWrapper::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	return 0;
}

int MFMDISKWrapper::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	return 0;
}

int MFMDISKWrapper::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	return 0;
}

#define MFM_INDEX_ERROR  return NUTSError( 0x0713, L"Index mark not found" )
#define MFM_SECTOR_ERROR return NUTSError( 0x0713, L"Sector mark not found" )
#define MFM_DATA_ERROR   return NUTSError( 0x0713, L"Data mark not found" )
#define MFM_TRACK_ERROR  return NUTSError( 0x0713, L"Sector not found" )

int MFMDISKWrapper::FindSectorStart( DWORD Head, DWORD Track, DWORD Sector, DWORD &SectorSize )
{
	if ( Head >= NumHeads )
	{
		return NUTSError( 0x711, L"Invalid Head ID" );
	}

	if ( Track >= NumTracks )
	{
		return NUTSError( 0x712, L"Invalid Track" );
	}

	BYTE TrackData[ MFM_DSK_TRACK_SIZE ];

	DWORD Offset = 0x100 + ( Track * MFM_DSK_TRACK_SIZE );

	Offset += Head * ( NumTracks * MFM_DSK_TRACK_SIZE );

	if ( pRawSource->ReadRaw( Offset, MFM_DSK_TRACK_SIZE, TrackData ) != DS_SUCCESS )
	{
		return -1;
	}

	DWORD TrackIndex = 0;

	// Now scan the track. First, find the index mark
	while ( ( ( TrackData[ TrackIndex ] == 0x4E ) || ( TrackData[ TrackIndex ] == 0xFF ) ) && ( TrackIndex < MFM_DSK_TRACK_SIZE ) )
	{
		TrackIndex++;
	}

	if ( TrackIndex > ( MFM_DSK_TRACK_SIZE - 7 ) ) { MFM_INDEX_ERROR; }

	// Need PLL bytes
	while ( ( TrackData[ TrackIndex ] == 0x00 ) && ( TrackIndex < MFM_DSK_TRACK_SIZE ) )
	{
		TrackIndex++;
	}

	// Need 3 SYNC bytes
	if ( TrackData[ TrackIndex ] == 0xC2 ) { TrackIndex++; } else { MFM_INDEX_ERROR; }
	if ( TrackData[ TrackIndex ] == 0xC2 ) { TrackIndex++; } else { MFM_INDEX_ERROR; }
	if ( TrackData[ TrackIndex ] == 0xC2 ) { TrackIndex++; } else { MFM_INDEX_ERROR; }

	// Index mark
	if ( TrackData[ TrackIndex ] == 0xFC ) { TrackIndex++; } else { MFM_INDEX_ERROR; }

	// Now the post-index gap
	while ( ( ( TrackData[ TrackIndex ] == 0x4E ) || ( TrackData[ TrackIndex ] == 0xFF ) ) && ( TrackIndex < MFM_DSK_TRACK_SIZE ) )
	{
		TrackIndex++;
	}

	if ( TrackIndex > MFM_DSK_TRACK_SIZE ) { MFM_INDEX_ERROR; }

	// Now look for sectors
	while ( TrackIndex < MFM_DSK_TRACK_SIZE )
	{
		if ( TrackIndex > ( MFM_DSK_TRACK_SIZE - 21 ) ) { MFM_SECTOR_ERROR; }

		// Sector gap PLL bytes
		while ( ( TrackData[ TrackIndex ] == 0x00 ) && ( TrackIndex < MFM_DSK_TRACK_SIZE ) )
		{
			TrackIndex++;
		}

		// Sector gap SYNC bytes
		for ( BYTE i=0; i<3; i++ )
		{
			if ( TrackData[ TrackIndex ] == 0xA1 ) { TrackIndex++; } else { MFM_SECTOR_ERROR; }

			if ( TrackIndex >= MFM_DSK_TRACK_SIZE ) { MFM_SECTOR_ERROR; }
		}

		// ID address mark
		if ( TrackData[ TrackIndex++ ] != 0xFE ) { MFM_SECTOR_ERROR; }

		TrackIndex++; // skip track number
		TrackIndex++; // skip head number
	
		BYTE SectorID = TrackData[ TrackIndex++ ];
		BYTE SecSize  = TrackData[ TrackIndex++ ];

		WORD Sizes[] = { 128, 256, 512, 1024 };
		WORD ThisSec = Sizes[ SecSize ];

		TrackIndex += 2; // Skip the CRC

		// Now the post-iD gap
		while ( ( ( TrackData[ TrackIndex ] == 0x4E ) || ( TrackData[ TrackIndex ] == 0xFF ) ) && ( TrackIndex < MFM_DSK_TRACK_SIZE ) )
		{
			TrackIndex++;
		}

		// Data gap PLL bytes
		while ( ( TrackData[ TrackIndex ] == 0x00 ) && ( TrackIndex < MFM_DSK_TRACK_SIZE ) )
		{
			TrackIndex++;
		}

		// Data gap SYNC bytes
		for ( BYTE i=0; i<3; i++ )
		{
			if ( TrackData[ TrackIndex ] == 0xA1 ) { TrackIndex++; } else { MFM_SECTOR_ERROR; }

			if ( TrackIndex >= MFM_DSK_TRACK_SIZE ) { MFM_SECTOR_ERROR; }
		}

		if ( TrackIndex > ( MFM_DSK_TRACK_SIZE - ( ThisSec + 0x03 ) ) ) { MFM_DATA_ERROR; }

		if ( TrackData[ TrackIndex++ ] != 0xFB ) { MFM_DATA_ERROR; }

		if ( SectorID == Sector )
		{
			int iOffset = (int) ( Offset + TrackIndex );

			SectorSize = ThisSec;

			return iOffset;
		}

		TrackIndex += ThisSec;

		TrackIndex += 2; // Skip the CRC bytes

		// Post data gap
		while ( ( ( TrackData[ TrackIndex ] == 0x4E ) || ( TrackData[ TrackIndex ] == 0xFF ) ) && ( TrackIndex < MFM_DSK_TRACK_SIZE ) )
		{
			TrackIndex++;
		}
	}

	return 0;
}

int MFMDISKWrapper::ReadSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	DWORD SectorSize = 0;

	int iOffset = FindSectorStart( Head, Track, Sector, SectorSize );

	if ( iOffset < 0 )
	{
		return -1;
	}

	DWORD Offset = (DWORD) iOffset;

	if ( SectorSize == 0 )
	{
		return NUTSError( 0x710, L"Wrong sector size" );
	}

	return pRawSource->ReadRaw( Offset, SectorSize, pSectorBuf );
}

int MFMDISKWrapper::WriteSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	DWORD SectorSize = 0;

	int iOffset = FindSectorStart( Head, Track, Sector, SectorSize );

	if ( iOffset < 0 )
	{
		return -1;
	}

	DWORD Offset = (DWORD) iOffset;

	if ( SectorSize == 0 )
	{
		return NUTSError( 0x710, L"Wrong sector size" );
	}

	return pRawSource->WriteRaw( Offset, SectorSize, pSectorBuf );
}

int MFMDISKWrapper::Truncate( QWORD Length )
{
	// There's no sensible way to translate this, so we're going to ignore it.

	return DS_SUCCESS;
}

void MFMDISKWrapper::StartFormat( )
{
	if ( pRawSource == nullptr )
	{
		return;
	}

	if ( DiskShapeSet )
	{
		BYTE Buf[ 256 ];

		rstrncpy( Buf, (BYTE *) "MFM_DISK", 8 );

		if ( ComplexDiskShape )
		{
			// Need to get the highest track number here
			std::map<BYTE,WORD> TrackCounts;

			for ( std::vector< DS_TrackDef >::iterator iTrack = ComplexMediaShape.TrackDefs.begin(); iTrack != ComplexMediaShape.TrackDefs.end(); iTrack++ )
			{
				if ( TrackCounts.find( iTrack->HeadID ) == TrackCounts.end() )
				{
					TrackCounts[ iTrack->HeadID ] = 0;
				}
				else
				{
					TrackCounts[ iTrack->HeadID ]++;
				}
			}

			for ( std::map<BYTE,WORD>::iterator iHead = TrackCounts.begin(); iHead != TrackCounts.end(); iHead++ )
			{
				if ( iHead->second > MaxTracks )
				{
					MaxTracks = iHead->second;
				}

				WLEDWORD( &Buf[ 0x08 ], (DWORD) TrackCounts.size() );
				WLEDWORD( &Buf[ 0x0C ], (DWORD) MaxTracks );
				WLEDWORD( &Buf[ 0x10 ], (DWORD) OricSequentialGeometry ); // always use sequential
			}
		}
		else
		{
			WLEDWORD( &Buf[ 0x08 ], MediaShape.Heads );
			WLEDWORD( &Buf[ 0x0C ], MediaShape.Tracks );
			WLEDWORD( &Buf[ 0x10 ], (DWORD) OricSequentialGeometry ); // always use sequential

			MaxTracks = MediaShape.Tracks;
		}

		if ( pRawSource->WriteRaw( 0, 0x100, Buf ) != DS_SUCCESS )
		{
			return;
		}

		if ( pRawSource->Truncate( 0x100 ) != DS_SUCCESS )
		{
			return;
		}
	}

	FormatTrackPosition = 0;
}

int MFMDISKWrapper::SeekTrack( WORD Track )
{
	if ( !DiskShapeSet )
	{
		return NUTSError( 0x70C, L"No disk shape set or a complex media shape in use. This cannot be used with this wrapper." );
	}

	FormatTrackPosition = 0x100 + ( Track * MFM_DSK_TRACK_SIZE );

	return DS_SUCCESS;
}

void MFMDISKWrapper::WriteGap( BYTE *buf, BYTE val, WORD repeats, DWORD &Index )
{
	for ( WORD n=0; n<repeats; n++ )
	{
		if ( Index > MFM_DSK_TRACK_SIZE )
		{
			// Rut-roh - overran!
			return;
		}

		buf[ Index ] = val;

		Index++;
	}
}

int  MFMDISKWrapper::WriteTrack( TrackDefinition track )
{
	if ( track.Density != DoubleDensity )
	{
		return NUTSError( 0x70E, L"Oric MFM wrapper only supports double density data." );
	}

	DWORD WritePos = FormatTrackPosition;

	WritePos += FormatHead * ( MaxTracks * MFM_DSK_TRACK_SIZE );

	BYTE TrackData[ MFM_DSK_TRACK_SIZE ];

	DWORD TrackIndex = 0;

	// We're going to do this controller style, and write the whole track definition as it would be
	// written by an actual FDC, but into a byte array.

	// Pre-index gap (GAP1)
	WriteGap( TrackData, track.GAP1.Value, track.GAP1.Repeats, TrackIndex );
	WriteGap( TrackData, 0x00, 0x0C, TrackIndex );
	WriteGap( TrackData, 0xC2, 0x03, TrackIndex );

	TrackData[ TrackIndex++ ] = 0xFC; // index mark

	WriteGap( TrackData, track.GAP1.Value, track.GAP1.Repeats, TrackIndex );

	// Now process the sectors
	for ( std::vector<SectorDefinition>::iterator iSector = track.Sectors.begin(); iSector != track.Sectors.end(); iSector++ )
	{
		// Sector ID pre-gaps
		WriteGap( TrackData, iSector->GAP2PLL.Value,  iSector->GAP2PLL.Repeats,  TrackIndex );
		WriteGap( TrackData, iSector->GAP2SYNC.Value, iSector->GAP2SYNC.Repeats, TrackIndex );

		TrackData[ TrackIndex++ ] = iSector->IAM; // ID address mark

		// Sector ID
		TrackData[ TrackIndex++ ] = iSector->Track;
		TrackData[ TrackIndex++ ] = iSector->Side;
		TrackData[ TrackIndex++ ] = iSector->SectorID;
		TrackData[ TrackIndex++ ] = iSector->SectorLength; // Indicator byte, e.g. 1 = 256 bytes

		// Sector ID CRC
		TrackData[ TrackIndex++ ] = 0x5A;
		TrackData[ TrackIndex++ ] = 0xA5;

		// Sector ID post-gap
		WriteGap( TrackData, iSector->GAP3.Value, iSector->GAP3.Repeats, TrackIndex );

		// Data pre-gaps
		WriteGap( TrackData, iSector->GAP3PLL.Value,  iSector->GAP3PLL.Repeats,  TrackIndex );
		WriteGap( TrackData, iSector->GAP3SYNC.Value, iSector->GAP3SYNC.Repeats, TrackIndex );

		TrackData[ TrackIndex++ ] = iSector->DAM; // Data address mark

		// Data block
		DWORD Sizes[] = { 128, 256, 512, 1024 };
		DWORD SectorSize = Sizes[ iSector->SectorLength ];

		memcpy( (void *) &TrackData[ TrackIndex ], (void *) iSector->Data, SectorSize );

		TrackIndex += SectorSize;

		// Data CRC
		TrackData[ TrackIndex++ ] = 0xA5;
		TrackData[ TrackIndex++ ] = 0x5A;

		// Post-data gap
		WriteGap( TrackData, iSector->GAP4.Value, iSector->GAP4.Repeats, TrackIndex );
	}

	// Post track-gap - fill to the end of the track.
	if ( TrackIndex >= MFM_DSK_TRACK_SIZE )
	{
		// Rut roh Shaggy!
		return NUTSError( 0x70F, L"Track overrun" );
	}

	WriteGap( TrackData, track.GAP5, MFM_DSK_TRACK_SIZE - TrackIndex, TrackIndex );

	return pRawSource->WriteRaw( WritePos, MFM_DSK_TRACK_SIZE, TrackData );
}

void MFMDISKWrapper::EndFormat( void )
{
	ValidDSK = true;

	NumHeads   = MediaShape.Heads;
	NumTracks  = MediaShape.Tracks;
}
