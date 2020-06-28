#include "StdAfx.h"

#include "../NUTS/CPM.h"
#include "AMSDOSFileSystem.h"
#include "AMSDOSDirectory.h"
#include "Defs.h"

CPMDPB AMSDOS_DPB = {
	/* .ShortFSName  = */ "AMSDOS",
	/* .Encoding     = */ ENCODING_CPC,
	/* .FSFileType   = */ FILE_AMSTRAD,
	/* .SysTracks    = */ 0,
	/* .SecsPerTrack = */ 9,
	/* .SecSize      = */ 0x200,
	/* .FSID         = */ FSID_AMSDOS,
	/* .DirSecs      = */ 4,
	/* .ExtentSize   = */ 1024,
	/* .Flags        = */ CF_UseHeaders,
	/* .HeaderSize   = */ 0x80,
};

AMSDOSFileSystem::AMSDOSFileSystem(DataSource *pDataSource) : CPMFileSystem( pDataSource, AMSDOS_DPB )
{
	/* Remove the directory object and create a new one */
	delete pDir;

	pDir = (CPMDirectory *) new AMSDOSDirectory( pDataSource, pMap, AMSDOS_DPB );

	pDirectory = (Directory *) pDir;
}

bool AMSDOSFileSystem::IncludeHeader( BYTE *pHeader )
{
	WORD sum = 0;

	for ( BYTE i=0; i<66; i++ )
	{
		sum += pHeader[ i ];
	}

	WORD expected = * (WORD *) &pHeader[ 67 ];

	if ( sum == expected )
	{
		return false;
	}

	return true;
}

FSHint AMSDOSFileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.FSID       = FSID;
	hint.Confidence = 0;

	if ( Extension != nullptr )
	{
		if ( rstrnicmp( Extension, (BYTE *) "DSK", 3 ) )
		{
			hint.Confidence += 10;
		}
	}

	WORD SecID = pSource->GetSectorID( 0 );

	/* CPC Data Disk */
	if ( ( SecID != 0xFFFF ) && ( SecID >= 0xC0 ) && ( SecID <= 0xCA ) )
	{
		hint.Confidence += 10;
	}

	/* CPC System Disk */
	if ( ( SecID != 0xFFFF ) && ( SecID >= 0x40 ) && ( SecID <= 0x4A ) )
	{
		hint.Confidence += 10;
	}

	return hint;
}