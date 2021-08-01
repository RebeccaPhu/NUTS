#include "StdAfx.h"
#include "ADFSDirectory.h"
#include "BBCFunctions.h"
#include "RISCOSIcons.h"
#include "../NUTS/libfuncs.h"

#include <algorithm>

int	ADFSDirectory::ReadDirectory( void ) {
	unsigned char	DirBytes[0x800];

	int Err = 0;

	if ( !UseDFormat )
	{
		Err += ReadTranslatedSector( DirSector + 0, &DirBytes[ 0x000 ], 256, pSource );
		Err += ReadTranslatedSector( DirSector + 1, &DirBytes[ 0x100 ], 256, pSource );
		Err += ReadTranslatedSector( DirSector + 2, &DirBytes[ 0x200 ], 256, pSource );
		Err += ReadTranslatedSector( DirSector + 3, &DirBytes[ 0x300 ], 256, pSource );
		Err += ReadTranslatedSector( DirSector + 4, &DirBytes[ 0x400 ], 256, pSource );
	}
	else
	{
		/* D Format is weird. It uses 1024-byte sectors, but references them as if they
		   are 256-byte sectors. So the second physical sector is 1, but the system refers
		   to it as 4.
		*/
		Err += ReadTranslatedSector( ( DirSector >> 2 ) + 0, &DirBytes[ 0x000 ], 1024, pSource );
		Err += ReadTranslatedSector( ( DirSector >> 2 ) + 1, &DirBytes[ 0x400 ], 1024, pSource );
	}

	if ( Err != DS_SUCCESS )
	{
		return -1;
	}

	Files.clear();
	ResolvedIcons.clear();

	DWORD ptr,c;

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

	while ( ptr < MaxPtr ) {
		for ( c=0; c<16; c++) { file.Attributes[ c ] = 0; }

		file.Flags    = 0;

		file.Filename = BYTEString( &DirBytes[ptr], 10 );

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

			if (file.Filename[4] & 128)
			{
				file.AttrExec   = 0xFFFFFFFF;
				file.AttrLocked = 0x00000000;
				file.AttrRead   = 0x00000000;
				file.AttrWrite  = 0x00000000;
			}
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

		for (c=0; c<10; c++) {
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

		if ( ( FSID == FSID_ADFS_L2 ) || ( FSID == FSID_ADFS_D ) || ( FSID == FSID_ADFS_HO ) )
		{
			file.EncodingID = ENCODING_RISCOS;
		}
		else
		{
			file.EncodingID = ENCODING_ACORN;
		}

		file.FSFileType = FT_ACORNX;

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

	NativeFileIterator iFile;

	for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		if ( ( FSID == FSID_ADFS_L2 ) || ( FSID == FSID_ADFS_D ) || ( FSID == FSID_ADFS_HO ) )
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

			if ( iFile->Type == FT_Graphic )
			{
				iFile->XlatorID = GRAPHIC_ACORN;
			}

			if ( iFile->Type == FT_BASIC )
			{
				iFile->XlatorID = BBCBASIC;
			}
		}
	}

	return 0;
}

static bool ADFSSort( NativeFile &a, NativeFile &b )
{
	/* ADFS is rather stupid about this. A comes first where B.substr( A.length() ) == A. But case-insensitve, coz why not */

	/* std::sort expects true when b comes after a */

	WORD alen = rstrnlen( a.Filename, 10 );
	WORD blen = rstrnlen( b.Filename, 10 );

	/* this one should never happen, but anyway */
	if ( ( rstrnicmp( a.Filename, b.Filename, alen ) ) && ( alen == blen ) ) 
	{
		return false;
	}

	/* A is a subset of B */
	if ( ( rstrnicmp( a.Filename, b.Filename, alen ) ) && ( alen < blen ) )
	{
		return true;
	}

	/* B is a subset of A */
	if ( ( rstrnicmp( a.Filename, b.Filename, blen ) ) && ( blen < alen ) )
	{
		return false;
	}

	/* Here A and B are completely different to each other. So use bbc_strcmp, which works like strncmp */
	if ( bbc_strcmp( a.Filename, b.Filename ) > 0 )
	{
		return false;
	}

	return true;
}

int	ADFSDirectory::WriteDirectory( void ) {
	//	Sort directory entries
	std::sort( Files.begin(), Files.end(), ADFSSort );

	//	Write directory structure
	BYTE DirectorySector[0x800];

	ZeroMemory( &DirectorySector[0x000], 0x800 );

	MasterSeq = ( ( MasterSeq / 16 ) * 10 ) + ( MasterSeq % 16 );
	MasterSeq++;
	MasterSeq = ( ( MasterSeq / 10 ) * 16 ) + ( MasterSeq % 10 );

	/* There seems to be confusion as to whether this should be Hugo or Nick on D format.
	   The use of new directory format suggests Nick, but Apps Disc 1 & 2 have Hugo.
	   It seems Hugo refers to Old FS Map (absolute addresses, contiguous files) and Nick
	   refers to New FS Map (indirect addresses, fragmented files). */
	DirectorySector[0x000]	= MasterSeq;
	DirectorySector[0x001]	= 'H';
	DirectorySector[0x002]	= 'u';
	DirectorySector[0x003]	= 'g';
	DirectorySector[0x004]	= 'o';

	if ( UseDFormat )
	{
		BBCStringCopy( &DirectorySector[0x7f0], DirString, 10 );
		BBCStringCopy( &DirectorySector[0x7dd], DirTitle,  19 );

		DirectorySector[0x7d8]  = 0;
		DirectorySector[0x7d9]  = 0;
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
		BBCStringCopy( &DirectorySector[0x4cc], DirString, 10 );
		BBCStringCopy( &DirectorySector[0x4d9], DirTitle,  19 );

		DirectorySector[0x4d6]	= (unsigned char)   ParentSector & 0xFF;
		DirectorySector[0x4d7]	= (unsigned char) ((ParentSector & 0xFF00) >> 8);
		DirectorySector[0x4d8]	= (unsigned char) ((ParentSector & 0xFF0000) >> 16);

		DirectorySector[0x4f9]  = 0;
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
		BBCStringCopy( &DirectorySector[ptr + 0], iFile->Filename, 10);

		* (DWORD *) &DirectorySector[ptr + 0x00a] = iFile->LoadAddr;
		* (DWORD *) &DirectorySector[ptr + 0x00e] = iFile->ExecAddr;
		* (DWORD *) &DirectorySector[ptr + 0x012] = (DWORD) iFile->Length;
		* (DWORD *) &DirectorySector[ptr + 0x016] = iFile->SSector;

		/* SML uses top bits of first four chars of filename. D uses an attribute byte */
		if ( !UseDFormat )
		{
			if ( !iFile->AttrExec )
			{
				if (iFile->AttrRead)
					DirectorySector[ptr + 0] |= 0x80;

				if (iFile->AttrWrite)
					DirectorySector[ptr + 1] |= 0x80;

				if (iFile->AttrLocked)
					DirectorySector[ptr + 2] |= 0x80;
			}
			else
			{
				DirectorySector[ ptr + 4 ] |= 0x80;
			}

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

	if ( UseDFormat )
	{
		/* D requires a check byte */
		DWORD Accumulator = 0;

		/* Directory words, up to, but not including the end-of-files marker */
		for ( WORD i=0; i < (ptr & 0xFFF8); i+=4 )
		{
			DWORD v = * (DWORD *) &DirectorySector[ i ] ;

			Accumulator = v ^ ( ( Accumulator >> 13 ) | ( Accumulator << 19 ) );
		}

		/* Trailing directory bytes */
		for ( WORD i=(ptr & 0xFFF8); i < ptr; i++ )
		{
			DWORD v = DirectorySector[ i ] ;

			Accumulator = v ^ ( ( Accumulator >> 13 ) | ( Accumulator << 19 ) );
		}

		/* Directory tail words */
		for ( WORD i=0x7D8; i < 0x7FC; i+=4 )
		{
			DWORD v = * (DWORD *) &DirectorySector[ i ] ;

			Accumulator = v ^ ( ( Accumulator >> 13 ) | ( Accumulator << 19 ) );
		}

		DirectorySector[ 0x7FF ] = ( BYTE) ( ( Accumulator & 0xFF ) ^ ( ( Accumulator & 0xFF00 ) >> 8 ) ^ ( ( Accumulator & 0xFF0000 ) >> 16 ) ^ ( ( Accumulator &0xFF000000 ) >> 24 ) );
	}

	int Err = 0;

	if ( !UseDFormat )
	{
		Err += WriteTranslatedSector( DirSector + 0, &DirectorySector[ 0x000 ], 256, pSource );
		Err += WriteTranslatedSector( DirSector + 1, &DirectorySector[ 0x100 ], 256, pSource );
		Err += WriteTranslatedSector( DirSector + 2, &DirectorySector[ 0x200 ], 256, pSource );
		Err += WriteTranslatedSector( DirSector + 3, &DirectorySector[ 0x300 ], 256, pSource );
		Err += WriteTranslatedSector( DirSector + 4, &DirectorySector[ 0x400 ], 256, pSource );
	}
	else
	{
		Err += WriteTranslatedSector( ( DirSector >> 2 ) + 0, &DirectorySector[ 0x000 ], 1024, pSource );
		Err += WriteTranslatedSector( ( DirSector >> 2 ) + 1, &DirectorySector[ 0x400 ], 1024, pSource );
	}

	return Err;
}

void ADFSDirectory::SetSector( DWORD Sector )
{
	DirSector = Sector;
}

