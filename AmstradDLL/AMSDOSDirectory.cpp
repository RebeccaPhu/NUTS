#include "StdAfx.h"

#include "AMSDOSDirectory.h"
#include "../NUTS/Preference.h"
#include "../NUTS/NUTSFlags.h"
#include "Defs.h"
#include "../NUTS/libfuncs.h"

void AMSDOSDirectory::ExtraReadDirectory( NativeFile *pFile )
{
	if ( pFile->Flags & FF_Extension )
	{
		if ( rstrnicmp( pFile->Extension, (BYTE *) "BAS", 3 ) )
		{
			pFile->Type = FT_BASIC;
			pFile->Icon = FT_BASIC;

			pFile->XlatorID = TUID_LOCO;
		}
	}

	bool SlowEnhance = (bool) Preference( L"SlowEnhance", false );

	if ( ( pSource->Flags & DS_SlowAccess ) && ( !SlowEnhance ) )
	{
		/* This will cause mondo extra disk reads, so don't do it if the data source is slow,
		   e.g. an actual floppy disk */
		return;
	}

	/* First we'll need the start block. We may be called multiple times, so check the attributes if
	    we've been here before. */

	if ( pFile->Attributes[ 0xF ] == 0 )
	{
		BYTE Header[ 512 ];
		BYTE CPMHeader[ 32 ];

		ExportCPMDirectoryEntry( pFile, CPMHeader );

		/* Calculate where the sector is */
		DWORD Adx = DirSector + ( pFile->Attributes[ 1 ] * 2 );

		WORD Track  = Adx / 9;
		WORD Sector = Adx % 9;

		SectorIDSet set = pSource->GetTrackSectorIDs( 0, Track, true );

		if ( set.size() < ( Sector + 1 ) )
		{
			return;
		}

		pSource->ReadSectorCHS( 0, Track, set[ Sector ], Header );

		/* Checksum the header, as AMSDOS uses headerless files sometimes because **** you, that's why */
		WORD sum = 0;

		for ( BYTE i=0; i<66; i++ )
		{
			sum += Header[ i ];
		}

		WORD expected = * (WORD *) &Header[ 67 ];

		if ( ( sum == expected ) && ( CPMDirectoryEntryCMP( CPMHeader, Header ) ) )
		{
			/* We can now enhance the length */
			pFile->Length = * (WORD *) &Header[ 24 ];

			pFile->Attributes[ 5 ] = * (WORD *) &Header[ 21 ];
			pFile->Attributes[ 6 ] = * (WORD *) &Header[ 26 ];

			pFile->Attributes[ 7 ] = Header[ 18 ];

			pFile->Attributes[ 8 ] = 0xFFFFFFFF;
		}
		else
		{
			pFile->Attributes[ 8 ] = 0x00000000;
		}

		/* Prevent redoing this */
		pFile->Attributes[ 0xF ] = 0xFFFFFFFF;
	}
}