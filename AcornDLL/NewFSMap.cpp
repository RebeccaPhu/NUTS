#include "StdAfx.h"
#include "NewFSMap.h"
#include "../NUTS/libfuncs.h"

#define PHASE_READ_ID     0
#define PHASE_TRAIL       1
#define PHASE_LEADING_BIT 2

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
	}

	/* Disc record starts at sector 4. */
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

		DWORD MapAddr = ((Zones / 2) * (8*1024-ZoneSpare)-ZZ)*BPMB;
		DWORD Bits    = 0;

		for ( WORD z = 0; z<Zones; z++ )
		{
			DWORD SubMapAddr = MapAddr + ( 1024 * z );

			DWORD Offset = 4;

			pSource->ReadSector( SubMapAddr / 1024, Sector, 1024 );

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

			ReadSubMap( &Sector[ Offset ], (1024 - Offset) * 8, z, Bits );

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

int	NewFSMap::WriteFSMap()
{

	return 0;
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

				f.Sector = FullAddress / 1024;
				f.Length = iFrag->Length;

				frags.push_back( f );
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
