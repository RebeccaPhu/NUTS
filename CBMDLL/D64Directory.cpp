#include "StdAfx.h"
#include "D64Directory.h"
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
		pSource->ReadSector(SectorForLink(nt, ns), d64cache, 256);

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

				_snprintf_s( (char *) file.Extension, 4, 3, (char *) extns[ ft ] );

				if ( ft >= 5 )
				{
					file.Flags = 0; // No extension for these types
				}

				memcpy( file.Filename, &fp[0x5], 16 );

				Shorten( file.Filename );

//				PETSCII(fname, 16);

				//	Getting the length is a fun one. The directory entry doesn't ACTUALLY give the length in
				//	bytes of the file, only in blocks - for listing purposes - i.e. it's there so your C64
				//	can show "FILE   [5A]" to give you an idea of the size. The actual size can be found (if required -
				//	the 1541/DOS doesn't do so) by trapesing the file sectors. Each data sector has 2 bytes of link,
				//	and 254 bytes of data. So start at the sector indicated by the entry, read the sector in and follow
				//	the trail. If the link gives a non-zero track, add 254 to the length and follow to the next sector.
				//	Otherwise, the sector part gives the bytes consumed in the last sector, and the job is done.

				file.Length	= 0;

				int	lt	= fp[0x3];
				int	ls	= fp[0x4];

				unsigned char	filecache[256];

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

				file.EncodingID = ENCODING_PETSCII;

				Files.push_back(file);
			}
		}
	}

	return 0;
}

int	D64Directory::WriteDirectory(void) {
	return 0;
}


void D64Directory::PETSCII(unsigned char *pptr, int chars) {
	unsigned char ascii[256] = {
		'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '~',
		' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
		'_', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
		'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '@', '@', '@', '@', '@',
		'@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@',
		'@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@',
		'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '~',
		' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
		'_', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
		'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '@', '@', '@', '@', '@',
		'@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@',
		'@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@', '@'
	};

	for (int n=0; n<16; n++)
		pptr[n]	= ascii[pptr[n]];

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

int	D64Directory::SectorForLink(int track, int sector) {
	int	sect	= 0;

	//	1541 disks have a variable tracks per sector count, so you have to add from the start to work
	//	out the logical sector from a track and sector pair, which is what is used in the sector links in the
	//	disk itself.

	int	tps[41] = {
		0,		//	Track 0 doesn't exist (it's used as a "no more" marker)
		21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,		//	Tracks 1  - 17
		19, 19, 19, 19, 19, 19, 19,												//	Tracks 18 - 24
		18, 18, 18, 18, 18, 18,													//	Tracks 25 - 30
		17, 17, 17, 17, 17, /* 40-track disks from here */ 17, 17, 17, 17, 17	//	Tracks 30 - 40
	};

	if (track == 1)
		return sector;	//	No track multiples for track 1.

	//	Add the tps for each track up to, but not including, the target track.
	for (int t=1; t < track; t++)
		sect	+= tps[t];

	//	Add on the offset
	sect	+= sector;

	return sect;
}