#include "stdafx.h"

#include "BAM.h"
#include "../NUTS/libfuncs.h"

extern const BYTE spt[41] = {
	0,    // Track 0 doesn't exist (it's used as a "no more" marker)
	21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,   // Tracks 1  - 17
	19, 19, 19, 19, 19, 19, 19,                                           // Tracks 18 - 24
	18, 18, 18, 18, 18, 18,                                               // Tracks 25 - 30
	17, 17, 17, 17, 17, /* 40-track disks from here */ 17, 17, 17, 17, 17 // Tracks 30 - 40
};

BYTE SecsPerTrack( BYTE Track )
{
	if ( Track >= 40 )
	{
		return 0;
	}

	return spt[ Track ];
}

DWORD SectorForLink( BYTE track, BYTE sector) {
	DWORD sect = 0;

	//	1541 disks have a variable tracks per sector count, so you have to add from the start to work
	//	out the logical sector from a track and sector pair, which is what is used in the sector links in the
	//	disk itself.

	if (track == 1)
	{
		return sector;	//	No track multiples for track 1.
	}

	//	Add the tps for each track up to, but not including, the target track.
	for ( BYTE t=1; t < track; t++ )
	{
		sect += spt[t];
	}

	//	Add on the offset
	sect += sector;

	return sect;
}

TSLink LinkForSector( DWORD Sector )
{
	BYTE Track = 1;

	TSLink TS = { 0, 0 };

	while ( Track <= 35 )
	{
		if ( Sector >= spt[ Track ] )
		{
			Sector -= spt[ Track ];
		}
		else
		{
			TS.Track  = Track;
			TS.Sector = (BYTE) Sector;

			return TS;
		}

		Track++;
	}

	return TS;
}

void SwapChars( BYTE *pString, WORD Len )
{
	for ( WORD i=0; i<Len; i++ )
	{
		if ( pString[ i ] == 0x00 )
		{
			pString[ i ] = 0xA0;
		}
		else if ( pString[ i ] == 0xA0 )
		{
			pString[ i ] = 0x00;
		}
	}
}

/* NOTE!!! This is not a proper PETSCII to ASCII conversion for a good reason:
   This function is used to convert FILENAMEs to ASCII. Since these are targeting an FS
   whose allowed character semantics are unknown, if this function needs to be called
   it assumes a restrictive FS that allows letters, numbers and a limited set of symbols
   ONLY. Hence, there's a lot of underscores.
*/
int MakeASCII( NativeFile *pFile )
{
	unsigned char ascii[256] = {
		'_', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '_', '_', '_', '_', '_',
		' ', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '-', '_', '_',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '_', '_', '_', '_', '_', '_',
		'_', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
		'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '_', '_', '_', '_', '_',
		'_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
		'_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
		'_', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '_', '_', '_', '_', '_',
		' ', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '-', '_', '_',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '_', '_', '_', '_', '_', '_',
		'_', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
		'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '_', '_', '_', '_', '_',
		'_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
		'_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_',
	};

	/* Technically, PETSCII 0 is a displayable character, and would resolve to 0 if poked
	   into the text mode screen memory on a C64. But NUTS always uses it as a terminator,
	   and if you PRINT'd a PETSCII 0 on a C64, you'd get nothing, so it works here.
	*/
	for ( WORD n=0; n<256; n++)
	{
		if ( pFile->Filename[ n ] != 0 )
		{
			pFile->Filename[ n ] = ascii[ pFile->Filename[ n ] ];
		}
	}

	for ( WORD n=0; n<4; n++)
	{
		if ( pFile->Extension[ n ] != 0 )
		{
			pFile->Extension[ n ] = ascii[ pFile->Extension[ n ] ];
		}
	}

	pFile->EncodingID = ENCODING_ASCII;

	return 0;
}

/* This does the reverse of the above; converts ASCII filenames to PETSCII */
void IncomingASCII( NativeFile *pFile )
{
	for ( WORD n=0; n<256; n++ )
	{
		if ( pFile->Filename[ n ] != 0 )
		{
			if ( ( pFile->Filename[ n ] >= 'a' ) && ( pFile->Filename[ n ] <= 'z' ) )
			{
				pFile->Filename[ n ] -= 0x20;
			}

			if ( pFile->Filename[ n ] == '\\' )
			{
				pFile->Filename[ n ] = '-';
			}

			if ( ( pFile->Filename[ n ] <= ' ' ) || ( pFile->Filename[ n ] >= ']' ) )
			{
				pFile->Filename[ n ] = '-';
			}
		}
	}

	/* Files not in ENCODING_PETSCII are assumed to be foreign, so should be auto-converted to PRG, etc */
	pFile->Flags |= FF_Extension;

	rstrncpy( pFile->Extension, (BYTE *) "PRG", 3 );
}
