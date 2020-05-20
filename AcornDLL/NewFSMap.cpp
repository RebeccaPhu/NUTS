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

int	NewFSMap::ReadFSMap()
{
	Fragments.clear();

	/* Read disc record. This tells us a few vital bits */
	BYTE Sector[ 1024 ];

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

		BYTE CheckByte = ZoneCheck( Sector );

		if ( CheckByte != Sector[ 0 ] )
		{
			SectorStage++;

			if ( SectorStage == 2 ) { SectorStage++; }

			continue;
		}

		HaveValidRecord = true;

		SingleZoneMapSector = SectorStage;
	}

	/* Disc record starts at sector offset 4. */
	pRecord = &Sector[ 4 ];

	if ( SectorStage == 3 )
	{
		pRecord = &Sector[ 0x1C0 ]; // Bit o' boot block.
	}

	IDLen = pRecord[ 0x04 ];
	BPMB  = 1 << pRecord[ 0x05 ];
	Skew  = pRecord[ 0x05 ]; // Used when accessing physical floppies only.
	Zones = pRecord[ 0x09 ];

	ZoneSpare = * (WORD *) &pRecord[ 0x0A ];
	RootLoc   = * (WORD *) &pRecord[ 0x0C ];
	SecSize   = 1 << pRecord[ 0x00 ];

	DiscName[ 10 ] = 0;

	if ( Zones == 1 )
	{
		/* Map starts at 0x40, and finishes at 0x3C0 */
		pMap = &Sector[ 0x40 ];

		ReadSubMap( pMap, 0x1E00, 0, 0 );

		/* Read the disc name */
		rstrncpy( DiscName, &pRecord[ 0x16 ], 10 );

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

			if ( z == 0 ) {
				Offset = 0x40;

				RootLoc = * (WORD *) &Sector[ 0x10 ];

				/* Read the disc name */
				rstrncpy( DiscName, &Sector[ 0x1A ], 10 );

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
			}

			ReadSubMap( &Sector[ Offset ], (SecSize - Offset) * 8, z, Bits );

			if ( z == 0 )
			{
				Bits += 0x1E20;
			}
			else
			{
				Bits += 0x2000;
			}
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

		pSource->ReadSector( SingleZoneMapSector, MapSector, 1024 );

		/* Do submap */
		WriteSubMap( &MapSector[ 0x40 ], 0x3C0 * 8, 0 );

		MapSector[ 0 ] = ZoneCheck( MapSector );

		pSource->WriteSector( 0, MapSector, 1024 );
		pSource->WriteSector( 1, MapSector, 1024 );
	}

	return 0;
}

DWORD NewFSMap::GetUnusedFragmentID( void )
{
	Fragment_iter iFrag;

	DWORD Frag      = 0;
	DWORD MaybeFrag = 3; /* IDs 0 to 2 are reserved */

	while ( Frag == 0 )
	{
		bool FreeFrag = true;

		for ( iFrag = Fragments.begin(); iFrag != Fragments.end(); )
		{
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

		if ( MaybeFrag > ( ( 1 << IDLen ) - 1 ) )
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
				DWORD FullAddress = ((iFrag->FragOffset - (ZoneSpare * iFrag->Zone)) * BPMB);

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

TargetedFileFragments NewFSMap::GetWriteFileFragments( DWORD SecLength )
{
	TargetedFileFragments Frags;

	Frags.SectorOffset = 0;

	DWORD ProposedFragID = GetUnusedFragmentID();

	if ( ProposedFragID == 0 )
	{
		return Frags;
	}

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

	      iFrag  = SortedFrags.begin();
	DWORD Secs   = AbsLength / SecSize;

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

		DWORD ThisSecs = iFrag->Length / SecSize;

		if ( ThisSecs > Secs )
		{
			ThisSecs = Secs;
		}

		if ( iFrag->FragID == 0 )
		{
			ClaimFragmentByOffset( iFrag->FragOffset, ThisSecs, ProposedFragID );

			FileFragment f;

			f.Sector = SectorForFragmentOffset( iFrag->FragOffset );
			f.Sector = ThisSecs * SecSize;

			Frags.Frags.push_back( f );
			Frags.FragID = ProposedFragID;

			Secs -= ThisSecs;
		}

		iFrag++;
	}

	/* Write the changes to the disk/image */
	WriteFSMap();

	return Frags;
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
		DWORD IDsPerZone = ((1 << (10 + 3)) - ZoneSpare) / (IDLen + 1);
		
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
				DWORD FullAddress = ((iFrag->FragOffset - (ZoneSpare * iFrag->Zone)) * BPMB);

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
		DWORD IDsPerZone = ((1 << (10 + 3)) - ZoneSpare) / (IDLen + 1);
		
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
				DWORD FullAddress = ((iFrag->FragOffset - (ZoneSpare * iFrag->Zone)) * BPMB) + (SectorID * 0x0400);

				FileFragment f;

				f.Sector = FullAddress / SecSize;
				f.Length = iFrag->Length - ( SectorID * 0x0400 );

				frags.push_back( f );

				/* Sector offset is only valid for the first fragment */
				SectorID = 0;
			}
		}

		SearchZone = ( SearchZone + 1 ) % Zones;

		if ( SearchZone == StartZone )
		{
			AllZones = true;
		}
	}

	return frags;
}

BYTE NewFSMap::ZoneCheck( BYTE *map_base )
{
	DWORD sum_vector0 = 0;
	DWORD sum_vector1 = 0;
	DWORD sum_vector2 = 0;
	DWORD sum_vector3 = 0;
	DWORD rover;

	for (rover = 1020; rover>0; rover-=4)
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