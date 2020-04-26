#include "StdAfx.h"
#include "ADFSDirectory.h"
#include "BBCFunctions.h"
#include "RISCOSIcons.h"
#include "../NUTS/libfuncs.h"

DWORD ADFSDirectory::TranslateSector(DWORD InSector)
{
	if ( !UseLFormat )
	{
		return InSector;
	}

	DWORD Track  = InSector / 16;
	DWORD Sector = InSector % 16;

	DWORD OutSec;

	if ( Track >= 80 )
	{
		OutSec = ( (Track -  80) * 32) + 16 + Sector;
	}
	else
	{
		OutSec = ( Track * 32 ) + Sector;
	}

	return OutSec;
}

int	ADFSDirectory::ReadDirectory( void ) {
	unsigned char	DirBytes[0x800];

	if ( !UseDFormat )
	{
		pSource->ReadSector( TranslateSector( DirSector + 0 ), &DirBytes[0x000], 256 );	
		pSource->ReadSector( TranslateSector( DirSector + 1 ), &DirBytes[0x100], 256 );	
		pSource->ReadSector( TranslateSector( DirSector + 2 ), &DirBytes[0x200], 256 );	
		pSource->ReadSector( TranslateSector( DirSector + 3 ), &DirBytes[0x300], 256 );	
		pSource->ReadSector( TranslateSector( DirSector + 4 ), &DirBytes[0x400], 256 );	
	}
	else
	{
		/* D Discs don't have to contend with Rebecca's stupid BeebEm design decisions */

		/* They do seem have peculiar ways of referring to sectors though. The address of a
		   directory is still in 256-byte sectors, even though the disc uses 1024-byte sectors
		   in it's logical structure. */
		pSource->ReadSector( DirSector + 0, &DirBytes[ 0x000 ], 256 );
		pSource->ReadSector( DirSector + 1, &DirBytes[ 0x100 ], 256 );
		pSource->ReadSector( DirSector + 2, &DirBytes[ 0x200 ], 256 );
		pSource->ReadSector( DirSector + 3, &DirBytes[ 0x300 ], 256 );
		pSource->ReadSector( DirSector + 4, &DirBytes[ 0x400 ], 256 );
		pSource->ReadSector( DirSector + 5, &DirBytes[ 0x500 ], 256 );
		pSource->ReadSector( DirSector + 6, &DirBytes[ 0x600 ], 256 );
		pSource->ReadSector( DirSector + 7, &DirBytes[ 0x700 ], 256 );
	}

	Files.clear();
	ResolvedIcons.clear();

	int	ptr,c;

	ptr	= 5;

	DWORD FileID = 0;

	NativeFile file;

	MasterSeq = DirBytes[0];

	if ( UseDFormat )
	{
		rstrncpy( (BYTE *) DirString, &DirBytes[0x7f0], 10 );
		rstrncpy( (BYTE *) DirTitle,  &DirBytes[0x7dd], 19 );
	}
	else
	{
		rstrncpy( (BYTE *) DirString, &DirBytes[0x4cc], 10);
		rstrncpy( (BYTE *) DirTitle,  &DirBytes[0x4d9], 19);
	}

	DWORD MaxPtr = 1227;

	if ( UseDFormat ) { MaxPtr = 3162; }

	/* This flag is set if any load address has the top 16 bits set to
	   anything other than FFFF or 0000, i.e. it looks like a RISC OS time stmap */
	LooksRISCOSIsh = false;

	while ( ptr < MaxPtr ) {
		memset( &file, 0, sizeof(file) );
		memcpy( &file.Filename, &DirBytes[ptr], 10 );

		if (file.Filename[0] == 0)
			break;

		if ( !UseDFormat )
		{
			if (file.Filename[0] & 128)
				file.AttrRead = 0xFFFFFFFF;

			if (file.Filename[1] & 128)
				file.AttrWrite = 0xFFFFFFFF;

			if (file.Filename[2] & 128)
				file.AttrLocked	= 0xFFFFFFFF;

			if (file.Filename[3] & 128)
				file.Flags |= FF_Directory;
		}
		else
		{
			if ( DirBytes[ ptr + 0x19 ] & 1 )
				file.AttrRead = 0xFFFFFFFF;

			if ( DirBytes[ ptr + 0x19 ] & 2 )
				file.AttrWrite = 0xFFFFFFFF;

			if ( DirBytes[ ptr + 0x19 ] & 4 )
				file.AttrLocked	= 0xFFFFFFFF;

			if ( DirBytes[ ptr + 0x19 ] & 8 )
				file.Flags |= FF_Directory;
		}		

		for (c=0; c<17; c++) {
			if ( (file.Filename[c] & 0x7F) == 0x0d)
				file.Filename[c] = 0;
			else
				file.Filename[c] &= 0x7f;
		}

		file.fileID   = FileID;
		file.LoadAddr = * (DWORD *) &DirBytes[ptr + 10];
		file.ExecAddr = * (DWORD *) &DirBytes[ptr + 14];
		file.Length   = * (DWORD *) &DirBytes[ptr + 18];
		file.SSector  = * (DWORD *) &DirBytes[ptr + 22]; file.SSector &= 0xFFFFFF;
		file.SeqNum   = DirBytes[ptr + 25];
		file.XlatorID = NULL;
		file.HasResolvedIcon = false;

		if ( ( file.ExecAddr & 0xFFF00000 ) == 0xFFF00000 )
		{
			if ( ( file.LoadAddr & 0xFFFF0000 ) != 0xFFFF0000 )
			{
				if ( ( file.LoadAddr &0xFFFF0000 ) != 0x00000000 )
				{
					LooksRISCOSIsh = true;
				}
			}
		}

		file.EncodingID = ENCODING_ACORN;
		file.FSFileType = FT_ACORN;

		if ( file.Flags && FF_Directory )
		{
			file.Type = FT_Directory;
			file.Icon = FT_Directory;
		}

		Files.push_back(file);

		ptr	+= 26;

		FileID++;
	}

	if ( !UseDFormat )
	{
		ParentSector = * (DWORD *) &DirBytes[ 1238 ]; ParentSector &= 0xFFFFFF;
	}
	else
	{
		ParentSector = * (DWORD *) &DirBytes[ 2010 ]; ParentSector &= 0xFFFFFF;
	}

	if ( UseDFormat )
	{
		/* Always true on D format discs */
		LooksRISCOSIsh = true;
	}

	NativeFileIterator iFile;

	for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		if ( LooksRISCOSIsh )
		{
			TranslateType( &*iFile );
		}
		else
		{
			/* Use plain old BBC interpreations */
			iFile->Type = FT_Binary;

			if (
				((iFile->ExecAddr & 0xFFFF) == 0x8023) ||
				((iFile->ExecAddr & 0xFFFF) == 0x802B)
				)
			{
				iFile->Type = FT_BASIC;
			}

			if (
				((iFile->ExecAddr & 0xFFFF) == 0xFFFF) &&
				((iFile->LoadAddr & 0xFFFF) == 0x0000)
				)
			{
				iFile->Type = FT_Spool;
			}

			if (
				((iFile->ExecAddr & 0xFFFF) == 0xFFFF) &&
				((iFile->LoadAddr & 0xFFFF) == 0xFFFF)
				)
			{
				iFile->Type = FT_Data;
			}

			if ( ( iFile->Length == 0x5000 ) && ( (iFile->LoadAddr & 0xFFFF) == 0x3000 ) )
			{
				iFile->Type = FT_Graphic; // Mode 0, 1 or 2
			}

			if ( ( iFile->Length == 0x4000 ) && ( (iFile->LoadAddr & 0xFFFF) == 0x4000 ) )
			{
				iFile->Type = FT_Graphic; // Mode 3
			}

			if ( ( iFile->Length == 0x2800 ) && ( (iFile->LoadAddr & 0xFFFF) == 0x5800 ) )
			{
				iFile->Type = FT_Graphic; // Mode 4 or 5
			}

			if ( ( iFile->Length == 0x6000 ) && ( (iFile->LoadAddr & 0xFFFF) == 0x2000 ) )
			{
				iFile->Type = FT_Graphic; // Mode 6
			}

			if ( ( iFile->Length == 0x400 ) && ( (iFile->LoadAddr & 0xFFFF) == 0x7C00 ) )
			{
				iFile->Type = FT_Graphic; // Mode 7
			}

			if ( iFile->Type == FT_Graphic )
			{
				iFile->XlatorID = GRAPHIC_ACORN;
			}

			if ( !( iFile->Flags & FF_Directory ) )
			{
				iFile->Icon = iFile->Type;
			}
		}
	}

	return 0;
}

int	ADFSDirectory::WriteDirectory( void ) {
	//	Sort directory entries
	int	i;
	bool sorted	= false;

	while (!sorted) {
		sorted	= true;

		for (i=0; i< (signed) Files.size() - 1; i++) {
			if (bbc_strcmp((char *) Files[i].Filename, (char *) Files[i+1].Filename) > 0) {
				NativeFile f  = Files[i];
				Files[i]      = Files[i+1];
				Files[i+1]    = f;

				sorted	= false;
			}
		}
	}

	//	Write directory structure
	BYTE DirectorySector[0x800];

	memset(&DirectorySector[0x000], 0, 0x800);

	MasterSeq = ( ( MasterSeq / 16 ) * 10 ) + ( MasterSeq % 16 );
	MasterSeq++;
	MasterSeq = ( ( MasterSeq / 10 ) * 16 ) + ( MasterSeq % 10 );

	DirectorySector[0x000]	= MasterSeq;
	DirectorySector[0x001]	= 'H';
	DirectorySector[0x002]	= 'u';
	DirectorySector[0x003]	= 'g';
	DirectorySector[0x004]	= 'o';

	if ( UseDFormat )
	{
		BBCStringCopy( (char *) &DirectorySector[0x7f0], DirString, 10);
		BBCStringCopy( (char *) &DirectorySector[0x7dd], DirTitle, 19);

		DirectorySector[0x7da]	= (unsigned char)   ParentSector & 0xFF;
		DirectorySector[0x7db]	= (unsigned char) ((ParentSector & 0xFF00) >> 8);
		DirectorySector[0x7dc]	= (unsigned char) ((ParentSector & 0xFF0000) >> 16);

		DirectorySector[0x7fa]	= MasterSeq;
		DirectorySector[0x7fb]	= 'H';
		DirectorySector[0x7fc]	= 'u';
		DirectorySector[0x7fd]	= 'g';
		DirectorySector[0x7fe]	= 'o';
	}
	else
	{
		BBCStringCopy( (char *) &DirectorySector[0x4cc], DirString, 10);
		BBCStringCopy( (char *) &DirectorySector[0x4d9], DirTitle, 19);

		DirectorySector[0x4d6]	= (unsigned char)   ParentSector & 0xFF;
		DirectorySector[0x4d7]	= (unsigned char) ((ParentSector & 0xFF00) >> 8);
		DirectorySector[0x4d8]	= (unsigned char) ((ParentSector & 0xFF0000) >> 16);

		DirectorySector[0x4fa]	= MasterSeq;
		DirectorySector[0x4fb]	= 'H';
		DirectorySector[0x4fc]	= 'u';
		DirectorySector[0x4fd]	= 'g';
		DirectorySector[0x4fe]	= 'o';
	}

	// Write directory entries
	std::vector<NativeFile>::iterator	iFile;

	int	ptr = 5;

	for (iFile = Files.begin(); iFile != Files.end(); iFile++) {
		BBCStringCopy((char *) &DirectorySector[ptr + 0], (char *) iFile->Filename, 10);

		* (DWORD *) &DirectorySector[ptr + 0x00a] = iFile->LoadAddr;
		* (DWORD *) &DirectorySector[ptr + 0x00e] = iFile->ExecAddr;
		* (DWORD *) &DirectorySector[ptr + 0x012] = (DWORD) iFile->Length;
		* (DWORD *) &DirectorySector[ptr + 0x016] = iFile->SSector;

		if ( !UseDFormat )
		{
			if (iFile->AttrRead)
				DirectorySector[ptr + 0] |= 0x80;

			if (iFile->AttrWrite)
				DirectorySector[ptr + 1] |= 0x80;

			if (iFile->AttrLocked)
				DirectorySector[ptr + 2] |= 0x80;

			if (iFile->Flags & FF_Directory)
				DirectorySector[ptr + 3] |= 0x80;

			DirectorySector[ptr + 0x019] = (BYTE) iFile->SeqNum;
		}
		else
		{
			DirectorySector[ ptr + 0x19 ] = 0;

			if (iFile->AttrRead)
			{
				DirectorySector[ ptr + 0x19 ] |= 1;
				DirectorySector[ ptr + 0x19 ] |= 16;
			}

			if (iFile->AttrWrite)
			{
				DirectorySector[ ptr + 0x19 ] |= 2;
				DirectorySector[ ptr + 0x19 ] |= 32;
			}

			if (iFile->AttrLocked)
			{
				DirectorySector[ ptr + 0x19 ] |= 4;
			}

			if (iFile->Flags & FF_Directory)
			{
				DirectorySector[ ptr + 0x19 ] |= 8;
			}
		}		

		ptr	+= 26;
	}

	pSource->WriteSector( TranslateSector( DirSector + 0 ), &DirectorySector[0x000], 256 );
	pSource->WriteSector( TranslateSector( DirSector + 1 ), &DirectorySector[0x100], 256 );
	pSource->WriteSector( TranslateSector( DirSector + 2 ), &DirectorySector[0x200], 256 );
	pSource->WriteSector( TranslateSector( DirSector + 3 ), &DirectorySector[0x300], 256 );
	pSource->WriteSector( TranslateSector( DirSector + 4 ), &DirectorySector[0x400], 256 );

	if ( UseDFormat )
	{
		pSource->WriteSector( DirSector + 5, &DirectorySector[0x500], 256 );
		pSource->WriteSector( DirSector + 6, &DirectorySector[0x600], 256 );
		pSource->WriteSector( DirSector + 7, &DirectorySector[0x700], 256 );
	}

	return 0;
}

void ADFSDirectory::SetSector( DWORD Sector )
{
	DirSector = Sector;
}

void ADFSDirectory::TranslateType( NativeFile *file )
{
	DWORD Type = ( file->LoadAddr & 0x000FFF00 ) >> 8;

	file->RISCTYPE = Type;

	file->Type = RISCOSIcons::GetIntTypeForType( Type );
	file->Icon = RISCOSIcons::GetIconForType( Type );

	/*
	if ( file->Icon == 0x0 )
	{
		file->Icon            = RISCOSIcons::GetIconForType( FT_Arbitrary );
		file->HasResolvedIcon = true;
	}
	*/

	/* This timestamp converstion is a little esoteric. RISC OS uses centiseconds
	   since 0:0:0@1/1/1900. This is *nearly* unixtime. */
	QWORD UnixTime = ( ( (QWORD) file->ExecAddr ) << 8 ) | (file->LoadAddr & 0xFF);

	UnixTime /= 100;
	UnixTime -= 2208988800; // Secs difference between 1/1/1970 and 1/1/1900.

	file->TimeStamp = (DWORD) UnixTime;

	if ( file->Flags & FF_Directory )
	{
		file->Type = FT_Directory;

		file->RISCTYPE = 0xA00;

		if ( file->Filename[ 0 ] == '!' )
		{
			file->Icon = RISCOSIcons::GetIconForType( 0xA01 );

			file->RISCTYPE = 0xA01;
		}
		else
		{
			file->Icon = RISCOSIcons::GetIconForType( 0xA00 );
		}
	}

	switch ( file->RISCTYPE )
	{
		case 0xFFF: // Text
		case 0xFFE: // EXEC
		case 0xFEB: // Obey
			{
				file->XlatorID = TUID_TEXT;
			}
			break;

		case 0xFFB: // BASIC
			{
				file->XlatorID = BBCBASIC;
			}
			break;
	}
}
