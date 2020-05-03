#include "StdAfx.h"
#include "OldFSMap.h"


int	OldFSMap::GetStartSector(FreeSpace &space) {

	std::vector<FreeSpace>::iterator iSpace;

	for (iSpace = Spaces.begin(); iSpace != Spaces.end(); iSpace++) {
		if (iSpace->Length >= space.OccupiedSectors) {
			space.StartSector = iSpace->StartSector;
			space.Length      = iSpace->Length;

			return 0;
		}
	}

	return -1;
}

int	OldFSMap::OccupySpace(FreeSpace &space) {

	std::vector<FreeSpace>::iterator iSpace;

	for (iSpace = Spaces.begin(); iSpace != Spaces.end(); iSpace++) {
		if (iSpace->StartSector == space.StartSector) {
			iSpace->StartSector += space.OccupiedSectors;
			iSpace->Length      -= space.OccupiedSectors;

			WriteFSMap();

			return 0;
		}
	}

	return -1;
}

int	OldFSMap::ClaimSpace(FreeSpace &space)
{
	/* See if there's an exact fitting space that already exists */
	std::vector<FreeSpace>::iterator iSpace;

	for (iSpace = Spaces.begin(); iSpace != Spaces.end(); iSpace++)
	{
		if ( ( iSpace->StartSector == space.StartSector ) && ( iSpace->Length == space.Length ) )
		{
			/* Found a slot that matches exactly. Just delete it. */
			Spaces.erase( iSpace );

			WriteFSMap();

			return 0;
		}
	}

	/* No exact space. Find one that cover it, and split it */
	for (iSpace = Spaces.begin(); iSpace != Spaces.end(); )
	{
		if ( ( iSpace->StartSector <= space.StartSector ) && ( iSpace->StartSector + iSpace->Length >= space.StartSector + space.Length ) )
		{
			/* Found a space that covers the block. There are 3 kinds of overlap, start end, finish end, both ends. */
			FreeSpace ExSpace;

			if ( iSpace->StartSector == space.StartSector )
			{
				/* Start end identical, length longer */
				iSpace->StartSector = iSpace->StartSector + space.Length;
				iSpace->Length      = iSpace->Length      - space.Length;

				iSpace = Spaces.end();
			}
			else if ( iSpace->StartSector + iSpace->Length == space.StartSector + space.Length )
			{
				/* Finish end identical, length longer */
				iSpace->Length = iSpace->Length - space.Length;

				iSpace = Spaces.end();
			}
			else
			{
				/* Both ends different, we must split the space */
				ExSpace.StartSector = space.StartSector + space.Length;
				ExSpace.Length      = iSpace->Length - ( ExSpace.StartSector - iSpace->StartSector );

				iSpace->Length      = space.StartSector - iSpace->StartSector;

				iSpace++;

				Spaces.insert( iSpace, ExSpace );

				iSpace = Spaces.end();
			}
		}

		if ( iSpace != Spaces.end() )
		{
			iSpace++;
		}
	}

	/* This may have resulted in adjacent spaces */
	std::vector<FreeSpace>::iterator iPoint = Spaces.begin();

	if ( Spaces.size() > 1 )
	{
		for (iSpace = Spaces.begin(); iSpace != Spaces.end(); ) {
			if ( iSpace != Spaces.begin() )
			{
				iPoint = iSpace;
				iPoint--;

				if ( ( iPoint->StartSector + iPoint->Length ) == iSpace->StartSector )
				{
					/* This sector runs on from the previous. Extend the previous, and delete this. */
					iPoint->Length += iSpace->Length;

					iSpace = Spaces.erase( iSpace );
				}
				else
				{
					/* The two sectors don't run together. Move on. */
					iSpace++;
				}
			}
			else
			{
				/* The very first space cannot run on from the previous space */
				iSpace++;
			}
		}
	}

	return 0;
}

int OldFSMap::ReleaseSpace(FreeSpace &space)
{
	/* Returning space is harder. We need to insert the free space at the appropriate point,
	   then concatentate adjacent spaces */

	std::vector<FreeSpace>::iterator iSpace;
	std::vector<FreeSpace>::iterator iPoint = Spaces.begin();

	for (iSpace = Spaces.begin(); iSpace != Spaces.end(); iSpace++) {
		if ( iSpace->StartSector < space.StartSector )
		{
			iPoint = iSpace;
		}
	}

	Spaces.insert( iPoint++, space );

	/* The vector now technically contains the free space, but if we left it this way the
	   map would fill with little bits when they shouldn't.
	*/

	if ( Spaces.size() > 1 )
	{
		for (iSpace = Spaces.begin(); iSpace != Spaces.end(); ) {
			if ( iSpace != Spaces.begin() )
			{
				iPoint = iSpace;
				iPoint--;

				if ( ( iPoint->StartSector + iPoint->Length ) == iSpace->StartSector )
				{
					/* This sector runs on from the previous. Extend the previous, and delete this. */
					iPoint->Length += iSpace->Length;

					iSpace = Spaces.erase( iSpace );
				}
				else
				{
					/* The two sectors don't run together. Move on. */
					iSpace++;
				}
			}
			else
			{
				/* The very first space cannot run on from the previous space */
				iSpace++;
			}
		}
	}

	/* Finally, store it */
	WriteFSMap();

	return 0;
}

int OldFSMap::ReadFSMap() {

	Spaces.clear();

	BYTE FSSectors[512];

	int Err = 0;

	// Free space map occupies two sectors in track 0, so no translation needed
	Err += pSource->ReadSector( 0, &FSSectors[0x000], 256);
	Err += pSource->ReadSector( 1, &FSSectors[0x100], 256);

	if ( Err != DS_SUCCESS )
	{
		return -1;
	}

	DiscIdentifier	= * (WORD *) &FSSectors[0x1fb];
	NextEntry		= FSSectors[0x1fe];
	TotalSectors	= (* (DWORD *) &FSSectors[0x0fc]) & 0xFFFFFF;
	BootOption		= FSSectors[0x1fd];

	memcpy( DiscName0, &FSSectors[ 0x0F7 ], 5 );
	memcpy( DiscName1, &FSSectors[ 0x1F6 ], 5 );

	int	ptr	= 0;
	int	e	= 0;

	while ((ptr < (3 * NextEntry)) && (e < 85)) {
		FreeSpace	fs;

		fs.StartSector	= (* (DWORD *) &FSSectors[ptr + 000]) & 0xFFFFFF;
		fs.Length		= (* (DWORD *) &FSSectors[ptr + 256]) & 0xFFFFFF;

		/* Neither a space starting at sector 0 (The FS map itself!) or having a zero-length are valid */

		if ( ( fs.StartSector != 0 ) && ( fs.Length != 0 ) )
		{
			Spaces.push_back(fs);
		}

		ptr	+= 3;

		e++;
	}

	return 0;
}

int OldFSMap::WriteFSMap() {

	BYTE FSBytes[0x200];

	ZeroMemory( FSBytes, 0x200 );

	std::vector<FreeSpace>::iterator iSpace;

	int	ptr  = 0;
	DWORD NS = 0;

	for (iSpace = Spaces.begin(); iSpace != Spaces.end(); iSpace++) {
		/* Neither a space starting at sector 0 (The FS map itself!) or having a zero-length are valid */
		if ( ( iSpace->StartSector != 0 ) && ( iSpace->Length != 0 ) )
		{
			* (DWORD *) &FSBytes[ptr + 0x000] = iSpace->StartSector;
			* (DWORD *) &FSBytes[ptr + 0x100] = iSpace->Length;

			ptr += 3;

			NS++;
		}
	}

	* (DWORD *) &FSBytes[0x0fc]	= TotalSectors;

	memcpy( &FSBytes[ 0x0F7 ], DiscName0, 5 );

	int	sum = 0;
	int c   = 0;

	for (ptr = 254; ptr >= 0; ptr--) {
		sum += c;

		c = 0;

		sum += FSBytes[ptr];

		if ( sum > 0xFF )
		{
			sum &= 0xFF;
			c    = 1;
		}
	}

	FSBytes[0xff]	= sum & 0xFF;

	* (WORD *) &FSBytes[0x1fb]	= DiscIdentifier;

	FSBytes[0x1fd]	= BootOption;
	FSBytes[0x1fe]	= NS * 3;

	memcpy( &FSBytes[ 0x1F6 ], DiscName1, 5 );

	sum = 0;
	c   = 0;

	for (ptr = 254; ptr >= 0; ptr--) {
		sum += c;

		c = 0;

		sum += FSBytes[ptr + 0x100];

		if ( sum > 0xFF )
		{
			sum &= 0xFF;
			c    = 1;
		}
	}

	FSBytes[0x1ff]	= sum & 0xff;

	int Err = DS_SUCCESS;

	// Free space map occupies two sectors in track 0, so no translation needed
	Err += pSource->WriteSector( 0, &FSBytes[0x000], 256);
	Err += pSource->WriteSector( 1, &FSBytes[0x100], 256);

	return Err;
}

