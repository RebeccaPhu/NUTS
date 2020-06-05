#include "StdAfx.h"
#include "BAM.h"

#include "CBMFunctions.h"
#include "../NUTS/libfuncs.h"
#include "OpenCBMPlugin.h"

int BAM::ReadBAM( void )
{
	BYTE Buffer[ 256 ];

	if ( pSource->ReadSector( SectorForLink( 18, 0 ), Buffer, 256 ) != DS_SUCCESS )
	{
		return -1;
	}

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	SwapChars( &Buffer[ 0x90 ], 16 );

	rstrncpy( DiskName, &Buffer[ 0x90 ], 16 );
	rstrncpy( DiskID,   &Buffer[ 0xA2 ], 2 );
	rstrncpy( DOSID,    &Buffer[ 0xA5 ], 2 );

	DosVer = Buffer[ 0x02 ];

	BYTE Track = 1;

	for ( BYTE o = 0x04; o<0x90; o += 4 )
	{
		for ( BYTE b=0; b<4; b++ )
		{
			BAMData[ Track ][ b ] = Buffer[ o + b ];
		}

		Track++;
	}

	return 0;
}

TSLink BAM::GetFreeTS( void )
{
	TSLink TS = { 0, 0 };

	for ( BYTE t=1; t<=35; t++ )
	{
		for ( BYTE b=0; b<spt[ t ]; b++ )
		{
			BYTE TB = b / 8;
			BYTE Bb = b % 8;

			/* First byte is free sector count and not part of the BAM */
			if ( BAMData[ t ][ TB + 1 ] & ( 1 << Bb ) )
			{
				BAMData[ t ][ TB + 1 ] &= 0xFF ^ ( 1 << Bb );

				TS.Track  = t;
				TS.Sector = b;

				return TS;
			}
		}
	}

	return TS;
}

DWORD BAM::CountFreeSectors( void )
{
	WORD Sectors = 0;

	for ( BYTE t=1; t<=35; t++ )
	{
		for ( BYTE b=0; b<spt[ t ]; b++ )
		{
			BYTE TB = b / 8;
			BYTE Bb = b % 8;

			if ( BAMData[ t ][ TB + 1 ] & ( 1 << Bb ) )
			{
				Sectors++;
			}
		}
	}

	return Sectors;
}

void BAM::ReleaseSector( BYTE Track, BYTE Sector )
{
	if ( Track > 35 )
	{
		return;
	}

	if ( Sector > spt[ Track ] )
	{
		return;
	}

	BYTE TB = Sector / 8;
	BYTE Bb = Sector % 8;

	BAMData[ Track ][ TB + 1] |= 1 << Bb;
}

void BAM::OccupySector( BYTE Track, BYTE Sector )
{
	if ( Track > 35 )
	{
		return;
	}

	if ( Sector > spt[ Track ] )
	{
		return;
	}

	BYTE TB = Sector / 8;
	BYTE Bb = Sector % 8;

	BAMData[ Track ][ TB + 1] &= 0xFF ^ ( 1 << Bb );
}

int BAM::WriteBAM( void )
{
	BYTE Buffer[ 256 ];

	ZeroMemory( Buffer, 256 );

	Buffer[ 0 ] = 18;
	Buffer[ 1 ] = 1;

	Buffer[ 0x02 ] = DosVer;

	rstrncpy( &Buffer[ 0x90 ], DiskName, 16 );
	rstrncpy( &Buffer[ 0xA2 ], DiskID, 2 );
	rstrncpy( &Buffer[ 0xA5 ], DOSID, 2 );

	SwapChars( &Buffer[ 0x90 ], 16 );

	memset( &Buffer[ 0xA0 ], 0xA0, 2 );
	memset( &Buffer[ 0xA7 ], 0xA0, 4 );

	Buffer[ 0xA4 ] = 0xA0;

	BYTE Track = 1;

	for ( BYTE o = 0x04; o<0x90; o += 4 )
	{
		for ( BYTE b = 0; b < 4; b++ )
		{
			Buffer[ o + b ] = BAMData[ Track ][ b ];
		}

		Track++;
	}

	if ( pSource->WriteSector( SectorForLink( 18, 0 ), Buffer, 256 ) != DS_SUCCESS )
	{
		return -1;
	}

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	return 0;
}

bool BAM::IsFreeBlock( BYTE Track, BYTE Sector )
{
	BYTE TB = Sector / 8;
	BYTE Bb = Sector % 8;

	if ( BAMData[ Track ][ TB + 1 ] & ( 1 << Bb ) )
	{
		return true;
	}

	return false;
}