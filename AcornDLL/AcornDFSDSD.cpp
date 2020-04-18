#include "StdAfx.h"
#include "AcornDFSDSD.h"
#include "../NUTS/NestedImageSource.h"

DataSource *AcornDFSDSD::FileDataSource( DWORD FileID )
{
	QWORD MaxDisk = pSource->PhysicalDiskSize;

	CTempFile Surface;

	QWORD Offset = 0;

	if ( FileID == 1 ) { Offset = 10U; }

	DWORD Sectors = (DWORD) MaxDisk / 256U;

	BYTE Track[ 2560 ];

	while ( 1 )
	{
		for ( BYTE sec=0; sec<10; sec++ )
		{
			pSource->ReadSector( (long) Offset + sec, &Track[ sec * 256 ], 256 );
		}

		Surface.Write( Track, 10 * 256 );

		Offset += 20; // Interleaved tracks

		if ( Offset >= Sectors )
		{
			break;
		}
	}

	Surface.Keep();

	std::wstring Path( UString( (char *) Surface.Name().c_str() ) );

	return new NestedImageSource( this, &pDirectory->Files[FileID], Path );
}


