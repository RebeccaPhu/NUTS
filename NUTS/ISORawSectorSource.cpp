#include "StdAfx.h"
#include "ISORawSectorSource.h"
#include "NUTSError.h"

#include "ISOECC.h"

ISORawSectorSource::ISORawSectorSource( DataSource *pRoot ) : DataSource( )
{
	pUpperSource = pRoot;

	SectorType = ISOSECTOR_AUTO;

	DS_RETAIN( pUpperSource );
}

ISORawSectorSource::~ISORawSectorSource(void)
{
	DS_RELEASE( pUpperSource );
}

#define SECTOR_TYPE_CHECK() if ( SectorType == ISOSECTOR_AUTO ) { AutoDetectType(); } \
	if ( SectorType == ISOSECTOR_UNKNOWN ) { return NUTSError( 0x81, L"Unknown ISO Sector type" ); }

int ISORawSectorSource::ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	SECTOR_TYPE_CHECK();

	QWORD Offset = Sector;

	Offset *= RAW_ISO_SECTOR_SIZE;

	if ( pUpperSource->ReadRaw( Offset, RAW_ISO_SECTOR_SIZE, RawSector ) != DS_SUCCESS )
	{
		return -1;
	}

	ZeroMemory( CheckSector, RAW_ISO_SECTOR_SIZE );

	switch ( SectorType )
	{
	case ISOSECTOR_MODE1:
		memcpy( pSectorBuf, &RawSector[ 0x10 ], min( 0x800, SectorSize ) );
		memcpy( CheckSector, RawSector, min( 0x810, SectorSize ) );
		break;

	case ISOSECTOR_MODE2:
		memcpy( pSectorBuf, &RawSector[ 0x10 ], min( 0x920, SectorSize ) );
		break;

	case ISOSECTOR_XA_M2F1:
		memcpy( pSectorBuf, &RawSector[ 0x18 ], min( 0x800, SectorSize ) );
		memcpy( CheckSector, RawSector, min( 0x810, SectorSize ) );
		break;

	case ISOSECTOR_XA_M2F2:
		memcpy( pSectorBuf, &RawSector[ 0x18 ], min( 0x914, SectorSize ) );
		memcpy( CheckSector, RawSector, min( 0x924, SectorSize ) );
		break;
	}

	eccedc_generate( CheckSector, SectorType );

	return DS_SUCCESS;
}

#define BCD(x) ( ( ( x / 10 ) << 4 ) | ( ( x % 10 ) & 0xF ) )

int ISORawSectorSource::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	SECTOR_TYPE_CHECK();

	QWORD Offset = Sector;

	Offset *= RAW_ISO_SECTOR_SIZE;

	ZeroMemory( RawSector, RAW_ISO_SECTOR_SIZE );

	switch (SectorType )
	{
	case ISOSECTOR_MODE1:
		memcpy( &RawSector[ 0x10 ], pSectorBuf, min( 0x800, SectorSize ) );
		break;
	case ISOSECTOR_MODE2:
		memcpy( &RawSector[ 0x10 ], pSectorBuf, min( 0x920, SectorSize ) );
		break;
	case ISOSECTOR_XA_M2F1:
		memcpy( &RawSector[ 0x18 ], pSectorBuf, min( 0x800, SectorSize ) );
		break;
	case ISOSECTOR_XA_M2F2:
		memcpy( &RawSector[ 0x18 ],pSectorBuf,  min( 0x914, SectorSize ) );
		break;
	}

	// Sector ID is in minutes, seconds and frames, written in BCD starting at 00:02:00.
	// 75 frames per second, 60 seconds per minute. No hours count.
	const BYTE SecSig[ 12 ] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

	memcpy( RawSector, SecSig, 12 );

	BYTE Frames = (BYTE) ( Sector % 75 );
	WORD Secs   = (BYTE) ( Sector / 75 );

	Secs += 2;

	BYTE RSecs = (BYTE) ( Secs % 60 );
	BYTE RMins = (BYTE) ( Secs / 60 );

	RawSector[ 12 ] = BCD( RMins );
	RawSector[ 13 ] = BCD( RSecs );
	RawSector[ 14 ] = BCD( Frames );

	if ( SectorType == ISOSECTOR_MODE1 )
	{
		RawSector[ 15 ] = 1;
	}
	else
	{
		RawSector[ 15 ] = 2;
	}

	// For CDROM/XA need to fill in a subheader
	if ( ( SectorType == ISOSECTOR_XA_M2F1 ) || ( SectorType == ISOSECTOR_XA_M2F2 ) )
	{
		RawSector[ 16 ] = RawSector[ 20 ] = 0; // File number - not used here
		RawSector[ 17 ] = RawSector[ 21 ] = 0; // Channel number - also not used
		RawSector[ 18 ] = RawSector[ 22 ] = 8; // Submode - DATA - see below
		RawSector[ 19 ] = RawSector[ 23 ] = 0; // Coding info - 0 for data sectors

		if ( SectorType == ISOSECTOR_XA_M2F2 )
		{
			RawSector[ 18 ] = RawSector[ 22] = 8 + 32; // Indicate Form 2.
		}

		// NOTE: There are extra bits involved here re: Byte 18/22. All VDs should have
		// bit 0 set, with the VST additionally having bit 7 set. Last sector of Path Tables,
		// files and directories should have both bit 0 and bit 7 set.

		if ( WriteHints.size() > 0 )
		{
			for ( std::vector<DSWriteHint>::iterator iHint = WriteHints.begin(); iHint != WriteHints.end(); iHint++ )
			{
				if ( iHint->HintName == L"CDROMXA_SubHeader" )
				{
					ISOWriteHint *hint = (ISOWriteHint *) iHint->Hint;

					if ( hint->IsEOF )
					{
						RawSector[ 18 ] |= 128;
					}

					if ( hint->IsEOR )
					{
						RawSector[ 18 ] |= 1;
					}

					RawSector[ 22 ] = RawSector[ 18 ];
				}
			}
		}
	}

	eccedc_generate( RawSector, SectorType );

	if ( pUpperSource->WriteRaw( Offset, RAW_ISO_SECTOR_SIZE, RawSector ) != DS_SUCCESS )
	{
		return -1;
	}

	return DS_SUCCESS;
}

int ISORawSectorSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	SECTOR_TYPE_CHECK();

	BYTE PureSector[ 0x920 ];

	DWORD SectorSize = 0x800;

	if ( SectorType == ISOSECTOR_MODE2 )
	{
		SectorSize = 0x920;
	}

	if ( SectorType == ISOSECTOR_XA_M2F2 )
	{
		SectorSize = 0x914;
	}

	QWORD Sector1 = Offset / SectorSize;
	QWORD Offset1 = Offset % SectorSize;
	DWORD Offset2 = 0;
	QWORD Length1 = SectorSize - Offset1;

	DWORD BytesToGo = Length;

	while ( BytesToGo > 0 )
	{
		if ( ReadSectorLBA( Sector1, PureSector, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		memcpy( &pBuffer[ Offset2 ], PureSector, min( Length1, min( SectorSize, BytesToGo ) ) );

		Sector1++;
		Offset1 = 0;
		Length1 = SectorSize;
		
		BytesToGo -= min( SectorSize, BytesToGo );
	}

	return DS_SUCCESS;
}

int ISORawSectorSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	SECTOR_TYPE_CHECK();

	BYTE PureSector[ 0x920 ];

	DWORD SectorSize = 0x800;

	if ( SectorType == ISOSECTOR_MODE2 )
	{
		SectorSize = 0x920;
	}

	if ( SectorType == ISOSECTOR_XA_M2F2 )
	{
		SectorSize = 0x914;
	}

	QWORD Sector1 = Offset / SectorSize;
	QWORD Offset1 = Offset % SectorSize;
	DWORD Offset2 = 0;
	QWORD Length1 = SectorSize - Offset1;

	DWORD BytesToGo = Length;

	while ( BytesToGo > 0 )
	{
		if ( ReadSectorLBA( Sector1, PureSector, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		memcpy( PureSector, &pBuffer[ Offset2 ], min( Length1, min( SectorSize, BytesToGo ) ) );

		if ( WriteSectorLBA( Sector1, PureSector, SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		Sector1++;
		Offset1 = 0;
		Length1 = SectorSize;
		
		BytesToGo -= min( SectorSize, BytesToGo );
	}

	return DS_SUCCESS;
}

void ISORawSectorSource::AutoDetectType( void )
{
	pUpperSource->ReadSectorLBA( 16, RawSector, RAW_ISO_SECTOR_SIZE );

	const BYTE ISOID[ 5 ] = { 'C', 'D', '0', '0', '1' };
	const BYTE HSID[ 5 ]  = { 'C', 'D', 'R', 'O', 'M' };

	// Let's look at byte 15, which gives us the base mode
	if ( RawSector[ 0x0F ] == 1 )
	{
		// Mode 1 - easy
		SectorType = ISOSECTOR_MODE1;

		Feedback = L"CDROM Mode 1";
	}
	else if ( RawSector[ 0x0F ] == 2 )
	{
		// Some kind of Mode 2. Could be pure mode 2, or an XA type.
		
		// If CD001 is at 0x11, or CDROM at 0x19, then it's pure mode 2.
		if ( ( memcmp( &RawSector[ 0x11 ], ISOID, 5 ) == 0 ) || ( memcmp( &RawSector[ 0x19 ], HSID, 5 ) == 0 ) )
		{
			SectorType = ISOSECTOR_MODE2;

			Feedback = L"CDROM Mode 2";
		}
		else
		{
			BYTE SH_Byte = RawSector[ 0x12 ];

			if ( SH_Byte & 0x20 )
			{
				SectorType = ISOSECTOR_XA_M2F1;

				Feedback = L"CDROM/XA Mode 2 Form 1";
			}
			else
			{
				SectorType = ISOSECTOR_XA_M2F1;

				Feedback = L"CDROM/XA Mode 2 Form 2";
			}
		}
	}

	PhysicalDiskSize = pUpperSource->PhysicalDiskSize;

	// Still auto?
	if ( SectorType == ISOSECTOR_AUTO )
	{
		// Dunno
		SectorType = ISOSECTOR_UNKNOWN;
	}
}
