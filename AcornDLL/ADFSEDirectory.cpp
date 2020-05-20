#include "StdAfx.h"
#include "ADFSEDirectory.h"
#include "RISCOSIcons.h"
#include "BBCFunctions.h"
#include "../NUTS/NUTSError.h"
#include "../NUTS/libfuncs.h"
#include "Defs.h"

#include <algorithm>

int	ADFSEDirectory::ReadDirectory( void ) {

	FileFragments Frags = pMap->GetFileFragments( DirSector );

	FileFragment_iter iFrag;

	DWORD TotalSize = 0;

	BYTE DirBytes[ 0x800 ];

	DWORD ReadSize = 0;

	for ( iFrag = Frags.begin(); iFrag != Frags.end(); iFrag++ )
	{
		DWORD Sectors = iFrag->Length / pMap->SecSize;

		if ( iFrag->Length % pMap->SecSize ) { Sectors++; }

		for ( DWORD sec = 0; sec<Sectors; sec++ )
		{
			pSource->ReadSector( iFrag->Sector + sec, &DirBytes[ ReadSize ], pMap->SecSize );

			ReadSize += pMap->SecSize;

			if ( ReadSize >= 0x800 )
			{
				break;
			}
		}
	}

	Files.clear();
	ResolvedIcons.clear();

	int	ptr,c;

	ptr	= 5;

	DWORD FileID = 0;

	NativeFile file;

	MasterSeq = DirBytes[0];

	memcpy( DirTitle, &DirBytes[0x7DD], 18);
	memcpy( DirName,  &DirBytes[0x7F0], 10);

	while ( ptr < 1227 ) {
		memset( &file, 0, sizeof(file) );
		memcpy( &file.Filename, &DirBytes[ptr], 10 );

		if (file.Filename[0] == 0)
			break;

		if ( DirBytes[ ptr + 0x19 ] & 1 )
			file.AttrRead = 0xFFFFFFFF;

		if ( DirBytes[ ptr + 0x19 ] & 2 )
			file.AttrWrite = 0xFFFFFFFF;

		if ( DirBytes[ ptr + 0x19 ] & 4 )
			file.AttrLocked	= 0xFFFFFFFF;

		if ( DirBytes[ ptr + 0x19 ] & 8 )
			file.Flags |= FF_Directory;
		
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
		file.XlatorID = NULL;
		file.HasResolvedIcon = false;

		TranslateType( &file );

		file.EncodingID = ENCODING_ACORN;
		file.FSFileType = FT_ACORNX;

		Files.push_back(file);

		ptr	+= 26;

		FileID++;
	}

	if ( ReadSize >= 0x7DE )
	{
		ParentSector = * (DWORD *) &DirBytes[ 0x7DA ]; ParentSector &= 0xFFFFFF;
	}
	else
	{
		ParentSector = 0x000203; // In theory at least
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

	/* Here A and B are completely different to each other. So use bbc_strcmp, which works like strnicmp */
	if ( bbc_strcmp( a.Filename, b.Filename ) > 0 )
	{
		return false;
	}

	return true;
}

int	ADFSEDirectory::WriteDirectory( void )
{
	//	Sort directory entries
	std::sort( Files.begin(), Files.end(), ADFSSort );

	FileFragments Frags = pMap->GetFileFragments( DirSector );

	FileFragment_iter iFrag;

	DWORD TotalSize = 0;

	for ( iFrag = Frags.begin(); iFrag != Frags.end(); iFrag++ )
	{
		TotalSize += iFrag->Length;
	}

	while ( TotalSize % 1024 )
	{
		TotalSize += ( 1024 - ( TotalSize % 1024 ) );
	}

	if ( TotalSize < 0x800 )
	{
		return NUTSError( 0x3A, L"Directory size too small for directory structure" );
	}

	BYTE DirBytes[ 0x800 ];

	ZeroMemory( DirBytes, 0x800 );

	MasterSeq = ( ( MasterSeq / 16 ) * 10 ) + ( MasterSeq % 16 );
	MasterSeq++;
	MasterSeq = ( ( MasterSeq / 10 ) * 16 ) + ( MasterSeq % 10 );

	DirBytes[ 0x000 ] = MasterSeq;
	DirBytes[ 0x7FA ] = MasterSeq;

	memcpy( &DirBytes[0x7DD], DirTitle, 18 );
	memcpy( &DirBytes[0x7F0], DirName,  10 );

	DirBytes[ 0x7FB ] = 'N'; DirBytes[ 0x001 ] = 'N';
	DirBytes[ 0x7FC ] = 'i'; DirBytes[ 0x002 ] = 'i';
	DirBytes[ 0x7FD ] = 'c'; DirBytes[ 0x003 ] = 'c';
	DirBytes[ 0x7FE ] = 'k'; DirBytes[ 0x004 ] = 'k';

	WORD ptr;

	ptr	= 5;

	NativeFileIterator iFile = Files.begin();

	while ( ( ptr < 0x7D6 ) && ( iFile != Files.end() ) ) {
		BBCStringCopy( (char *) &DirBytes[ ptr ], (char *) iFile->Filename, 10 );

		DirBytes[ ptr + 0x19 ] = 0;

		/* Do this line first, as the indirect address is actually only 3 bytes, and the
		   effective MSByte of the DWORD is the attribute byte */
		* (DWORD *) &DirBytes[ ptr + 0x16 ] = iFile->SSector;

		if ( iFile->AttrRead != 0x00000000 )
		{
			DirBytes[ ptr + 0x19 ] |= 1;
			DirBytes[ ptr + 0x19 ] |= 16;
		}

		if ( iFile->AttrWrite != 0x00000000 )
		{
			DirBytes[ ptr + 0x19 ] |= 2;
			DirBytes[ ptr + 0x19 ] |= 32;
		}

		if ( iFile->AttrLocked != 0x00000000 )
		{
			DirBytes[ ptr + 0x19 ] |= 4;
		}

		if ( iFile->Flags & FF_Directory )
		{
			DirBytes[ ptr + 0x19 ] |= 8;
		}

		* (DWORD *) &DirBytes[ ptr + 0x0A ] = iFile->LoadAddr;
		* (DWORD *) &DirBytes[ ptr + 0x0E ] = iFile->ExecAddr;
		* (DWORD *) &DirBytes[ ptr + 0x12 ] = iFile->Length;
	
		ptr	+= 0x1A;

		iFile++;
	}

	DirBytes[ ptr ] = 0; /* End of files marker */

	DirBytes[ 0x7DA ] =   ParentSector & 0xFF;
	DirBytes[ 0x7DB ] = ( ParentSector & 0xFF00   ) >> 8;
	DirBytes[ 0x7DC ] = ( ParentSector & 0xFF0000 ) >> 16;

	DirBytes[ 0x7D8 ] = 0;
	DirBytes[ 0x7D9 ] = 0; /* Reserved - must be zero */

	DirBytes[ 0x7D7 ] = 0; /* Absolute end of directory marker */

	BBCStringCopy( (char *) &DirBytes[0x7F0], (char *) DirName,  10 );
	BBCStringCopy( (char *) &DirBytes[0x7DD], (char *) DirTitle, 19 );

	/* Calculate check byte */
	DWORD Accumulator = 0;

	/* Directory words, up to, but not including the end-of-files marker */
	for ( WORD i=0; i < (ptr & 0xFFF8); i+=4 )
	{
		DWORD v = * (DWORD *) &DirBytes[ i ] ;

		Accumulator = v ^ ( ( Accumulator >> 13 ) | ( Accumulator << 19 ) );
	}

	/* Trailing directory bytes */
	for ( WORD i=(ptr & 0xFFF8); i < ptr; i++ )
	{
		DWORD v = DirBytes[ i ] ;

		Accumulator = v ^ ( ( Accumulator >> 13 ) | ( Accumulator << 19 ) );
	}

	/* Directory tail words */
	for ( WORD i=0x7D8; i < 0x7FC; i+=4 )
	{
		DWORD v = * (DWORD *) &DirBytes[ i ] ;

		Accumulator = v ^ ( ( Accumulator >> 13 ) | ( Accumulator << 19 ) );
	}

	/* Trailing directory tail bytes, not including check byte itself */

	/* Only for big dirs!
	for ( WORD i=0x7FC; i < 0x7FF; i++ )
	{
		DWORD v = DirBytes[ i ] ;

		Accumulator = v ^ ( ( Accumulator >> 13 ) | ( Accumulator << 19 ) );
	}
	*/

	DirBytes[ 0x7FF ] = ( BYTE) ( ( Accumulator & 0xFF ) ^ ( ( Accumulator & 0xFF00 ) >> 8 ) ^ ( ( Accumulator & 0xFF0000 ) >> 16 ) ^ ( ( Accumulator &0xFF000000 ) >> 24 ) );

	DWORD WriteSize = 0;

	for ( iFrag = Frags.begin(); iFrag != Frags.end(); iFrag++ )
	{
		DWORD Sectors = iFrag->Length / pMap->SecSize;

		if ( iFrag->Length % pMap->SecSize ) { Sectors++; }

		for ( DWORD sec = 0; sec<Sectors; sec++ )
		{
			pSource->WriteSector( iFrag->Sector + sec, &DirBytes[ WriteSize ], pMap->SecSize );

			WriteSize += pMap->SecSize;

			if ( WriteSize >= 0x800 )
			{
				break;
			}
		}
	}

	return 0;
}

void ADFSEDirectory::TranslateType( NativeFile *file )
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
	QWORD UnixTime = ( ( (QWORD) file->LoadAddr & 0xFF) << 32 ) | file->ExecAddr;

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

