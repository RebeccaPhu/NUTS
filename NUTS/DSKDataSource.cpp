#include "StdAfx.h"
#include "DSKDataSource.h"
#include "../NUTS/NUTSError.h"

#include "libfuncs.h"

#include <algorithm>
 
int DSKDataSource::ReadSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	DWORD Offset = OffsetForSector( Sector );

	if ( Offset == 0 )
	{
		return NUTSError( 0xB2, L"Sector not found" );
	}

	return pSource->ReadRaw( Offset, SectorSize, pSectorBuf );
}

int DSKDataSource::WriteSector( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
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

	PhysicalDiskSize = 0;

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

WORD DSKDataSource::GetSectorID( WORD Track, WORD Head, WORD Sector )
{
	WORD TrackID = ( Track << 8 ) | Head;

	std::map<WORD, DSK_TrackInfo>::iterator iTrack;

	iTrack = DiskData.Tracks.find( TrackID );

	if ( iTrack == DiskData.Tracks.end() )
	{
		return 0xFFFF;
	}

	std::map< BYTE, DSK_SectorInfo >::iterator iSector;

	std::vector<DSK_SectorInfo> Secs;

	for ( iSector = iTrack->second.Sectors.begin(); iSector != iTrack->second.Sectors.end(); iSector++ )
	{
		Secs.push_back( iSector->second );
	}

	std::sort( Secs.begin(), Secs.end(), SecSort );

	return Secs[ Sector ].SectorID;
}

WORD DSKDataSource::GetSectorID( DWORD Sector )
{
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

			return iSec->SectorID;
		}

		iTrack++;
	}

	return 0xFFFF;
}