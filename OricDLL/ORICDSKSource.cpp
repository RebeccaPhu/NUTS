#include "StdAfx.h"
#include "ORICDSKSource.h"
#include "..\NUTS\NUTSError.h"
#include "..\NUTS\NUTSFlags.h"

ORICDSKSource::ORICDSKSource( DataSource *pRawSrc )
{
	pRawSource = pRawSrc;

	Feedback = L"Oric DSK Wrapper";

	Flags = DS_SupportsTruncate | DS_SupportsLLF | DS_AlwaysLLF;

	if ( pRawSrc != nullptr )
	{
		DS_RETAIN( pRawSrc );
	}

	ValidDSK = false;

	BYTE Buf[ 256 ];

	if ( pRawSource->ReadRaw( 0, 0x100, Buf ) != DS_SUCCESS )
	{
		return;
	}

	if ( memcmp( Buf, (void *) "ORICDISK", 8 ) == 0 )
	{
		ValidDSK = true;

		NumSides   = LEDWORD( &Buf[ 0x08 ] );
		NumTracks  = LEDWORD( &Buf[ 0x0C ] );
		NumSectors = LEDWORD( &Buf[ 0x10 ] );
	}
}


ORICDSKSource::~ORICDSKSource(void)
{
	if ( pRawSource != nullptr )
	{
		DS_RELEASE( pRawSource );
	}
}

int	ORICDSKSource::ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	// Proxy this through ReadRaw, skipping the header. Sectors are always 256 bytes with this format.

	return pRawSource->ReadRaw( ( Sector * 0x100 ) + 0x100, 0x100, pSectorBuf );
}

int	ORICDSKSource::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	// Proxy this through WriteRaw, skipping the header. Sectors are always 256 bytes with this format.

	return pRawSource->WriteRaw( ( Sector * 0x100 ) + 0x100, 0x100, pSectorBuf );
}

int ORICDSKSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	// Just add the offset here.

	return pRawSource->ReadRaw( Offset + 0x100, Length, pBuffer );
}

int ORICDSKSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	// Just add the offset here.

	return pRawSource->WriteRaw( Offset + 0x100, Length, pBuffer );
}


int ORICDSKSource::Truncate( QWORD Length )
{
	// Just add the offset here.

	return pRawSource->Truncate( Length + 0x100 );
}

void ORICDSKSource::StartFormat( )
{
	if ( pRawSource == nullptr )
	{
		return;
	}

	// Can't work with a complex media shape
	if ( ( DiskShapeSet ) && ( !ComplexDiskShape ) )
	{
		BYTE Buf[ 256 ];

		rstrncpy( Buf, (BYTE *) "ORICDISK", 8 );

		WLEDWORD( &Buf[ 0x08 ], MediaShape.Heads );
		WLEDWORD( &Buf[ 0x0C ], MediaShape.Tracks );
		WLEDWORD( &Buf[ 0x10 ], MediaShape.Sectors );

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

int ORICDSKSource::SeekTrack( WORD Track )
{
	if ( ( !DiskShapeSet ) || ( ComplexDiskShape ) )
	{
		return NUTSError( 0x70C, L"No disk shape set or a complex media shape in use. This cannot be used with this wrapper." );
	}

	FormatTrackPosition = 0x100 + ( Track * MediaShape.Sectors * 0x100 );

	return DS_SUCCESS;
}

int ORICDSKSource::WriteTrack( TrackDefinition track )
{
	if ( ( !DiskShapeSet ) || ( ComplexDiskShape ) )
	{
		return NUTSError( 0x70C, L"No disk shape set or a complex media shape in use. This cannot be used with this wrapper." );
	}

	// This wrapper discards everything except the sector content, but uses the sector IDs to set the position.

	for ( std::vector<SectorDefinition>::iterator iSector = track.Sectors.begin(); iSector != track.Sectors.end(); iSector++ )
	{
		DWORD WritePos = FormatTrackPosition;

		if ( FormatHead != 0 )
		{
			WritePos += ( MediaShape.Tracks * MediaShape.Sectors * 0x100 );
		}

		WritePos +=  iSector->SectorID * 0x100;

		if ( pRawSource->WriteRaw( WritePos, 0x100, iSector->Data ) != DS_SUCCESS )
		{
			return -1;
		}
	}

	return DS_SUCCESS;
}

void ORICDSKSource::EndFormat( void )
{
	ValidDSK = true;

	NumSides   = MediaShape.Heads;
	NumTracks  = MediaShape.Tracks;
	NumSectors = MediaShape.Sectors;
}

int ORICDSKSource::ReadSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	// We cannot use the disk shape here, as the disk shape is already in the image itself.

	if ( ValidDSK )
	{
		DWORD SectorsPerSide = MediaShape.Tracks * MediaShape.Sectors;

		QWORD Offset = 0x100;

		Offset += ( SectorsPerSide * Head ) * 0x100;

		Offset += ( Track * MediaShape.Sectors ) * 0x100;

		Offset += Sector * 0x100;

		return pRawSource->ReadRaw( Offset, 0x100, pSectorBuf );
	}

	return NUTSError( 0x70D, L"Invalid DSK container" );
}

int ORICDSKSource::WriteSectorCHS( DWORD Head, DWORD Track, DWORD Sector, BYTE *pSectorBuf )
{
	// We cannot use the disk shape here, as the disk shape is already in the image itself.

	if ( ValidDSK )
	{
		DWORD SectorsPerSide = MediaShape.Tracks * MediaShape.Sectors;

		QWORD Offset = 0x100;

		Offset += ( SectorsPerSide * Head ) * 0x100;

		Offset += ( Track * MediaShape.Sectors ) * 0x100;

		Offset += Sector * 0x100;

		return pRawSource->WriteRaw( Offset, 0x100, pSectorBuf );
	}

	return NUTSError( 0x70D, L"Invalid DSK container" );
}
