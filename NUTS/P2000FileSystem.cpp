#include "StdAfx.h"
#include "P2000FileSystem.h"
#include "P2000Directory.h"

CPMDPB P2000_DPB = {
	/* .ShortFSName  = */ "P2000C",
	/* .Encoding     = */ ENCODING_ASCII,
	/* .FSFileType   = */ L"P2000DISK",
	/* .SysTracks    = */ 2,
	/* .SecsPerTrack = */ 16,
	/* .SecSize      = */ 0x100,
	/* .FSID         = */ L"P2000CPM",
	/* .DirSecs      = */ 4,
	/* .ExtentSize   = */ 1024,
	/* .Flags        = */ CF_Force8bit,
	/* .HeaderSize   = */ 0,
};

P2000FileSystem::P2000FileSystem(DataSource *pDataSource) : CPMFileSystem( pDataSource, P2000_DPB )
{
	/* Remove the directory object and create a new one */
	delete pDir;

	P2000_DPB.Encoding = ENCODING_ASCII;
	dpb.Encoding      = ENCODING_ASCII;

	pDir = (CPMDirectory *) new P2000Directory( pDataSource, pMap, P2000_DPB );

	pDirectory = (Directory *) pDir;

	Flags =
		FSF_Creates_Image | FSF_Formats_Image |
		FSF_SupportBlocks | FSF_SupportFreeSpace | FSF_Capacity |
		FSF_FixedSize | FSF_UseSectors | FSF_No_Quick_Format | FSF_Uses_Extensions;
}


P2000FileSystem::~P2000FileSystem(void)
{
}


FSHint P2000FileSystem::Offer( BYTE *Extension )
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

	SectorIDSet set = pSource->GetTrackSectorIDs( 0, 0, false );

	/* +3DOS Disk */
	if ( ( set.size() > 0 ) && ( set[ 0 ] >= 0x01 ) && ( set[ 0 ] <= 0x09 ) )
	{
		hint.Confidence += 10;
	}

	return hint;
}