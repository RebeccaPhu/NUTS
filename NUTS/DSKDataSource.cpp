#include "StdAfx.h"
#include "DSKDataSource.h"
#include "../NUTS/NUTSError.h"

#include "libfuncs.h"

#include <algorithm>
 
int DSKDataSource::ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	DWORD Offset = OffsetForSector( Sector );

	if ( Offset == 0 )
	{
		return NUTSError( 0xB2, L"Sector not found" );
	}

	return pSource->ReadRaw( Offset, SectorSize, pSectorBuf );
}

int DSKDataSource::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	DWORD Offset = OffsetForSector( Sector );

	if ( Offset == 0 )
	{
		return NUTSError( 0xB2, L"Sector not found" );
	}

	return pSource->WriteRaw( Offset, SectorSize, pSectorBuf );
}

int DSKDataSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	DWORD ROffset = OffsetForOffset( Offset );

	if ( ROffset == 0 )
	{
		return NUTSError( 0xB2, L"Sector not found" );
	}

	return pSource->ReadRaw( ROffset, Length, pBuffer );
}

int DSKDataSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	DWORD ROffset = OffsetForOffset( Offset );

	if ( ROffset == 0 )
	{
		return NUTSError( 0xB2, L"Sector not found" );
	}

	return pSource->WriteRaw( ROffset, Length, pBuffer );
}

int DSKDataSource::Truncate( QWORD Length )
{
	/* TODO: Calculate this properly Becky you nob */
	int r = pSource->Truncate( Length );

	ReadDiskData();

	return r;
}


void DSKDataSource::ReadDiskData( void )
{
	ValidDisk = true;
	Extended  = false;

	BYTE Buffer[ 256 ];

	PhysicalDiskSize = pSource->PhysicalDiskSize;

	pSource->ReadRaw( 0, 256, Buffer );

	if ( !rstrncmp( Buffer, (BYTE *) "MV - CPC", 8 ) )
	{
		if ( !rstrncmp( Buffer, (BYTE *) "EXTENDED CPC DSK File\r\n", 23 ) )
		{
			ValidDisk = false;

			return;
		}
		else
		{
			Extended = true;
		}
	}

	/* Reset this, and get it properly from the DSK blocks */
	PhysicalDiskSize = 0;

	/* Read the disk information block */
	DiskData.DiskInfo.Tracks    = Buffer[ 0x30 ];
	DiskData.DiskInfo.Sides     = Buffer[ 0x31 ];
	DiskData.DiskInfo.TrackSize = * (WORD *) &Buffer[ 0x32 ];

	if ( Extended )
	{
		for ( BYTE t=0; t<DiskData.DiskInfo.Tracks; t++ )
		{
			DiskData.DiskInfo.TrackSizes[ t ] = (WORD) Buffer[ 0x34 + t ] << 8;
		}
	}

	/* Read the track information block for each track */
	DWORD Offset = 0x100;

	for ( BYTE t=0; t<DiskData.DiskInfo.Tracks; t++ )
	{
		for ( BYTE s=0; s<DiskData.DiskInfo.Sides; s++ )
		{
			DSK_TrackInfo ti;

			pSource->ReadRaw( Offset, 0x100, Buffer );

			if ( !rstrncmp( Buffer, (BYTE *) "Track-Info\r\n", 0x10 ) )
			{
				ValidDisk = false;

				return;
			}

			ti.TrackNum   = Buffer[ 0x10 ];
			ti.SideNum    = Buffer[ 0x11 ];
			ti.SecSize    = Buffer[ 0x14 ] * 0x100;
			ti.NumSectors = Buffer[ 0x15 ];
			ti.GAP3       = Buffer[ 0x16 ];
			ti.Filler     = Buffer[ 0x17 ];

			WORD DSKID = ( ti.TrackNum << 8 ) | ti.SideNum;

			for ( BYTE st=0; st<ti.NumSectors; st++ )
			{
				BYTE *sti = &Buffer[ 0x18 + ( st * 8 ) ];

				DSK_SectorInfo si;
				
				si.TrackID = sti[ 0 ];
				si.SideID = sti[ 1 ];
				si.SectorID = sti[ 2 ];
				si.SectorSize = sti[ 3 ] * 0x100;
				si.FDC1 = sti[ 4 ];
				si.FDC2 = sti[ 5 ];
				
				if ( Extended )
				{
					si.SectorSize = sti[ 6 ] | ( sti[7] << 8 );
				}

				si.ImageOffset = Offset + 0x100 + ( st * si.SectorSize );

				ti.Sectors[ si.SectorID ] = si;

				PhysicalDiskSize += si.SectorSize;
			}

			ti.ImageOffset = 0x100 + Offset;

			DiskData.Tracks[ DSKID ] = ti;

			if ( Extended )
			{
				Offset += DiskData.DiskInfo.TrackSizes[ t ];
			}
			else
			{
				Offset += DiskData.DiskInfo.TrackSize;
			}
		}
	}
}

static bool SecSort( DSK_SectorInfo &a, DSK_SectorInfo &b )
{
	if ( a.SectorID < b.SectorID )
	{
		return true;
	}

	return false;
}

DWORD DSKDataSource::OffsetForSector( DWORD Sector )
{
	if ( !ValidDisk ) { return 0; }

	std::map<WORD, DSK_TrackInfo>::iterator iTrack;

	iTrack = DiskData.Tracks.begin();

	DWORD SectorsLeft = Sector;

	while ( iTrack != DiskData.Tracks.end() )
	{
		if ( SectorsLeft >= iTrack->second.NumSectors )
		{
			SectorsLeft -= iTrack->second.NumSectors;
		}
		else
		{
			/* Get the lowest numbered sector ID */
			std::map< BYTE, DSK_SectorInfo >::iterator iSector;

			std::vector<DSK_SectorInfo> Secs;

			for ( iSector = iTrack->second.Sectors.begin(); iSector != iTrack->second.Sectors.end(); iSector++ )
			{
				Secs.push_back( iSector->second );
			}

			std::sort( Secs.begin(), Secs.end(), SecSort );

			std::vector<DSK_SectorInfo>::iterator iSec = Secs.begin();

			/* Now look for sectors */
			while ( SectorsLeft )
			{
				iSec++;

				SectorsLeft--;
			}

			return iTrack->second.Sectors[ iSec->SectorID ].ImageOffset;
		}

		iTrack++;
	}

	return 0;
}

DWORD DSKDataSource::OffsetForOffset( DWORD Offset )
{
	if ( !ValidDisk ) { return 0; }

	std::map<WORD, DSK_TrackInfo>::iterator iTrack;

	iTrack = DiskData.Tracks.begin();

	DWORD OffsetLeft = Offset;

	while ( iTrack != DiskData.Tracks.end() )
	{
		/* Get size of track */
		DWORD TrackSize = iTrack->second.Sectors.size() * iTrack->second.SecSize;
		if ( Extended )
		{
			TrackSize = 0;

			std::map< BYTE, DSK_SectorInfo >::iterator iSector;

			for ( iSector = iTrack->second.Sectors.begin(); iSector != iTrack->second.Sectors.end(); iSector++ )
			{
				TrackSize += iSector->second.SectorSize;
			}
		}

		if ( OffsetLeft >= TrackSize )
		{
			OffsetLeft -= TrackSize ;
		}
		else
		{
			/* Get the lowest numbered sector ID */
			std::map< BYTE, DSK_SectorInfo >::iterator iSector;

			std::vector<DSK_SectorInfo> Secs;

			for ( iSector = iTrack->second.Sectors.begin(); iSector != iTrack->second.Sectors.end(); iSector++ )
			{
				Secs.push_back( iSector->second );
			}

			std::sort( Secs.begin(), Secs.end(), SecSort );

			std::vector<DSK_SectorInfo>::iterator iSec = Secs.begin();

			/* Now look for sectors */
			while ( OffsetLeft >= iTrack->second.SecSize )
			{
				iSec++;

				OffsetLeft -=  iTrack->second.SecSize;
			}

			BYTE SectorID = iSec->SectorID;

			DSK_SectorInfo si = iTrack->second.Sectors[ SectorID ];

			return ( si.ImageOffset ) + OffsetLeft;
		}

		iTrack++;
	}

	return 0;
}

void DSKDataSource::StartFormat( DiskShape &shape )
{
	BYTE DSKHeader[ 256 ];

	ZeroMemory( DSKHeader, 256 );

	/* A sector size of 0 implies an extended format (sector size determined at each track) */
	if ( shape.SectorSize == 0 )
	{
		rsprintf( &DSKHeader[ 0x00 ], "EXTENDED CPC DSK File\r\nDisk-Info\r\n" );

		Extended = true;
	}
	else
	{
		rsprintf( &DSKHeader[ 0x00 ], "MV - CPCEMU Disk-File\r\nDisk-Info\r\n" );

		Extended = false;
	}

	rsprintf( &DSKHeader[ 0x22 ], "NUTS" );

	DSKHeader[ 0x30 ] = shape.Tracks;
	DSKHeader[ 0x31 ] = shape.Heads;

	if ( !Extended )
	{
		* (WORD *) &DSKHeader[ 0x32 ] = ( shape.Sectors * shape.SectorSize ) + 0x100;
	}
	else
	{
		* (WORD *) &DSKHeader[ 0x32 ] = 0;
	}

	FormatPointer    = 0x100;
	TrackDataPointer = 0x34;

	pSource->WriteRaw( 0, 256, DSKHeader );
}

int DSKDataSource::WriteTrack( TrackDefinition track )
{
	BYTE TrackHeader[ 256 ];

	ZeroMemory( TrackHeader, 256 );

	rsprintf( TrackHeader, "Track-Info\r\n" );

	if ( track.Sectors.size() > 0 )
	{
		TrackHeader[ 0x10 ] = track.Sectors[ 0 ].Track;
		TrackHeader[ 0x11 ] = track.Sectors[ 0 ].Side;
		TrackHeader[ 0x14 ] = track.Sectors[ 0 ].SectorLength;
		TrackHeader[ 0x16 ] = track.Sectors[ 0 ].GAP3PLL.Repeats;
		TrackHeader[ 0x17 ] = track.Sectors[ 0 ].Data[ 0 ];
	}
	else
	{
		TrackHeader[ 0x10 ] = 0;
		TrackHeader[ 0x11 ] = 0;
		TrackHeader[ 0x14 ] = 0x01;
		TrackHeader[ 0x16 ] = 12;
		TrackHeader[ 0x17 ] = 0xE5;
	}

	TrackHeader[ 0x15 ] = track.Sectors.size();

	DWORD TrackInfoPointer  = FormatPointer;
	BYTE *SectorInfoPointer = &TrackHeader[ 0x18 ];

	FormatPointer += 0x100;

	BYTE Sector[ 256 ];

	DWORD TrackLength = 0;

	std::vector<SectorDefinition>::iterator iSector;

	WORD LengthTable[ 4 ] = { 0x80, 0x100, 0x200, 0x400 };

	for ( iSector = track.Sectors.begin(); iSector != track.Sectors.end(); iSector++ )
	{
		WORD SectorSize = LengthTable[ min( iSector->SectorLength, 3 ) ];

		pSource->WriteRaw( FormatPointer, SectorSize, iSector->Data );

		FormatPointer += SectorSize;
		TrackLength   += SectorSize;

		SectorInfoPointer[ 0x00 ] = iSector->Track;
		SectorInfoPointer[ 0x01 ] = iSector->Side;
		SectorInfoPointer[ 0x02 ] = iSector->SectorID;
		SectorInfoPointer[ 0x03 ] = min( iSector->SectorLength, 4 );
		SectorInfoPointer[ 0x04 ] = 0;
		SectorInfoPointer[ 0x05 ] = 0;

		if ( Extended )
		{
			* (WORD *) SectorInfoPointer[ 0x06 ] = SectorSize;
		}
		else
		{
			SectorInfoPointer[ 0x06 ] = 0;
			SectorInfoPointer[ 0x07 ] = 0;
		}

		SectorInfoPointer += 8;
	}

	/* Update the track block */
	pSource->WriteRaw( TrackInfoPointer, 256, TrackHeader );

	/* Update the disk info block, if extended format */

	if ( Extended )
	{
		pSource->ReadRaw( 0, 256, Sector );

		* (WORD *) &Sector[ TrackDataPointer ] = TrackLength + 0x100;

		TrackDataPointer += 2;

		pSource->WriteRaw( 0, 256, Sector );
	}

	return 0;
}

static bool SectorSorter( WORD &a, WORD &b )
{
	return a < b;
}

SectorIDSet DSKDataSource::GetTrackSectorIDs( WORD Head, DWORD Track, bool Sorted )
{
	SectorIDSet set;

	std::map<WORD, DSK_TrackInfo>::iterator iTrack;
	std::map< BYTE, DSK_SectorInfo >::iterator iSector;

	for ( iTrack = DiskData.Tracks.begin(); iTrack != DiskData.Tracks.end(); iTrack++ )
	{
		if ( ( iTrack->second.SideNum == Head ) && ( iTrack->second.TrackNum == Track ) )
		{
			for ( iSector = iTrack->second.Sectors.begin(); iSector != iTrack->second.Sectors.end(); iSector++ )
			{
				set.push_back( iSector->second.SectorID );
			}
		}
	}

	if ( Sorted )
	{
		std::sort( set.begin(), set.end(), SectorSorter );
	}

	return set;
}

int DSKDataSource::ReadSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	std::map<WORD, DSK_TrackInfo>::iterator iTrack;
	std::map< BYTE, DSK_SectorInfo >::iterator iSector;

	for ( iTrack = DiskData.Tracks.begin(); iTrack != DiskData.Tracks.end(); iTrack++ )
	{
		if ( ( iTrack->second.SideNum == Head ) && ( iTrack->second.TrackNum == Track ) )
		{
			for ( iSector = iTrack->second.Sectors.begin(); iSector != iTrack->second.Sectors.end(); iSector++ )
			{
				DWORD offset = iSector->second.ImageOffset;

				if ( iSector->second.SectorID == Sector )
				{
					if ( !ComplexDiskShape )
					{
						return pSource->ReadRaw( offset, MediaShape.SectorSize, pSectorBuf );
					}
					else
					{
						return pSource->ReadRaw( offset, ComplexMediaShape.SectorSize, pSectorBuf );
					}
				}
			}
		}
	}

	return NUTSError( 0x163, L"Sector not found");
}

int DSKDataSource::WriteSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	std::map<WORD, DSK_TrackInfo>::iterator iTrack;
	std::map< BYTE, DSK_SectorInfo >::iterator iSector;

	for ( iTrack = DiskData.Tracks.begin(); iTrack != DiskData.Tracks.end(); iTrack++ )
	{
		if ( ( iTrack->second.SideNum == Head ) && ( iTrack->second.TrackNum == Track ) )
		{
			for ( iSector = iTrack->second.Sectors.begin(); iSector != iTrack->second.Sectors.end(); iSector++ )
			{
				DWORD offset = iSector->second.ImageOffset;

				if ( iSector->second.SectorID == Sector )
				{
					if ( !ComplexDiskShape )
					{
						return pSource->WriteRaw( offset, MediaShape.SectorSize, pSectorBuf );
					}
					else
					{
						return pSource->WriteRaw( offset, ComplexMediaShape.SectorSize, pSectorBuf );
					}
				}
			}
		}
	}

	return NUTSError( 0x163, L"Sector not found");
}
