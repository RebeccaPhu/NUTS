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

	SourceDesc = (BYTE *) "";

	Feedback = L"";
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
		return ReadRaw( ResolveSector( Head, Track, Sector ), ResolveSectorSize( Head, Track, Sector ), pSectorBuf );
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
		return WriteRaw( ResolveSector( Head, Track, Sector ), ResolveSectorSize( Head, Track, Sector ), pSectorBuf );
	}
}

DWORD DataSource::ResolveSectorSize( DWORD Head, DWORD Track, DWORD Sector )
{
	if ( !ComplexDiskShape )
	{
		return MediaShape.SectorSize;
	}

	for ( std::vector< DS_TrackDef >::iterator i = ComplexMediaShape.TrackDefs.begin(); i != ComplexMediaShape.TrackDefs.end(); i++ )
	{
		if ( ( i->TrackID == Track ) && ( i->HeadID == Head ) )
		{
			DWORD SectorCount = 0;

			for ( std::vector< WORD >::iterator is = i->SectorSizes.begin(); is != i->SectorSizes.end(); is++ )
			{
				if ( SectorCount++ == Sector )
				{
					return *is;
				}
			}
		}
	}

	return 0xFFFFFFFF;
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
		DWORD ThisHead  = ComplexMediaShape.Head1;

		while ( ThisHead <= Head )
		{
			DWORD ThisTrack = ComplexMediaShape.Track1;

			for ( std::vector< DS_TrackDef >::iterator i = ComplexMediaShape.TrackDefs.begin(); i != ComplexMediaShape.TrackDefs.end(); i++ )
			{
				if ( ( ThisTrack != Track ) || ( ThisHead != Head ) )
				{
					DWORD SectorSizeTotal = 0;

					for ( std::vector< WORD >::iterator is = i->SectorSizes.begin(); is != i->SectorSizes.end(); is++ )
					{
						SectorSizeTotal += *is;
					}

					RawOffset += SectorSizeTotal;
				}
				else
				{
					DWORD SectorSizeTotal = 0;
					WORD  SectorCount     = i->Sector1;

					for ( std::vector< WORD >::iterator is = i->SectorSizes.begin(); is != i->SectorSizes.end(); is++ )
					{
						if ( SectorCount++ != Sector )
						{
							SectorSizeTotal += *is;
						}
						else
						{
							break;
						}
					}

					RawOffset += SectorSizeTotal;

					break;
				}

				ThisTrack++;
			}

			ThisHead++;
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

	return 0;
}

static bool SectorSorter( BYTE &a, BYTE &b )
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
			DWORD hid = ComplexMediaShape.Head1;

			for ( std::vector< DS_TrackDef >::iterator i = ComplexMediaShape.TrackDefs.begin(); i != ComplexMediaShape.TrackDefs.end(); i++ )
			{
				if ( ( tid == Track ) && ( hid == Head ) )
				{
					WORD SectorID = i->Sector1;

					for ( std::vector< WORD >::iterator is = i->SectorSizes.begin(); is != i->SectorSizes.end(); i++ )
					{
						set.push_back( (BYTE) SectorID++ );
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
				set.push_back( (BYTE) sid );
			}
		}
	}

	if ( Sorted )
	{
		std::sort( set.begin(), set.end(), SectorSorter );
	}

	return set;
}