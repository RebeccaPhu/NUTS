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
			pSource->ReadRaw( ( Offset + sec ) * 256, 256, &Track[ sec * 256 ] );
		}

		Surface.Write( Track, 10 * 256 );

		Offset += 20; // Interleaved tracks

		if ( Offset >= Sectors )
		{
			break;
		}
	}

	Surface.Keep();

	return new NestedImageSource( this, &pDirectory->Files[FileID], Surface.Name() );
}

int AcornDFSDSD::ReplaceFile(NativeFile *pFile, CTempFile &store)
{
	DWORD Sectors = 2 * 80 * 10;

	QWORD Offset = 0;

	if ( pFile->fileID == 1 ) { Offset = 10U; }

	BYTE Track[ 2560 ];

	store.Seek( 0 );

	QWORD SourceData = store.Ext();
	QWORD DataSoFar  = 0;

	while ( 1 )
	{
		store.Read( Track, 10 * 256 );

		for ( BYTE sec=0; sec<10; sec++ )
		{
			if ( pSource->WriteRaw( ( Offset + sec ) * 256, 256, &Track[ sec * 256 ] ) != DS_SUCCESS )
			{
				return -1;
			}
		}

		Offset += 20; // Interleaved tracks

		DataSoFar += 10 * 256;

		if ( ( Offset >= Sectors ) || ( DataSoFar >= SourceData ) )
		{
			break;
		}
	}

	return 0;
}

