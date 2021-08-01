#include "stdafx.h"

#include "TranslatedSector.h"
#include "../NUTS/NUTSError.h"

TranslatedSector::TranslatedSector()
{
	FloppyFormat = false;
}

int TranslatedSector::ReadTranslatedSector( QWORD LBA, BYTE *Buffer, WORD SectorSize, DataSource *pSource )
{
	if ( FloppyFormat )
	{
		QWORD Head   = LBA / ( MediaShape.Tracks * MediaShape.Sectors );
		QWORD HLBA   = LBA % ( MediaShape.Tracks * MediaShape.Sectors );
		QWORD Track  = HLBA / MediaShape.Sectors;
		QWORD Sector = HLBA % MediaShape.Sectors;

		return pSource->ReadSectorCHS( (BYTE) Head, (BYTE) Track, (BYTE) Sector, Buffer );
	}
	else
	{
		return pSource->ReadSectorLBA( LBA, Buffer, SectorSize );
	}

	return NUTSError( 0xFFF, L"Internal application error: Boolean was not boolean" );
}

int TranslatedSector::WriteTranslatedSector( QWORD LBA, BYTE *Buffer, WORD SectorSize, DataSource *pSource )
{
	if ( FloppyFormat )
	{
		QWORD Head   = LBA / ( MediaShape.Tracks * MediaShape.Sectors );
		QWORD HLBA   = LBA % ( MediaShape.Tracks * MediaShape.Sectors );
		QWORD Track  = HLBA / MediaShape.Sectors;
		QWORD Sector = HLBA % MediaShape.Sectors;

		return pSource->WriteSectorCHS( (BYTE) Head, (BYTE) Track, (BYTE) Sector, Buffer );
	}
	else
	{
		return pSource->WriteSectorLBA( LBA, Buffer, SectorSize );
	}

	return NUTSError( 0xFFF, L"Internal application error: Boolean was not boolean" );
}
