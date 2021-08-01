#include "StdAfx.h"
#include "DataSource.h"
#include "DataSourceCollector.h"
#include "NUTSError.h"

extern DataSourceCollector *pCollector;

#include <algorithm>

DataSource::DataSource(void)
{
	InitializeCriticalSection(&RefLock);

	pCollector->RegisterDataSource( this );

	RefCount = 1;

	Flags    = 0;

	DataOffset = 0;

	DiskShapeSet     = false;
	ComplexDiskShape = false;
}

DataSource::~DataSource(void)
{
	DeleteCriticalSection(&RefLock);
}

int DataSource::ReadSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	if ( !DiskShapeSet )
	{
		return NUTSError( 0x101, L"Attempt to read sector by disk shape, but disk shape not set (application bug)" );
	}
	else
	{
		return ReadRaw( ResolveSector( Head, Track, Sector ), MediaShape.SectorSize, pSectorBuf );
	}
}

int DataSource::WriteSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	if ( !DiskShapeSet )
	{
		return NUTSError( 0x101, L"Attempt to write sector by disk shape, but disk shape not set (application bug)" );
	}
	else
	{
		return WriteRaw( ResolveSector( Head, Track, Sector ), MediaShape.SectorSize, pSectorBuf );
	}
}

QWORD DataSource::ResolveSector( DWORD Head, DWORD Track, DWORD Sector )
{
	QWORD RawOffset = 0;

	if ( !ComplexDiskShape )
	{
		/* Perform a simple resolution */
		QWORD TrackSize = MediaShape.Sectors * MediaShape.SectorSize;

		if ( MediaShape.InterleavedHeads )
		{
			/* Track 0, Head 1 follows Track 0, head 0 */
			RawOffset = ( TrackSize * MediaShape.Heads ) * Track;

			RawOffset += TrackSize * Head;

			RawOffset += MediaShape.SectorSize * Sector;
		}
		else
		{
			/* Track 0, Head 1 follows Track N, head 0 */
			RawOffset = ( MediaShape.Tracks * TrackSize ) * Head;

			RawOffset += ( Track * TrackSize ) + ( Sector * MediaShape.SectorSize );
		}
	}
	else
	{
		/* Work through the offsets */
		if ( !ComplexMediaShape.Interleave )
		{
			DWORD ThisHead  = ComplexMediaShape.Head1;

			while ( ThisHead <= Head )
			{
				DWORD ThisTrack = ComplexMediaShape.Track1;

				for ( std::vector< DS_TrackDef >::iterator i = ComplexMediaShape.TrackDefs.begin(); i != ComplexMediaShape.TrackDefs.end(); i++ )
				{
					if ( ( ThisTrack != Track ) || ( ThisHead != Head ) )
					{
						RawOffset += i->Sectors * ComplexMediaShape.SectorSize;
					}
					else
					{
						RawOffset += ( Sector - i->Sector1 ) * ComplexMediaShape.SectorSize;

						break;
					}

					ThisTrack++;
				}

				ThisHead++;
			}
		}
		else
		{
			DWORD ThisTrack = ComplexMediaShape.Track1;
				
			for ( std::vector< DS_TrackDef >::iterator i = ComplexMediaShape.TrackDefs.begin(); i != ComplexMediaShape.TrackDefs.end(); i++ )
			{
				for ( DWORD ThisHead = ComplexMediaShape.Head1; ThisHead < ( ComplexMediaShape.Head1 + ComplexMediaShape.Heads ); ThisHead++ )
				{
					if ( ( ThisTrack != Track ) || ( ThisHead != Head ) )
					{
						RawOffset += i->Sectors * ComplexMediaShape.SectorSize;
					}
					else
					{
						RawOffset += ( Sector - i->Sector1 ) * ComplexMediaShape.SectorSize;

						break;
					}
				}

				if ( ThisTrack == Track )
				{
					break;
				}

				ThisTrack++;
			}
		}
	}

	return RawOffset;
}

int DataSource::SetDiskShape( DiskShape shape )
{
	MediaShape = shape;

	DiskShapeSet = true;

	ComplexDiskShape = false;

	return 0;
}

int DataSource::SetComplexDiskShape( DS_ComplexShape shape )
{
	ComplexMediaShape = shape;

	DiskShapeSet = true;

	ComplexDiskShape = true;

	MediaShape.SectorSize = (WORD) ComplexMediaShape.SectorSize;

	return 0;
}

static bool SectorSorter( WORD &a, WORD &b )
{
	return a < b;
}

SectorIDSet DataSource::GetTrackSectorIDs( WORD Head, DWORD Track, bool Sorted )
{
	SectorIDSet set;

	if ( !DiskShapeSet )
	{
		int i = NUTSError( 0x162, L"Sector IDs requested without disk shape set (Application Bug)" );
	}
	else
	{
		if ( ComplexDiskShape )
		{
			DWORD tid = ComplexMediaShape.Track1;

			for ( std::vector< DS_TrackDef >::iterator i = ComplexMediaShape.TrackDefs.begin(); i != ComplexMediaShape.TrackDefs.end(); i++ )
			{
				if ( tid == Track )
				{
					DWORD sid;

					for ( sid = i->Sector1; sid<(i->Sector1 + i->Sectors); sid++ )
					{
						set.push_back( (WORD) sid );
					}
				}

				tid++;
			}
		}
		else
		{
			WORD sid;

			for ( sid=0; sid<MediaShape.Sectors; sid++ )
			{
				set.push_back( sid );
			}
		}
	}

	if ( Sorted )
	{
		std::sort( set.begin(), set.end(), SectorSorter );
	}

	return set;
}