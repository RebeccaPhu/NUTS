#include "StdAfx.h"
#include "DOS3FileSystem.h"

#include "../NUTS/CPM.h"
#include "DOS3FileSystem.h"
#include "DOS3Directory.h"
#include "SinclairDefs.h"

CPMDPB DOS3_DPB = {
	/* .ShortFSName  = */ "+3DOS",
	/* .Encoding     = */ ENCODING_SINCLAIR,
	/* .FSFileType   = */ FT_SINCLAIR,
	/* .SysTracks    = */ 1,
	/* .SecsPerTrack = */ 9,
	/* .SecSize      = */ 0x200,
	/* .FSID         = */ FSID_DOS3,
	/* .DirSecs      = */ 4,
	/* .ExtentSize   = */ 1024,
	/* .Flags        = */ CF_UseHeaders,
	/* .HeaderSize   = */ 0x80,
};

DOS3FileSystem::DOS3FileSystem(DataSource *pDataSource) : CPMFileSystem( pDataSource, DOS3_DPB )
{
	/* Remove the directory object and create a new one */
	delete pDir;

	pDir = (CPMDirectory *) new DOS3Directory( pDataSource, pMap, DOS3_DPB );

	pDirectory = (Directory *) pDir;
}

bool DOS3FileSystem::IncludeHeader( BYTE *pHeader )
{
	if ( !rstrnicmp( pHeader, (BYTE *) "PLUS3DOS", 8 ) )
	{
		return true;
	}

	WORD sum = 0;

	for ( BYTE i=0; i<0x7F; i++ )
	{
		sum += pHeader[ i ];
	}

	sum &= 0xFF;

	if ( pHeader[ 0x7F ] == (BYTE) sum )
	{
		return false;
	}

	return true;
}

FSHint DOS3FileSystem::Offer( BYTE *Extension )
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

	/* +3DOS Disk */
	if ( ( SecID != 0xFFFF ) && ( SecID >= 0x01 ) && ( SecID <= 0x09 ) )
	{
		hint.Confidence += 10;
	}

	return hint;
}