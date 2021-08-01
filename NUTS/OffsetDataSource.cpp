#include "StdAfx.h"
#include "OffsetDataSource.h"


OffsetDataSource::OffsetDataSource( DWORD Offset, DataSource *pSource )
{
	SourceOffset = Offset;
	DataOffset   = Offset;
	pSrc         = pSource;

	PhysicalDiskSize = pSrc->PhysicalDiskSize;

	pSrc->Retain();
}


OffsetDataSource::~OffsetDataSource(void)
{
	pSrc->Release();
}

int	OffsetDataSource::ReadSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	QWORD NewOffset = SourceOffset + ( Sector * SectorSize );

	return pSrc->ReadRaw( NewOffset, SectorSize, ( BYTE * ) pSectorBuf );
}

int	OffsetDataSource::WriteSectorLBA( DWORD Sector, BYTE *pSectorBuf, DWORD SectorSize )
{
	QWORD NewOffset = SourceOffset + ( Sector * SectorSize );

	return pSrc->WriteRaw( NewOffset, SectorSize, ( BYTE * ) pSectorBuf );
}

int OffsetDataSource::ReadRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	return pSrc->ReadRaw( Offset + SourceOffset, Length, pBuffer );
}

int OffsetDataSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	return pSrc->WriteRaw( Offset + SourceOffset, Length, pBuffer );
}

int OffsetDataSource::Truncate( QWORD Length )
{
	return pSrc->Truncate( SourceOffset + Length );
}
