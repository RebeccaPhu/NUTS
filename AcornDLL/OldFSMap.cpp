#include "StdAfx.h"
#include "OldFSMap.h"


int	OldFSMap::GetStartSector(FreeSpace &space) {

	std::vector<FreeSpace>::iterator	iSpace;

	for (iSpace = Spaces.begin(); iSpace != Spaces.end(); iSpace++) {
		if (iSpace->Length >= space.OccupiedSectors) {
			space.StartSector	= iSpace->StartSector;
			space.Length		= iSpace->Length;

			return 0;
		}
	}

	return -1;
}

int	OldFSMap::OccupySpace(FreeSpace &space) {

	std::vector<FreeSpace>::iterator	iSpace;

	for (iSpace = Spaces.begin(); iSpace != Spaces.end(); iSpace++) {
		if (iSpace->StartSector == space.StartSector) {
			iSpace->StartSector += space.OccupiedSectors;
			iSpace->Length -= space.OccupiedSectors;

			WriteFSMap();

			return 0;
		}
	}

	return -1;
}

int OldFSMap::ReadFSMap() {

	Spaces.clear();

	unsigned char	FSSectors[512];

	// Free space map occupies two sectors in track 0, so no translation needed
	pSource->ReadSector( 0, &FSSectors[0x000], 256);
	pSource->ReadSector( 1, &FSSectors[0x100], 256);

	DiscIdentifier	= * (unsigned short *) &FSSectors[0x1fb];
	NextEntry		= FSSectors[0x1fe];
	TotalSectors	= (* (unsigned long *) &FSSectors[0x0fb]) & 0xFFFFFF;
	BootOption		= FSSectors[0x1fd];

	int	ptr	= 0;
	int	e	= 0;

	while ((ptr < (3 * NextEntry)) && (e < 85)) {
		FreeSpace	fs;

		fs.StartSector	= (* (unsigned long *) &FSSectors[ptr + 000]) & 0xFFFFFF;
		fs.Length		= (* (unsigned long *) &FSSectors[ptr + 256]) & 0xFFFFFF;

//		char err[256];
//		sprintf_s(err, 256, "Read: %06X sectors at %06X\n", fs.Length, fs.StartSector);
//		OutputDebugStringA(err);

		Spaces.push_back(fs);

		ptr	+= 3;

		e++;
	}

	return 0;
}

int OldFSMap::WriteFSMap() {

	unsigned char	FSBytes[0x200];

	std::vector<FreeSpace>::iterator	iSpace;

	int	ptr = 0;

	* (unsigned long *) &FSBytes[0x0fc]	= TotalSectors;

	for (iSpace = Spaces.begin(); iSpace != Spaces.end(); iSpace++) {
		* (unsigned long *) &FSBytes[ptr + 0x000]	= iSpace->StartSector;
		* (unsigned long *) &FSBytes[ptr + 0x100]	= iSpace->Length;

		char err[256];
		sprintf_s(err, 256, "Write: %06X sectors at %06X\n", iSpace->Length, iSpace->StartSector);
		OutputDebugStringA(err);

		ptr += 3;
	}

	int	sum = 255;

	for (ptr = 254; ptr >= 0; ptr--) {
		if (sum > 255)
			sum = (sum + 1) & 0xFF;

		sum += FSBytes[ptr];
	}

	FSBytes[0xff]	= sum & 0xFF;

	* (unsigned short *) &FSBytes[0x1fb]	= DiscIdentifier;
	FSBytes[0x1fd]	= BootOption;
	FSBytes[0x1fe]	= NextEntry;

	sum	= 255;
	
	for (ptr = 254; ptr >= 0; ptr--) {
		if (sum > 255)
			sum = (sum + 1) & 0xFF;

		sum += FSBytes[ptr + 0x100];
	}

	FSBytes[0x1ff]	= sum & 0xff;

	// Free space map occupies two sectors in track 0, so no translation needed
	pSource->WriteSector( 0, &FSBytes[0x000], 256);
	pSource->WriteSector( 1, &FSBytes[0x100], 256);

	return 0;
}

