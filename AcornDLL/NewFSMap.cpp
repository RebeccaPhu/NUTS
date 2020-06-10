#include "StdAfx.h"
#include "NewFSMap.h"
#include "../NUTS/libfuncs.h"
#include "../NUTS/NUTSError.h"

#include <algorithm>

#define PHASE_READ_ID     0
#define PHASE_TRAIL       1
#define PHASE_LEADING_BIT 2

/* Much gratitude to Gerald Holdsworth here. I have basically copied his Delphi code
   for reading this, as at the time I hadn't got my head around the bit ordering.

   Which is actually partly Gerald's fault for not explaining properly in his doc,
   hence I feel justified in stealing his code :D */
DWORD NewFSMap::ReadBits( BYTE *map, DWORD offset, BYTE length )
{
	DWORD start_byte, start_bit, bit, b, prev;
	BYTE  lastbyte;

	DWORD Result = 0;

	prev       = 0xFFFFFFFF;
	lastbyte   = 0;
	b          = 0;

	for ( bit = 0; bit < length; bit++ )
	{
		start_byte = ( offset + bit ) / 8;
		start_bit  = ( offset + bit ) % 8;
   
		if ( prev != start_byte )
		{
			prev     = start_byte;
			lastbyte = map[ start_byte ];
		}

		b = ( lastbyte & (1 << start_bit ) ) >> start_bit;
   
		Result |= ( b << bit );
	}

	return Result;
}

void NewFSMap::WriteBits( BYTE *map, DWORD offset, BYTE length, DWORD v )
{
	DWORD start_byte, start_bit, bit, b;
	BYTE  lastbyte;

	DWORD Result = 0;

	lastbyte   = 0;
	b          = 0;

	for ( bit = 0; bit < length; bit++ )
	{
		start_byte = ( offset + bit ) / 8;
		start_bit  = ( offset + bit ) % 8;

		BYTE bv = map[ start_byte ];
		
		if ( v & ( 1 << bit ) )
		{
			bv |= ( 1 << start_bit );
		}

		map[ start_byte ] = bv;
	}
}

void NewFSMap::WriteDiscRecord( BYTE *pRecord, bool Partial )
{
	ZeroMemory( pRecord, (Partial)?0x3B:0x3C );

	pRecord[ 0x00 ] = LogSecSize;
	pRecord[ 0x01 ] = SecsPerTrack;
	pRecord[ 0x02 ] = Heads;
	pRecord[ 0x03 ] = Density;
	pRecord[ 0x04 ] = IDLen;
	pRecord[ 0x05 ] = LogBPMB;
	pRecord[ 0x06 ] = Skew;
	pRecord[ 0x07 ] = BootOption;
	pRecord[ 0x08 ] = LowSector;
	pRecord[ 0x09 ] = Zones & 0xFF;

	* (WORD *)  &pRecord[ 0x0A ] = ZoneSpare;
	* (DWORD *) &pRecord[ 0x0C ] = RootLoc;
	* (DWORD *) &pRecord[ 0x10 ] = (DWORD) ( DiscSize & 0xFFFFFFFF );

	if ( Partial )
	{
		return;
	}

	* (WORD *)  &pRecord[ 0x14 ] = CycleID;

	pRecord[ 0x20 ] = DiscType;

	BBCStringCopy( (char *) &pRecord[ 0x16 ], (char *) DiscName, 10 );

	
	/* Big fields */
	pRecord[ 0x28 ] = LogShareSize;
	pRecord[ 0x29 ] = BigFlag;

	* (DWORD *) &pRecord[ 0x2C ] = FormatVersion;
	* (DWORD *) &pRecord[ 0x30 ] = RootSize;

	if ( FormatVersion == 0x01 )
	{
		pRecord[ 0x2A ] = ( BYTE ) ( Zones >> 8 );

		* (DWORD *) &pRecord[ 0x24 ] = ( DWORD ) ( DiscSize >> 32 );
	}
}

void NewFSMap::ReadDiscRecord( BYTE *pRecord )
{
	LogSecSize   = pRecord[ 0x00 ];
	SecsPerTrack = pRecord[ 0x01 ];
	Heads        = pRecord[ 0x02 ];
	Density      = pRecord[ 0x03 ];
	IDLen        = pRecord[ 0x04 ];
	LogBPMB      = pRecord[ 0x05 ];
	Skew         = pRecord[ 0x06 ];
	BootOption   = pRecord[ 0x07 ];
	LowSector    = pRecord[ 0x08 ];
	Zones        = pRecord[ 0x09 ];
	ZoneSpare    = * (WORD *)  &pRecord[ 0x0A ];
	RootLoc      = * (DWORD *) &pRecord[ 0x0C ];
	DiscSize     = * (DWORD *) &pRecord[ 0x10 ];
	CycleID      = pRecord[ 0x14 ];
	DiscType     = pRecord[ 0x20 ];

	rstrncpy( DiscName, &pRecord[ 0x16 ], 10 );

	SecSize = 1 << LogSecSize;
	BPMB    = 1 << LogBPMB;

	/* Big fields */
	LogShareSize  = pRecord[ 0x28 ];
	BigFlag       = pRecord[ 0x29 ];
	FormatVersion = * (DWORD *) &pRecord[ 0x2C ];
	RootSize      = * (DWORD *) &pRecord[ 0x30 ];

	if ( FormatVersion == 0x01 )
	{
		Zones += ( pRecord[ 0x2A ] * 256 );

		QWORD HighDiscSize = * (DWORD *) &pRecord[ 0x24 ];

		DiscSize += HighDiscSize * 0x100000000;
	}
}

int	NewFSMap::ReadFSMap()
{
	Fragments.clear();

	ZoneMapSectors.clear();
	UsedExtraSector.clear();

	/* Read disc record. This tells us a few vital bits */
	BYTE Sector[ 4096 ];

	bool HaveValidRecord = false;
	BYTE SectorStage     = 0;

	BYTE *pRecord = nullptr;
	BYTE *pMap    = nullptr;

	while ( !HaveValidRecord )
	{
		pSource->ReadSector( SectorStage, Sector, 1024 );

		if ( SectorStage == 3 )
		{
			break;
		}

		BYTE CheckByte = ZoneCheck( Sector, 1024 );

		if ( ( CheckByte != Sector[ 0 ] ) || ( Sector[ 0x04 + 0x09 ] == 0 ) || ( Sector[ 0x04 + 0x04 ] >= 22 ) )
		{
			SectorStage++;

			if ( SectorStage == 2 ) { SectorStage++; }

			continue;
		}

		HaveValidRecord = true;

		ZoneMapSectors[ 0 ] = SectorStage;
	}

	/* Disc record starts at sector offset 4. */
	pRecord = &Sector[ 4 ];

	if ( SectorStage == 3 )
	{
		pRecord = &Sector[ 0x1C0 ]; // Bit o' boot block.
	}

	ReadDiscRecord( pRecord );

	if ( Zones == 1 )
	{
		/* Map starts at 0x40, and finishes at 0x3C0 */
		pMap = &Sector[ 0x40 ];

		ReadSubMap( pMap, 0x1E00, 0, 0 );
	}
	else
	{
		/* Calculate location of root zone */
		DWORD DR_Size = 60;
		DWORD ZZ      = DR_Size * 8;

		if ( Zones <= 2 ) { ZZ = 0; }

		DWORD MapAddr = ((Zones / 2) * (8*SecSize-ZoneSpare)-ZZ)*BPMB;
		DWORD Bits    = 0;

		for ( WORD z = 0; z<Zones; z++ )
		{
			DWORD SubMapAddr = MapAddr + ( SecSize * z );

			DWORD Offset = 4;

			pSource->ReadSector( SubMapAddr / SecSize, Sector, SecSize );

			if ( ZoneCheck( Sector, SecSize ) != Sector[ 0 ] )
			{
				UsedExtraSector[ z ] = true;

				SubMapAddr += ( Zones * SecSize );

				pSource->ReadSector( SubMapAddr / SecSize, Sector, SecSize );
			}

			ZoneMapSectors[ z ] = SubMapAddr / SecSize;

			if ( z == 0 ) {
				Offset = 0x40;

				/* This is the real disc record, and the one to trust */
				ReadDiscRecord( &Sector[ 0x04 ] );
			}

			DWORD BitsInZone = ( ( SecSize - Offset )  * 8 ) - ( ZoneSpare - 32 );

			ReadSubMap( &Sector[ Offset ], BitsInZone, z, Bits );

			/*
			if ( z == 0 )
			{
				Bits += 0x1E20;
			}
			else
			{
				Bits += 0x2000;
			}
			*/
			Bits += BitsInZone;
		}
	}

	BYTE c = 9;

	while ( c < 10 )
	{
		if ( DiscName[ c ] != 0x20 )
		{
			break;
		}
		else
		{
			DiscName[ c ] = 0;
		}

		c--;
	}

	IDsPerZone = ((SecSize * 8) - ZoneSpare) / (IDLen + 1 );

	/* Fix up the disc name */
	for ( BYTE i=0; i<10; i++ )
	{
		if ( DiscName[ i ] == 0x0D ) 
		{
			DiscName[ i ] = 0;
		}
	}

	return 0;
}

int NewFSMap::ReadSubMap( BYTE *pMap, DWORD NumBits, WORD zone, DWORD BitsOffset )
{
	DWORD CurrentBit = 0;

	while ( 1 )
	{
		DWORD FragId = ReadBits( pMap, CurrentBit, IDLen );
		DWORD Offset = CurrentBit;

		CurrentBit += IDLen;

		DWORD bit = ReadBits( pMap, CurrentBit, 1 );

		while ( bit == 0 )
		{
			CurrentBit++;

			if ( CurrentBit >= NumBits )
			{
				return 0;
			}

			bit = ReadBits( pMap, CurrentBit, 1 );
		}

		CurrentBit++;

		Fragment f;

		f.FragID     = FragId;
		f.Zone       = zone;
		f.FragOffset = Offset + BitsOffset;
		f.Length     = ( CurrentBit - Offset ) * BPMB;

		Fragments.push_back( f );
	}

	return 0;
}

int NewFSMap::WriteSubMap( BYTE *pMap, DWORD NumBits, WORD zone )
{
	Fragment_iter iFrag;

	DWORD BitCount = 0;

	ZeroMemory( pMap, NumBits / 8 );

	for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); iFrag++ )
	{
		if ( iFrag->Zone != zone )
		{
			continue;
		}

		if ( BitCount >= NumBits )
		{
			return NUTSError( 0x39, L"Map full - on new map discs this indicates corruption" );
		}

		DWORD FragBits = ( iFrag->Length / BPMB ) - 1;

		WriteBits( pMap, BitCount, IDLen, iFrag->FragID );

		FragBits -= IDLen;
		BitCount += IDLen;

		while ( FragBits != 0 )
		{
			WriteBits( pMap, BitCount, 1, 0 );

			BitCount++;

			FragBits--;
		}

		WriteBits( pMap, BitCount, 1, 1 );

		BitCount++;
	}

	return 0;
}

int	NewFSMap::WriteFSMap()
{
	if ( Zones == 1 )
	{
		BYTE MapSector[ 1024 ];

		WriteDiscRecord( &MapSector[ 0x04 ], false );

		/* Do submap */
		WriteSubMap( &MapSector[ 0x40 ], 0x3C0 * 8, 0 );

		MapSector[ 0 ] = ZoneCheck( MapSector, 1024 );

		pSource->WriteSector( 0, MapSector, 1024 );
		pSource->WriteSector( 1, MapSector, 1024 );
	}
	else 
	{
		for ( WORD n=0; n<Zones; n++ )
		{
			BYTE MapSector[ 4096 ];

			DWORD Offset = 0x4;

			if ( n == 0 )
			{
				Offset = 0x40;

				/* Disc record in zone 0 */
				WriteDiscRecord( &MapSector[ 0x04 ], false );
			}

			/* Do submap */
			DWORD BitsInZone = ( ( SecSize - Offset )  * 8 ) - ( ZoneSpare - 32 );

			WriteSubMap( &MapSector[ Offset ], BitsInZone, n );

			MapSector[ 0 ] = ZoneCheck( MapSector, SecSize );

			pSource->WriteSector( ZoneMapSectors[ n ], MapSector, SecSize );

			DWORD SecondSector = ZoneMapSectors[ n ];

			if ( UsedExtraSector[ n ] )
			{
				SecondSector -= Zones;
			}
			else
			{
				SecondSector += Zones;
			}

			pSource->WriteSector( SecondSector, MapSector, SecSize );
		}
	}

	return 0;
}

DWORD NewFSMap::GetUnusedFragmentID( DWORD Zone )
{
	Fragment_iter iFrag;

	DWORD Frag      = 0;
	DWORD MaybeFrag = 3; /* IDs 0 to 2 are reserved */

	if ( Zone > 0 )
	{
		MaybeFrag = IDsPerZone * Zone;
	}

	DWORD HighFrag  = IDsPerZone * ( Zone + 1 );

	while ( Frag == 0 )
	{
		bool FreeFrag = true;

		for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); )
		{
			if ( iFrag->Zone != Zone )
			{
				iFrag++;

				continue;
			}

			if ( iFrag->FragID == MaybeFrag )
			{
				FreeFrag = false;

				MaybeFrag++;

				iFrag = Fragments.end();
			}
			else
			{
				iFrag++;
			}
		}

		if ( FreeFrag )
		{
			Frag = MaybeFrag;
		}

		if ( MaybeFrag >= HighFrag )
		{
			break;
		}
	}

	return Frag;
}

bool FragmentSort( Fragment &a, Fragment &b )
{
	if ( a.Length == b.Length )
	{
		return false;
	}

	if ( a.Length > b.Length )
	{
		return true;
	}

	return false;
}

bool ZoneSort( Fragment &a, Fragment &b )
{
	if ( a.Zone == b.Zone )
	{
		return false;
	}

	if ( a.Zone < b.Zone )
	{
		return true;
	}

	return false;
}

DWORD NewFSMap::SectorForFragmentOffset( DWORD FragOffset )
{
	if ( Zones == 0 )
	{
		return 0xFFFFFFFF;
	}

	Fragment_iter iFrag;

	DWORD StartZone = 0;

	DWORD SearchZone = StartZone;
	bool  AllZones   = false;

	while ( !AllZones )
	{
		for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); iFrag++ )
		{
			if ( ( iFrag->FragOffset == FragOffset ) && ( iFrag->Zone == SearchZone ) )
			{
				DWORD FullAddress = ( iFrag->FragOffset * BPMB );
//				DWORD FullAddress = ((iFrag->FragOffset - (ZoneSpare * iFrag->Zone)) * BPMB);

				return FullAddress / SecSize;
			}
		}

		SearchZone = ( SearchZone + 1 ) % Zones;

		if ( SearchZone == StartZone )
		{
			AllZones = true;
		}
	}

	return 0xFFFFFFFF;
}

void NewFSMap::ClaimFragment( Fragment_iter iFrag, DWORD SecLength, DWORD ProposedFragID )
{
	/* Take the free fragment identified by the iterator, and change it to have the proposed fragment ID.
	   If the SecLength is less than the sector length of the whole fragment, then a new blank fragment
	   must be inserted after. Note that fragments are measured in BPMB. */

	if ( iFrag->Length == ( SecLength * SecSize ) )
	{
		/* Simple swap. Change the fragment ID and leave */
		iFrag->FragID = ProposedFragID;

		return;
	}

	/* Fragment is bigger than required. Note that we are never called with a fragment smaller than SecLength */

	DWORD FragBits = IDLen; /* Fragment size must be at least this big */

	while ( ( FragBits * BPMB ) < ( SecLength * SecSize ) )
	{
		FragBits++; /* This is the length in bits of the fragment */
	}

	/* We can't leave a space that is less than at least IDLen bits in map length, otherwise it becomes unaddressable */
	DWORD NewLength  = FragBits * BPMB;
	DWORD FreeLength = iFrag->Length - NewLength;
	DWORD MinLength  = ( IDLen + 1 ) * BPMB;

	if ( FreeLength < MinLength )
	{
		/* This means the entire free space is now occupied by the sectors required. But a later write may (if this is
		   not a multi-fragment file) claim the remaining space for smaller files. */
		NewLength  = iFrag->Length;
		FreeLength = 0;
	}

	/* Alter this fragment */
	iFrag->FragID = ProposedFragID;
	iFrag->Length = NewLength;

	if ( FreeLength != 0 )
	{
		/* Make a fragment to represent the trailing free space */
		Fragment FreeFrag;

		FreeFrag.FragID     = 0;
		FreeFrag.Length     = FreeLength;
		FreeFrag.FragOffset = iFrag->FragOffset + FragBits;
		FreeFrag.Zone       = iFrag->Zone;

		iFrag++;

		Fragments.insert( iFrag, FreeFrag );
	}
}

void NewFSMap::ClaimFragmentByOffset( DWORD FragOffset, DWORD SecLength, DWORD ProposedFragID )
{
	Fragment_iter iFrag;

	for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); iFrag++ )
	{
		if ( iFrag->FragOffset == FragOffset )
		{
			ClaimFragment( iFrag, SecLength, ProposedFragID );

			return;
		}
	}
}

TargetedFileFragments NewFSMap::GetWriteFileFragments( DWORD SecLength, DWORD ExistingFragID, bool UseExistingFragID )
{
	/* The ExistingFragID stuff is used to extend Big directories, by allocating additional fragments under the existing
	   Fragment ID. This works, because all the newly allocated fragments will have the same ID, and will still be
	   returned in offset order, then cycled to start at the starting zone according to the fragment ID. */
	TargetedFileFragments Frags;

	Frags.SectorOffset = 0;

	/* First try and find a space begin enough for the file */
	DWORD AbsLength = SecLength * SecSize;

	/* The fragment size must be at least big enough to be represented by IDLen + 1 bits */
	while ( AbsLength < ( ( IDLen + 1 ) * BPMB ) )
	{
		AbsLength += SecSize;
	}

	Fragment_iter iFrag;

	for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); iFrag++ )
	{
		if ( ( iFrag->Length >= AbsLength ) && ( iFrag->FragID == 0 ) )
		{
			DWORD ProposedFragID = 0;
			
			if ( !UseExistingFragID )
			{
				ProposedFragID = GetUnusedFragmentID( iFrag->Zone );
			}
			else
			{
				ProposedFragID = ExistingFragID;
			}

			if ( ProposedFragID == 0 )
			{
				return Frags;
			}

			/* Woot - We now need to split this up */
			ClaimFragment( iFrag, AbsLength / SecSize, ProposedFragID );

			FileFragment f;

			f.Length = AbsLength;
			f.Sector = SectorForSingleFragment( ProposedFragID );

			Frags.Frags.push_back( f );
			Frags.FragID = ProposedFragID;

			/* Write the changes to the disk/image */
			WriteFSMap();

			return Frags;
		}
	}

	/* If we're here, then there isn't a fragment big enough. We'll need to gather some fragments */
	FragmentList SortedFrags = Fragments;

	/* It's important to not do this on the original vector, otherwise the map becomes meaningless */
	std::sort( SortedFrags.begin(), SortedFrags.end(), FragmentSort );

	/* Prune out the used fragments. They're of no use to us */
	for ( iFrag = SortedFrags.begin(); iFrag != SortedFrags.end(); )
	{
		if ( iFrag->FragID != 0 )
		{
			iFrag = SortedFrags.erase( iFrag );
		}
		else
		{
			iFrag++;
		}
	}

	      iFrag  = SortedFrags.begin();
	DWORD Secs   = AbsLength / SecSize;

	if ( SortedFrags.size() == 0 )
	{
		return Frags;
	}

	/* The frag ID will be based on the zone of the first fragment (the biggest), but may cycle around */
	DWORD ProposedFragID = 0;

	if ( !UseExistingFragID )
	{
		ProposedFragID = GetUnusedFragmentID( iFrag->Zone );
	}
	else
	{
		ProposedFragID = ExistingFragID;
	}

	if ( ProposedFragID == 0 )
	{
		return Frags;
	}

	while ( Secs > 0 )
	{
		if ( iFrag == SortedFrags.end() )
		{
			/* Out of disk space! Oh noes! */
			TargetedFileFragments NullFrags;

			/* Re-read FSMap to undo the changes */
			ReadFSMap();

			return NullFrags;
		}

		/* Claim from this fragment however many sectors remain */
		DWORD ThisSecs = iFrag->Length / SecSize;

		if ( ThisSecs > Secs )
		{
			ThisSecs = Secs;
		}

		/* The fragment size must be at least big enough to be represented by IDLen + 1 bits */
		while ( ( ThisSecs * SecSize ) < ( ( IDLen + 1 ) * BPMB ) )
		{
			ThisSecs++;
		}

		if ( iFrag->FragID == 0 )
		{
			ClaimFragmentByOffset( iFrag->FragOffset, ThisSecs, ProposedFragID );

			FileFragment f;

			f.Sector = SectorForFragmentOffset( iFrag->FragOffset );
			f.Sector = ThisSecs * SecSize;
			f.Zone   = iFrag->Zone;

			Frags.Frags.push_back( f );
			Frags.FragID = ProposedFragID;

			Secs -= ThisSecs;
		}

		iFrag++;
	}

	/* Write the changes to the disk/image */
	WriteFSMap();

	/* We need to re-order the fragments at this point, as they will be in a higgledy-piggledy order.
	   The fragments need to be sorted by zone, then re-ordered to start at the beginning zone for the fragment ID. */

	ReorderWriteFragments( &Frags );

	return Frags;
}

bool OffsetSort( FileFragment &a, FileFragment &b )
{
	if ( a.Sector == b.Sector )
	{
		return false;
	}
	else
	{
		if ( a.Sector < b.Sector )
		{
			return true;
		}
	}

	return false;
}

void NewFSMap::ReorderWriteFragments( TargetedFileFragments *pFrags )
{
	FileFragments ZonedFrags = pFrags->Frags;

	DWORD StartingZone = pFrags->FragID / IDsPerZone;

	/* Now we must sort fragment sets within each zone to be in order of offset */
	std::sort( ZonedFrags.begin(), ZonedFrags.end(), OffsetSort );

	/* Rotate the vector until the first fragment's Zone matches the starting zone */
	while ( ZonedFrags.begin()->Zone != StartingZone )
	{
		FileFragment tFrag = ZonedFrags.front();

		ZonedFrags.erase( ZonedFrags.begin() );

		ZonedFrags.push_back( tFrag );
	}

	pFrags->Frags = ZonedFrags;
}

DWORD NewFSMap::GetMatchingFragmentCount( DWORD FragmentID )
{
	Fragment_iter iFrag;

	DWORD Count = 0;

	for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); iFrag++ )
	{
		if ( iFrag->FragID == FragmentID )
		{
			Count++;
		}
	}

	return Count;
}

DWORD NewFSMap::GetSingleFragmentSize( DWORD FragmentID )
{
	Fragment_iter iFrag;

	DWORD FragSize = 0;

	for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); iFrag++ )
	{
		if ( iFrag->FragID == FragmentID )
		{
			FragSize = iFrag->Length;
		}
	}

	if ( FragSize % SecSize ) { FragSize = ( FragSize / SecSize ) + 1; } else { FragSize /= SecSize; }

	return FragSize;
}

DWORD NewFSMap::SectorForSingleFragment( DWORD FragmentID )
{
	if ( Zones == 0 )
	{
		return 0xFFFFFFFF;
	}

	Fragment_iter iFrag;

	DWORD StartZone = 0;

	if ( Zones > 1 )
	{
		DWORD IDsPerZone = ( (1 << ( LogSecSize + 3) ) - ZoneSpare) / (IDLen + 1);
		
		StartZone = FragmentID / IDsPerZone;
	}

	if ( FragmentID == 2 )
	{
		/* Root. *sigh* */
		StartZone = Zones / 2;
	}

	DWORD SearchZone = StartZone;
	bool  AllZones   = false;

	while ( !AllZones )
	{
		for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); iFrag++ )
		{
			if ( ( iFrag->FragID == FragmentID ) && ( iFrag->Zone == SearchZone ) )
			{
				DWORD FullAddress = ( iFrag->FragOffset * BPMB);
//				DWORD FullAddress = ((iFrag->FragOffset - (ZoneSpare * iFrag->Zone)) * BPMB);

				return FullAddress / SecSize;
			}
		}

		SearchZone = ( SearchZone + 1 ) % Zones;

		if ( SearchZone == StartZone )
		{
			AllZones = true;
		}
	}

	return 0xFFFFFFFF;
}

FileFragments NewFSMap::GetFileFragments( DWORD DiscAddr )
{
	FileFragments frags;

	if ( Zones == 0 )
	{
		return frags;
	}

	Fragment_iter iFrag;

	DWORD FragmentID = DiscAddr >> 8;
	BYTE  SectorID   = DiscAddr & 0xFF;

	if ( SectorID > 0 ) { SectorID--; }

	DWORD StartZone = 0;

	if ( Zones > 1 )
	{
		DWORD IDsPerZone = ((1 << (LogSecSize + 3)) - ZoneSpare) / (IDLen + 1);
		
		StartZone = FragmentID / IDsPerZone;
	}

	if ( FragmentID == 2 )
	{
		/* Root. *sigh* */
		StartZone = Zones / 2;
	}

	for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); iFrag++ )
	{
		if ( iFrag->FragID == FragmentID )
		{
//			DWORD FullAddress = ((iFrag->FragOffset - (ZoneSpare * iFrag->Zone)) * BPMB);
			DWORD FullAddress = ( iFrag->FragOffset * BPMB );

			FileFragment f;

			f.Sector = FullAddress / SecSize;
			f.Length = iFrag->Length;
			f.Zone   = iFrag->Zone;

			frags.push_back( f );
		}
	}

	/* Rotate the vector until the first fragment's Zone matches the starting zone */
	while ( frags.begin()->Zone != StartZone )
	{
		FileFragment tFrag = frags.front();

		frags.erase( frags.begin() );

		frags.push_back( tFrag );
	}

	/* Add the sector offset to the first fragment */
	frags.begin()->Sector += SectorID;

	/* ... and subtract the length */
	frags.begin()->Length -= ( SectorID * SecSize );

	return frags;
}

BYTE NewFSMap::ZoneCheck( BYTE *map_base, DWORD SSize )
{
	DWORD sum_vector0 = 0;
	DWORD sum_vector1 = 0;
	DWORD sum_vector2 = 0;
	DWORD sum_vector3 = 0;
	DWORD rover;

	for (rover = SSize - 4; rover>0; rover-=4)
	{
		sum_vector0 += map_base[ rover + 0 ] + (sum_vector3>>8);
		sum_vector3 &= 0xFF;
		sum_vector1 += map_base[ rover + 1 ] + (sum_vector0>>8);
		sum_vector0 &= 0xFF;
		sum_vector2 += map_base[ rover + 2 ] + (sum_vector1>>8);
		sum_vector1 &= 0xFF;
		sum_vector3 += map_base[ rover + 3 ] + (sum_vector2>>8);
		sum_vector2 &= 0xFF;
	}

	sum_vector0 += (sum_vector3>>8);
	sum_vector1 += map_base[rover+1] + (sum_vector0>>8);
	sum_vector2 += map_base[rover+2] + (sum_vector1>>8);
	sum_vector3 += map_base[rover+3] + (sum_vector2>>8);

	return (BYTE) ((sum_vector0^sum_vector1^sum_vector2^sum_vector3) & 0xFF);
}

void NewFSMap::ReleaseFragment( DWORD FragmentID )
{
	/* This is a two step process. Firstly, change the IDs of every fragment that matches
	   to 0, marking it as free. Then, collapse adjacent spaces (as long as they are in
	   the same zone) to make one big space */

	Fragment_iter iFrag;

	for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); iFrag++ )
	{
		if ( iFrag->FragID == FragmentID )
		{
			iFrag->FragID = 0;
		}
	}

	/* Step two: collapse adjacent fragments */
	Fragment_iter pFrag = Fragments.end();

	for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); )
	{
		bool DidErase = false;

		if ( pFrag != Fragments.end() )
		{
			if ( ( pFrag->Zone == iFrag->Zone ) && ( pFrag->FragID == 0 ) && ( iFrag->FragID == 0 ) )
			{
				pFrag->Length += iFrag->Length;

				iFrag = Fragments.erase( iFrag );

				DidErase = true;
			}
		}

		pFrag = iFrag;

		if ( !DidErase )
		{
			iFrag++;
		}
		else
		{
			pFrag--;
		}
	}

	WriteFSMap();
}

BYTE NewFSMap::BootBlockCheck( BYTE *block )
{
	WORD c = 0x0000;

	for ( WORD n=0; n<0x1FF; n++ )
	{
		c += block[ n ];

		if ( c > 0x100 )
		{
			c++;

			c &= 0xFF;
		}
	}

	return (BYTE) ( c & 0xFF );
}

inline void NewFSMap::IDExtend( DWORD *pID )
{
	DWORD MinSize = ( IDLen + 1 ) * BPMB;

	if ( *pID < MinSize )
	{
		*pID = MinSize;
	}
}

void NewFSMap::ConfigureDisk( DWORD FSID )
{
	QWORD PhysicalDiskSize = pSource->PhysicalDiskSize;

	/* First calculate how many bits we need to represent the bitmap */
	QWORD BitsRequired = PhysicalDiskSize / BPMB;

	/* Now we need to settle on a ZoneSpare value. This is the number of bits from the end of the
	   last zone's bitmap to the start of this zone's map. This needs to take into account that
	   zone 0 has the disc record, and thus is 0x3C bytes shorter than the other zones.

	   This is based on SecSize as well.
	
	   To do this, start with an assumed ZoneSpare of 0 (best case scenario). The remaining space in
	   both circumstances should be either SecSize - 4 or SecSize - 64 bytes, times 8 bits. This space
	   should be exactly divisible by IDLen + 1 (the smallest fragment size). If it is, congratulations,
	   ZoneSpare has been found. If not, add 1 to ZoneSpare and repeat. If ZoneSpare reaches (SecSize - 64)
	   bytes, then the calculation has failed and the disk cannot be configured.
	*/

	WORD  MaxID   = IDLen + 1;

	ZoneSpare = 0;
	bool Done = false;

	while ( ZoneSpare < ( (SecSize - 64 ) * 8 ) )
	{
		DWORD Bitmap1 = ( ( SecSize - 64 ) * 8 ) - ZoneSpare;
		DWORD Bitmap2 = ( ( SecSize - 4  ) * 8 ) - ZoneSpare;

		if ( ( Bitmap1 % MaxID ) || ( Bitmap2 % MaxID ) )
		{
			ZoneSpare++;

			continue;
		}
		else
		{
			Done = true;

			break;
		}
	}

	if ( !Done )
	{
		return;
	}

	/* We'll need Sector Size presetting, as it features in many calculations */
	switch ( FSID )
	{
	case FSID_ADFS_E:
	case FSID_ADFS_F:
	case FSID_ADFS_EP:
	case FSID_ADFS_FP:
		LogSecSize = 0xA;
		SecSize    = 0x400;
		break;

	case FSID_ADFS_HN:
	case FSID_ADFS_HP:
		LogSecSize = 0x9;
		SecSize    = 0x200;
		break;
	}

	/* Add on 32 bits for the header in the next block */
	ZoneSpare += 32;

	/* Now we know how many bits are in each zone, we can calculate the number of zones we need. */
	Zones = 0;

	IDsPerZone = ( ( 1 << (LogSecSize + 3 ) ) - ZoneSpare ) / MaxID;

	while ( ( Zones * ( IDsPerZone * MaxID ) ) < BitsRequired )
	{
		Zones++;
	}

	/* Fill in the other details */
	LowSector = 0;
	DiscSize  = PhysicalDiskSize;
	CycleID   = 0;

	SecsPerTrack = 5;
	Density      = 2;
	BootOption   = 0;
	Skew         = 1;

	switch ( FSID )
	{
	case FSID_ADFS_H:
		Density += 4;
		SecsPerTrack += 10;
	case FSID_ADFS_F:
	case FSID_ADFS_FP:
		SecsPerTrack += 5;
		Density      += 2;
	case FSID_ADFS_E:
	case FSID_ADFS_EP:
		Heads    = 2;
		DiscType = 0;
		break;

	case FSID_ADFS_HN:
		Heads        = 16;
		DiscType     = 0x00040000;
		SecsPerTrack = 0x3F;
		Density      = 0;
		break;

	case FSID_ADFS_HP:
		Heads        = 16;
		DiscType     = 0x00040000;
		SecsPerTrack = 0x3F;
		Density      = 0;
		break;
	}

	/* Now we must determine the location of the root cluster. For ADFS E/E+, this is sector 0. For everything else,
	   this must becalculated. 
	*/

	ZoneMapSectors.clear();
	UsedExtraSector.clear();

	if ( FSID == FSID_ADFS_E )
	{
		ZoneMapSectors[ 0 ]  = 0;
		UsedExtraSector[ 0 ] = false;
	}
	else
	{
		/* Calculate location of root zone */
		DWORD DR_Size = 60;
		DWORD ZZ      = DR_Size * 8;

		DWORD MapAddr = ((Zones / 2) * (8*SecSize-ZoneSpare)-ZZ)*BPMB;

		for ( WORD z=0; z<Zones; z++ )
		{
			ZoneMapSectors[ z ]  = ( MapAddr / SecSize ) + z;
			UsedExtraSector[ z ] = 0;
		}
	}

	if ( ( FSID == FSID_ADFS_EP ) || ( FSID == FSID_ADFS_FP ) || ( FSID == FSID_ADFS_G ) || ( FSID == FSID_ADFS_HP ) )
	{
		RootSize = 0x1000; // This will actually be overwritten later.
	}
	else
	{
		RootSize = 0x800;
	}

	/* Now fill in the one free fragment for each zone. The middle zone is special, because it contains the root
	   directory, which will be ( Zones * 2 ) + 0x8 + 0x08 sectors. ( 8 sectors for the directory itself, plus 8
	   spare for small file recording.
	   
	   We'll also need to grab a small bit at the beginning of Zone 0, to cover the boot block, otherwise the
	   first file write that's big enough will wipe it out.
	*/
	Fragments.clear();

	DWORD BitsOffset = 0;
	DWORD BitsLeft   = BitsRequired;

	for ( WORD z=0; z<Zones; z++ )
	{
		DWORD Offset = 4;

		if ( z == 0 ) { Offset += 60; }

		Fragment f;

		f.FragID     = 0;
		f.FragOffset = BitsOffset;
		f.Zone       = z;

		BitsOffset += ( ( SecSize - Offset ) * 8 ) - ( ZoneSpare - 32 );

		DWORD ThisLen = ( ( SecSize - Offset ) * 8 ) - ( ZoneSpare - 32 );

		if ( ThisLen > BitsLeft ) { ThisLen = BitsLeft; }

		f.Length     = ThisLen * BPMB;

		if ( z == ( Zones / 2 ) )
		{
			Fragment r;

			if ( ( FSID == FSID_ADFS_EP ) || ( FSID == FSID_ADFS_FP ) || ( FSID == FSID_ADFS_G ) || ( FSID == FSID_ADFS_HP ) )
			{
				r.FragID     = 2;
				r.FragOffset = f.FragOffset;
				r.Length     = ( ( Zones * 2 ) * SecSize);
				r.Zone       = z;

				IDExtend( &r.Length );

				Fragments.push_back( r );
			}
			else
			{
				r.FragID     = 2;
				r.FragOffset = f.FragOffset;
				r.Length     = ( ( Zones * 2 ) * SecSize)  + 0x1000;
				r.Zone       = z;

				IDExtend( &r.Length );

				Fragments.push_back( r );

				RootSize = r.Length;
			}

			f.FragOffset += r.Length / BPMB;
			f.Length     -= r.Length;
		}

		if ( ( z == 0 ) && ( Zones > 2 ) )
		{
			/* This only applies on multi-zone discs. Single-zone discs have their zone 0's covered by the block above. */
			Fragment r;

			r.FragID     = 2; // This is ignored by the root dir, which always starts from the middle zone.
			r.FragOffset = 0;
			r.Length     = max( MaxID * BPMB, 0x1000 ); // Smallest size possbile
			r.Zone       = 0;

			IDExtend( &r.Length );

			Fragments.push_back( r );

			f.FragOffset += r.Length / BPMB;
			f.Length     -= r.Length;

		}

		if ( z == ( Zones / 2 ) )
		{
			if ( ( FSID == FSID_ADFS_EP ) || ( FSID == FSID_ADFS_FP ) || ( FSID == FSID_ADFS_G ) || ( FSID == FSID_ADFS_HP ) )
			{
				Fragment r;

				r.FragID     = 3;
				r.FragOffset = f.FragOffset;
				r.Length     = 0x1000;
				r.Zone       = z;

				IDExtend( &r.Length );

				Fragments.push_back( r );

				RootSize = r.Length;

				f.FragOffset += r.Length / BPMB;
				f.Length     -= r.Length;
			}
		}

		IDExtend( &f.Length );

		Fragments.push_back( f );

		BitsLeft -= ThisLen;
	}

	/* Set the root directory location, which is one sector higher because of sub-fragment usage */
	if ( ( FSID == FSID_ADFS_EP ) || ( FSID == FSID_ADFS_FP ) || ( FSID == FSID_ADFS_G ) || ( FSID == FSID_ADFS_HP ) )
	{
		RootLoc = 0x000301;

		FormatVersion = 0x00000001;
	}
	else
	{
		RootLoc = 0x000200 + ( Zones * 2 ) + 1;

		FormatVersion = 0x00000000;
	}

	if ( ( FSID == FSID_ADFS_F ) || ( FSID = FSID_ADFS_HN ) )
	{
		/* Finally, Create a boot block */
		BYTE BootBlock[ 0x200 ];

		ZeroMemory( BootBlock, 0x200 );

		WriteDiscRecord( &BootBlock[ 0x1C0 ], true );

		BootBlock[ 0x1FF ] = BootBlockCheck( BootBlock );

		pSource->WriteSector( 6, BootBlock, 0x200 );
	}

	/* Done! */
}