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

					ReadDiskMetrics();
				}
				else if ( dwGeo == 0x00000002 )
				{
					Geometry = OricInterleavedGeometry;

					ReadDiskMetrics();
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

void MFMDISKWrapper::ReadDiskMetrics( )
{
	BYTE TrackBuffer[ MFM_DSK_TRACK_SIZE ];

	BYTE Track = 0;
	BYTE Head  = 0;

	DWORD SourceOffset = 0x100;

	WORD SyncBytes = 0;

	BYTE ReadHead   = 0xFF;
	BYTE ReadSector = 0xFF;
	BYTE ReadTrack  = 0xFF;
	WORD ReadSize   = 0xFFFF;

	DiskData.clear();

	PhysicalDiskSize = 0U;

	while ( true )
	{
		if ( ( Track >= NumTracks ) || ( Head >= NumHeads ) )
		{
			break;
		}

		if ( pRawSource->ReadRaw( SourceOffset, MFM_DSK_TRACK_SIZE, TrackBuffer ) != DS_SUCCESS )
		{
			break;
		}

		OricTrack track;

		FDCState DiskState = SeekPreIndexGapMark;

		WORD Offset = 0;

		while ( true )
		{
			if ( Offset == MFM_DSK_TRACK_SIZE )
			{
				break;
			}

			BYTE FDCData = TrackBuffer[ Offset ];

			switch ( DiskState )
			{
			case SeekPreIndexGapMark:
				if ( ( FDCData == 0x4E ) || ( FDCData == 0xFF ) )
				{
					DiskState = ReadingPreIndexGap;
				}
				break;

			case ReadingPreIndexGap:
				if ( ( FDCData != 0x4E ) && ( FDCData != 0xFF ) )
				{
					if ( FDCData == 0 )
					{
						DiskState = PreIndexGapPLL;
					}
					else
					{
						DiskState = SeekPreIndexGapMark;
					}
				}
				break;

			case PreIndexGapPLL:
				if ( FDCData != 0x00 )
				{
					if ( FDCData == 0xC2 )
					{
						DiskState = PostIndexGapSync;

						SyncBytes = 0;
					}
					else
					{
						DiskState = SeekPreIndexGapMark;
					}
				}
				break;

			case PostIndexGapSync:
				if ( FDCData == 0xC2 )
				{
					if ( SyncBytes == 2 )
					{
						DiskState = IndexMark;
					}
				}
				else
				{
					DiskState = SeekPreIndexGapMark;
				}
				break;

			case IndexMark:
				if ( FDCData == 0xFC )
				{
					DiskState = SeekSectorGapMark;
				}
				else
				{
					DiskState = SeekPreIndexGapMark;
				}
				break;

			case SeekSectorGapMark:
				if ( ( FDCData == 0x4E ) || ( FDCData == 0xFF ) )
				{
					DiskState = ReadingSectorPreGap;
				}
				break;

			case ReadingSectorPreGap:
				if ( ( FDCData != 0x4E ) && ( FDCData != 0xFF ) )
				{
					if ( FDCData == 0 )
					{
						DiskState = SectorGapPLL;
					}
					else
					{
						DiskState = SeekSectorGapMark;
					}
				}
				break;

			case SectorGapPLL:
				if ( FDCData != 0x00 )
				{
					if ( FDCData == 0xA1 )
					{
						DiskState = SectorGapSync;

						SyncBytes = 0;
					}
					else
					{
						DiskState = SeekSectorGapMark;
					}
				}
				break;

			case SectorGapSync:
				if ( FDCData == 0xA1 )
				{
					if ( SyncBytes == 2 )
					{
						DiskState = SectorMark;
					}
				}
				else
				{
					DiskState = SeekSectorGapMark;
				}
				break;

			case SectorMark:
				if ( FDCData == 0xFE )
				{
					DiskState = SectorID;

					SyncBytes = 0;
				}
				else
				{
					DiskState = SeekSectorGapMark;
				}
				break;

			case SectorID:
				{
					switch ( SyncBytes )
					{
					case 1:
						ReadTrack  = FDCData;
						break;
					case 2:
						ReadHead   = FDCData;
						break;
					case 3:
						ReadSector = FDCData;
						break;
					case 4:
						{
							if ( FDCData < 4 )
							{
								WORD Sizes[] = { 128, 256, 512, 1024 };

								ReadSize = Sizes[ FDCData ];
							}

							DiskState = SectorCRC;

							SyncBytes = 0;
						}
						break;

					default:
						DiskState = SeekDataGapMark;

						break;
					}
				}
				break;

			case SectorCRC:
				if ( SyncBytes == 2) 
				{
					DiskState = SeekDataGapMark;
				}
				break;

			case SeekDataGapMark:
				if ( ( FDCData == 0x4E ) || ( FDCData == 0xFF ) )
				{
					DiskState = ReadingDataGap;
				}
				else
				{
					DiskState = SeekSectorGapMark;
				}
				break;

			case ReadingDataGap:
				if ( ( FDCData != 0x4E ) && ( FDCData != 0xFF ) )
				{
					if ( FDCData == 0 )
					{
						DiskState = DataGapPLL;
					}
					else
					{
						DiskState = SeekSectorGapMark;
					}
				}
				break;

			case DataGapPLL:
				if ( FDCData != 0x00 )
				{
					if ( FDCData == 0xA1 )
					{
						DiskState = DataGapSync;

						SyncBytes = 0;
					}
					else
					{
						DiskState = SeekSectorGapMark;
					}
				}
				break;

			case DataGapSync:
				if ( FDCData == 0xA1 )
				{
					if ( SyncBytes == 2 )
					{
						DiskState = DataMark;
					}
				}
				else
				{
					DiskState = SeekSectorGapMark;
				}
				break;

			case DataMark:
				if ( FDCData == 0xFB )
				{
					DiskState = SectorData;

					SyncBytes = 0;
				}
				else
				{
					DiskState = SeekSectorGapMark;
				}
				break;

			case SectorData:
				if ( SyncBytes == 1 )
				{
					OricSector sec;

					sec.Head       = ReadHead;
					sec.Sector     = ReadSector;
					sec.Track      = ReadTrack;
					sec.SectorSize = ReadSize;
					sec.Offset     = SourceOffset + Offset;

					track.Sectors.push_back( sec );

					PhysicalDiskSize += ReadSize;
				}
				else if ( SyncBytes == ReadSize )
				{
					DiskState = DataCRC;

					SyncBytes = 0;
				}
				break;
				
			case DataCRC:
				if ( SyncBytes == 2 )
				{
					DiskState = SeekSectorGapMark;
				}
				break;

			default:
				break;
			}

			SyncBytes++;
			Offset++;
		}

		track.Head  = Head;
		track.Track = Track;

		DiskData.push_back( track );

		SourceOffset += MFM_DSK_TRACK_SIZE;

		if ( Geometry == OricSequentialGeometry )
		{
			Track++;

			if ( Track == NumTracks ) { Track = 0; Head++; }
		}
		else
		{
			Head++;

			if ( Head == NumHeads ) { Head = 0; Track++; }
		}
	}
}

DWORD MFMDISKWrapper::FindStartOfSector( BYTE Head, BYTE Track, BYTE Sector, WORD &SectorSize )
{
	for ( OricDisk::iterator iTrack = DiskData.begin(); iTrack != DiskData.end(); iTrack++ )
	{
		if ( ( iTrack->Head == Head ) && ( iTrack->Track == Track ) )
		{
			for ( std::vector<OricSector>::iterator iSector = iTrack->Sectors.begin(); iSector != iTrack->Sectors.end(); iSector++ )
			{
				if ( iSector->Sector == Sector )
				{
					SectorSize = iSector->SectorSize;

					return iSector->Offset;
				}
			}
		}
	}

	return 0;
}

int MFMDISKWrapper::ReadSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	WORD SectorSize = 0;

	DWORD Offset = FindStartOfSector( Head, Track, Sector, SectorSize );

	if ( SectorSize == 0 )
	{
		return NUTSError( 0x710, L"Sector not found" );
	}

	return pRawSource->ReadRaw( Offset, SectorSize, pSectorBuf );
}

int MFMDISKWrapper::WriteSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	WORD SectorSize = 0;
	
	DWORD Offset = FindStartOfSector( Head, Track, Sector, SectorSize );

	if ( SectorSize == 0 )
	{
		return NUTSError( 0x710, L"Sector not found" );
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

	ReadDiskMetrics();
}
