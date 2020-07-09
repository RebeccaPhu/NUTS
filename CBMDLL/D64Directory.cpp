#include "StdAfx.h"
#include "D64Directory.h"
#include "CBMFunctions.h"
#include "OpenCBMPlugin.h"
#include "../nuts/libfuncs.h"

int	D64Directory::ReadDirectory(void) {
	Files.clear();

	DWORD FileID = 0;

	int	nt,ns;

	//	Start at track 18 sector 1
	nt	= 18;
	ns	= 1;

	unsigned char d64cache[256];

	FileType ftypes[8] = {
		FT_Arbitrary, //DEL
		FT_Data,      // SEQ
		FT_Binary,    // PRG
		FT_Data,      // USR
		FT_Data,      // REL
		FT_None,      // Illegal
		FT_None,      // Illegal
		FT_None       // Illegal
	};

	char extns[8][4] = { "DEL", "SEQ", "PRG", "USR", "REL", "", "", "" };

	while (nt != 0) {
		if ( pSource->ReadSector(SectorForLink(nt, ns), d64cache, 256) != DS_SUCCESS )
		{
			return -1;
		}

		nt	= d64cache[0];
		ns	= d64cache[1];

		for (int f=0; f<8; f++) {
			unsigned char	*fp	= &d64cache[f * 0x20];

			if (fp[0x03] != 0) {	//	Files can't be on track 0, so this is an indicator of a null entry.
				NativeFile	file;

				BYTE ft = fp[0x02] & 0x7;

				file.fileID        = FileID++;
				file.Flags         = FF_Extension; //	No sub-dir support, but all files have an extn
				file.Attributes[0] = SectorForLink(fp[0x3], fp[0x4]);
				file.Attributes[1] = ft;
				file.Type          = ftypes[ ft ];
				file.Icon          = ftypes[ ft ];
				file.FSFileType    = FT_C64;
				file.XlatorID      = NULL;
				file.HasResolvedIcon = false;

				rsprintf( file.Extension, extns[ ft ] );

				if ( fp[ 0x02 ] & 0x80 ) { file.Attributes[ 2 ] = 0xFFFFFFFF; } else { file.Attributes[ 2 ] = 0x00000000; }
				if ( fp[ 0x02 ] & 0x40 ) { file.Attributes[ 3 ] = 0xFFFFFFFF; } else { file.Attributes[ 3 ] = 0x00000000; }

				if ( ft >= 5 )
				{
					file.Flags = 0; // No extension for these types
				}

				if ( ft == 0x04 )
				{
					/* REL files. Icky. */
					BYTE RELTrack = fp[ 0x15 ];
					BYTE RELSect  = fp[ 0x16 ];

					file.Attributes[ 4 ] = SectorForLink( RELTrack, RELSect );
					file.Attributes[ 5 ] = fp[ 0x17 ];
				}
				else
				{
					file.Attributes[ 4 ] = 0;
					file.Attributes[ 5 ] = 0;
				}

				SwapChars( &fp[ 0x05 ], 16 );

				rstrncpy( file.Filename, &fp[ 0x05 ], 16 );

				//	Getting the length is a fun one. The directory entry doesn't ACTUALLY give the length in
				//	bytes of the file, only in blocks - for listing purposes - i.e. it's there so your C64
				//	can show "FILE   [5A]" to give you an idea of the size. The actual size can be found (if required -
				//	the 1541/DOS doesn't do so) by trapesing the file sectors. Each data sector has 2 bytes of link,
				//	and 254 bytes of data. So start at the sector indicated by the entry, read the sector in and follow
				//	the trail. If the link gives a non-zero track, add 254 to the length and follow to the next sector.
				//	Otherwise, the sector part gives the bytes consumed in the last sector, and the job is done.

				/* If we're using a REAL 1541 drive then this will make getting the directory PAINFULLY slow. So
				   use an approximation from the sector count, and then be more specific later. */

				file.Length	= * (WORD *) &fp[ 0x1E ];

				/* This gives an approx calculation as a starting point */
				file.Length *= 254;

				if ( !NoLengthChase )
				{
					int	lt	= fp[0x3];
					int	ls	= fp[0x4];

					unsigned char filecache[256];

					file.Length = 0;

					while (1) {
						pSource->ReadSector(SectorForLink(lt, ls), filecache, 256);

						lt	= filecache[0];
						ls	= filecache[1];

						if (lt == 0) {
							file.Length	+= filecache[1];

							break;
						} else {
							file.Length	+= 254;
						}
					}
				}

				file.EncodingID = ENCODING_PETSCII;

				Files.push_back(file);
			}
		}
	}

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	return 0;
}

int	D64Directory::WriteDirectory(void) {
	/* This will be slightly different. We start at track 18 sector 1 as usual, but we'll read the sector in to get the next sector link.
	   If we run out before we're finished (e.g. a new file was added that requires a new sector for the directory, we'll claim one and
	   tack it on. */

	int	nt,ns;

	//	Start at track 18 sector 1
	nt	= 18;
	ns	= 1;

	DWORD CFile = 0;

	BYTE Buffer[ 256 ];

	bool SectorsGrabbed = false;

	while ( CFile <= Files.size() ) {
		if ( pSource->ReadSector( SectorForLink( nt, ns ), Buffer, 256 ) != DS_SUCCESS )
		{
			return -1;
		}
				
		ZeroMemory( &Buffer[ 2 ], 254 );

		for (int f=0; f<8; f++) {
			unsigned char *fp = &Buffer[f * 0x20];

			if ( CFile >= Files.size() )
			{
				continue;
			}

			NativeFile *pFile = &Files[ CFile ];

			fp[ 0x02 ] = pFile->Attributes[ 1 ] & 0x7;

			if ( pFile->Attributes[ 2 ] ) { fp[ 0x02 ] |= 0x40; }
			if ( pFile->Attributes[ 3 ] ) { fp[ 0x02 ] |= 0x80; }

			TSLink Loc = LinkForSector( pFile->Attributes[ 0 ] );

			fp[ 0x03 ] = Loc.Track;
			fp[ 0x04 ] = Loc.Sector;

			Loc = LinkForSector( pFile->Attributes[ 4 ] );

			fp[ 0x15 ] = Loc.Track;
			fp[ 0x16 ] = Loc.Sector;
			fp[ 0x17 ] = (BYTE) pFile->Attributes[ 5 ];

			rstrncpy( &fp[ 0x05 ], pFile->Filename, 16 );

			SwapChars( &fp[ 0x05 ], 16 );

			WORD Sectors = pFile->Length / 254;

			if ( pFile->Length % 254 ) { Sectors++; }

			fp[ 0x1E ] = (BYTE) Sectors & 0xFF;
			fp[ 0x1F ] = (BYTE) ( Sectors >> 8 );

			CFile++;
		}

		if ( ( CFile < Files.size() ) && ( Buffer[ 0 ] == 0 ) )
		{
			/* need another sector */
			TSLink NextLoc = pBAM->GetFreeTS();

			Buffer[ 0 ] = NextLoc.Track;
			Buffer[ 1 ] = NextLoc.Sector;

			SectorsGrabbed = true;
		}

		if ( CFile >= Files.size() )
		{
			Buffer[ 0 ] = 0;
			Buffer[ 1 ] = 0xFF;
		}

		if ( pSource->WriteSector( SectorForLink( nt, ns ), Buffer, 256 ) != DS_SUCCESS )
		{
			if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

			return -1;
		}

		nt = Buffer[ 0 ];
		ns = Buffer[ 1 ];

		if ( CFile >= Files.size() )
		{
			break;
		}
	}

	/* IF we grabbed sectors to extend the directory, we'll need to write the BAM back */
	if ( SectorsGrabbed )
	{
		if ( pBAM->WriteBAM() != DS_SUCCESS )
		{
			if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

			return -1;
		}
	}

	if ( IsOpenCBM ) { OpenCBM_CloseDrive( Drive ); }

	return 0;
}


void D64Directory::Shorten( unsigned char *dptr ) {
	dptr[ 16 ] = 0;

	int	 n	= 15;

	while (n >= 0) {
		if ((dptr[n] & 0x7f) == 0x20)
			dptr[n]	= 0;
		else
			return;

		n--;
	}
}

