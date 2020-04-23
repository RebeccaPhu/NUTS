#include "StdAfx.h"
#include "AmigaDirectory.h"

#include "../NUTS/libfuncs.h"
#include "Defs.h"

int AmigaDirectory::ReadDirectory(void)
{
	Files.clear();
	ResolvedIcons.clear();

	FileID = 0;

	BYTE HTSector[ 512 ];

	/* Read the hash table sector first */
	pSource->ReadSector( DirSector, HTSector, 512 );

	DWORD Ptr = 0x18;

	while ( Ptr < 0x138 ) /* End of hash table */
	{
		DWORD NSector = BEDWORD( &HTSector[ Ptr ] );

		if ( NSector != 0 )
		{
			ProcessHashChain( NSector );
		}

		Ptr += 4;
	}

	ParentSector = BEDWORD( &HTSector[ 0x1F4 ] );

	return 0;
}

int AmigaDirectory::WriteDirectory(void)
{

	return 0;
}

void AmigaDirectory::ProcessHashChain( DWORD NSector )
{
	BYTE FSector[ 512 ];

	pSource->ReadSector( NSector, FSector, 512 );

	NativeFile file;

	file.EncodingID      = ENCODING_AMIGA;
	file.fileID          = FileID;
	file.Flags           = 0;
	file.FSFileType      = FILE_AMIGA;
	file.HasResolvedIcon = false;
	file.Icon            = FT_Arbitrary;
	file.Type            = FT_Arbitrary;
	file.XlatorID        = 0;
	file.Length          = BEDWORD( &FSector[ 0x144 ] );

	rstrncpy( file.Filename, &FSector[ 0x1b1 ], FSector[ 0x1b0 ] );

	DWORD FType = BEDWORD( &FSector[ 0x1FC ] );

	if ( FType == 0x00000002 )
	{
		file.Flags |= FF_Directory;
		file.Icon   = FT_Directory;
		file.Type   = FT_Directory;
	}

	DWORD next_hash = BEDWORD( &FSector[ 0x1F0 ] );

	file.Attributes[ 0 ] = NSector;
	file.Attributes[ 1 ] = 0;

	Files.push_back( file );

	FileID++;

	if ( next_hash != 0 )
	{
		ProcessHashChain( next_hash );
	}
}
