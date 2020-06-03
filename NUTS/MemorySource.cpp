#include "StdAfx.h"
#include "MemorySource.h"

#include "../NUTS/NUTSError.h"


MemorySource::MemorySource( BYTE *pDataSet, DWORD DataSize ) : DataSource( )
{
	Size = DataSize;

	pData = (BYTE *) malloc( DataSize );

	memcpy( pData, pDataSet, DataSize );
}


MemorySource::~MemorySource(void)
{
	free( pData );
}

int MemorySource::ReadSector(long Sector, void *pSectorBuf, long SectorSize )
{
	return ReadRaw( Sector * SectorSize, SectorSize, (BYTE *) pSectorBuf );
}

int MemorySource::WriteSector(long Sector, void *pSectorBuf, long SectorSize)
{
	return WriteRaw( Sector * SectorSize, SectorSize, (BYTE *) pSectorBuf );
}

int MemorySource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	if ( ( Offset + Length ) < Size )
	{
		memcpy( pBuffer, &pData[ Offset ], Length );

		return 0;
	}

	return NUTSError( 0x90, L"Memory overrun in memory source (software bug)" );
}

int MemorySource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	if ( ( Offset + Length ) < Size )
	{
		memcpy( &pData[ Offset ], pBuffer, Length );

		return 0;
	}

	return NUTSError( 0x90, L"Memory overrun in memory source (software bug)" );
}
int MemorySource::Truncate( QWORD Length )
{
	pData = (BYTE *) realloc( pData, Length );

	Size = Length;

	return 0;
}

