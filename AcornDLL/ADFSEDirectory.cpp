#include "StdAfx.h"
#include "ADFSEDirectory.h"
#include "RISCOSIcons.h"
#include "BBCFunctions.h"
#include "../NUTS/NUTSError.h"
#include "../NUTS/libfuncs.h"
#include "Defs.h"
#include "AcornDLL.h"
#include <algorithm>

int	ADFSEDirectory::ReadDirectory( void ) {
	if ( pMap->FormatVersion == 1 )
	{
		return ReadPDirectory();
	}

	return ReadEDirectory();
}

int	ADFSEDirectory::ReadEDirectory( void )
{
	Files.clear();
	ResolvedIcons.clear();

	FileFragments Frags = pMap->GetFileFragments( DirSector );

	if ( Frags.size() == 0 )
	{
		return NUTSError( 0x90, L"Directory has no fragments. Disk is corrupt." );
	}

	FileFragment_iter iFrag;

	DWORD TotalSize = 0;

	BYTE DirBytes[ 0x800 ];

	DWORD ReadSize = 0;

	for ( iFrag = Frags.begin(); iFrag != Frags.end(); )
	{
		DWORD Sectors = iFrag->Length / pMap->SecSize;

		if ( iFrag->Length % pMap->SecSize ) { Sectors++; }

		for ( DWORD sec = 0; sec<Sectors; sec++ )
		{
			if ( ReadTranslatedSector( iFrag->Sector + sec, &DirBytes[ ReadSize ], pMap->SecSize, pSource ) != DS_SUCCESS )
			{
				return -1;
			}

			ReadSize += pMap->SecSize;

			if ( ReadSize >= 0x800 )
			{
				break;
			}
		}

		if ( ReadSize >= 0x800 )
		{
			iFrag = Frags.end();
		}
	}

	int	ptr,c;

	ptr	= 5;

	DWORD FileID = 0;

	NativeFile file;

	MasterSeq = DirBytes[0];

	memcpy( DirTitle, &DirBytes[0x7DD], 18);
	memcpy( DirName,  &DirBytes[0x7F0], 10);

	while ( ptr < 2007 ) {
		for ( c=0; c<16; c++) { file.Attributes[ c ] = 0; }

		file.Flags    = 0;

		file.Filename = BYTEString( &DirBytes[ptr], 10 );

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

int	ADFSEDirectory::ReadPDirectory( void ) {

	Files.clear();
	ResolvedIcons.clear();

	FileFragments Frags = pMap->GetFileFragments( DirSector );

	if ( Frags.size() == 0 )
	{
		return NUTSError( 0x90, L"Directory has no fragments. Disk is corrupt." );
	}

	FileFragment_iter iFrag;

	DWORD TotalSize = 0;

	/* Big directories can grow, so we need to allocate the memory for this (hopefully it won't be toooo big... ) */
	DWORD DirSize = 0;

	for ( iFrag = Frags.begin(); iFrag != Frags.end(); iFrag++ )
	{
		DirSize += iFrag->Length;
	}

	BYTE *DirBytes = (BYTE *) malloc( DirSize );

	DWORD ReadSize = 0;

	for ( iFrag = Frags.begin(); iFrag != Frags.end(); )
	{
		DWORD Sectors = iFrag->Length / pMap->SecSize;

		if ( iFrag->Length % pMap->SecSize ) { Sectors++; }

		for ( DWORD sec = 0; sec<Sectors; sec++ )
		{
			if ( ReadTranslatedSector( iFrag->Sector + sec, &DirBytes[ ReadSize ], pMap->SecSize, pSource ) != DS_SUCCESS )
			{
				return -1;
			}

			ReadSize += pMap->SecSize;

			if ( ReadSize >= pMap->RootSize )
			{
				break;
			}
		}

		if ( ReadSize >= pMap->RootSize )
		{
			iFrag = Frags.end();
		}
		else
		{
			iFrag++;
		}
	}

	Files.clear();
	ResolvedIcons.clear();

	DWORD FileID = 0;

	NativeFile file;

	MasterSeq = DirBytes[0];

	DWORD Entries  = * (DWORD *) &DirBytes[ 0x10 ];
	DWORD NameSize = * (DWORD *) &DirBytes[ 0x14 ];
	DWORD DNameLen = * (DWORD *) &DirBytes[ 0x08 ];

	ParentSector   = * (DWORD *) &DirBytes[ 0x18 ];

	DWORD ptr = 0;

	if ( BigDirName != nullptr )
	{
		free( BigDirName );
	}

	BigDirName = rstrndup( &DirBytes[ 0x1C ], (WORD) DNameLen );

	ptr = 0x1C + DNameLen + 1;

	/* Everything must align on 4 byte boundaries */
	while ( ptr % 4 ) { ptr++; }

	/* Calculate start of name heap - it follows after the entries, which are helpfully an exact multiple of 4 */
	DWORD NameHeapPtr = ptr + ( Entries * 0x1C );

	for ( DWORD Entry=0; Entry<Entries; Entry++ )
	{
		for ( DWORD c=0; c<16; c++) { file.Attributes[ c ] = 0; }

		file.Flags    = 0;

		DWORD ObNameLen = * (DWORD *) &DirBytes[ ptr + 0x14 ];
		DWORD ObNamePtr = * (DWORD *) &DirBytes[ ptr + 0x18 ];

		/* Note: The disk format terminates these with 0x0D, but does not include said byte in the length field */
		file.Filename = BYTEString( &DirBytes[ NameHeapPtr + ObNamePtr ], (WORD) ObNameLen );

		DWORD BigAttr = * (DWORD *) &DirBytes[ ptr + 0x10 ];

		if ( BigAttr & 1 )
			file.AttrRead = 0xFFFFFFFF;

		if ( BigAttr & 2 )
			file.AttrWrite = 0xFFFFFFFF;

		if ( BigAttr & 4 )
			file.AttrLocked	= 0xFFFFFFFF;

		if ( BigAttr & 8 )
			file.Flags |= FF_Directory;
		
		file.fileID   = FileID;
		file.LoadAddr = * (DWORD *) &DirBytes[ ptr + 0x00 ];
		file.ExecAddr = * (DWORD *) &DirBytes[ ptr + 0x04 ];
		file.Length   = * (DWORD *) &DirBytes[ ptr + 0x08 ];
		file.SSector  = * (DWORD *) &DirBytes[ ptr + 0x0C ];
		file.XlatorID = NULL;
		file.HasResolvedIcon = false;

		TranslateType( &file );

		file.EncodingID = ENCODING_ACORN;
		file.FSFileType = FT_ACORNX;

		Files.push_back(file);

		ptr	+= 0x1C;

		FileID++;
	}

	free( DirBytes );

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
	if ( pMap->FormatVersion == 1 )
	{
		return WritePDirectory();
	}

	return WriteEDirectory();
}

int	ADFSEDirectory::WriteEDirectory( void )
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

	while ( TotalSize % pMap->SecSize )
	{
		TotalSize += ( pMap->SecSize - ( TotalSize % pMap->SecSize ) );
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
		BBCStringCopy( &DirBytes[ ptr ], iFile->Filename, 10 );

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
		* (DWORD *) &DirBytes[ ptr + 0x12 ] = (DWORD) iFile->Length;
	
		ptr	+= 0x1A;

		iFile++;
	}

	DirBytes[ ptr ] = 0; /* End of files marker */

	DirBytes[ 0x7DA ] = (BYTE) (   ParentSector & 0xFF );
	DirBytes[ 0x7DB ] = (BYTE) ( ( ParentSector & 0xFF00   ) >> 8 );
	DirBytes[ 0x7DC ] = (BYTE) ( ( ParentSector & 0xFF0000 ) >> 16 );

	DirBytes[ 0x7D8 ] = 0;
	DirBytes[ 0x7D9 ] = 0; /* Reserved - must be zero */

	DirBytes[ 0x7D7 ] = 0; /* Absolute end of directory marker */

	BBCStringCopy( &DirBytes[0x7F0], DirName,  10 );
	BBCStringCopy( &DirBytes[0x7DD], DirTitle, 19 );

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

	DirBytes[ 0x7FF ] = ( BYTE) ( ( Accumulator & 0xFF ) ^ ( ( Accumulator & 0xFF00 ) >> 8 ) ^ ( ( Accumulator & 0xFF0000 ) >> 16 ) ^ ( ( Accumulator &0xFF000000 ) >> 24 ) );

	DWORD WriteSize = 0;

	for ( iFrag = Frags.begin(); iFrag != Frags.end(); iFrag++ )
	{
		DWORD Sectors = iFrag->Length / pMap->SecSize;

		if ( iFrag->Length % pMap->SecSize ) { Sectors++; }

		for ( DWORD sec = 0; sec<Sectors; sec++ )
		{
			if ( WriteTranslatedSector( iFrag->Sector + sec, &DirBytes[ WriteSize ], pMap->SecSize, pSource ) != DS_SUCCESS )
			{
				return -1;
			}

			WriteSize += pMap->SecSize;

			if ( WriteSize >= 0x800 )
			{
				break;
			}
		}
	}

	return 0;
}

int	ADFSEDirectory::WritePDirectory( void )
{
	//	Sort directory entries
	std::sort( Files.begin(), Files.end(), ADFSSort );

	FileFragments Frags = pMap->GetFileFragments( DirSector );

	FileFragment_iter iFrag;

	/* Calculate the proposed directory size, and see if we have enough space to accomodate it. If not,
	   ask the map for an extension. */
	DWORD TotalSize = 0;

	for ( iFrag = Frags.begin(); iFrag != Frags.end(); iFrag++ )
	{
		TotalSize += iFrag->Length;
	}

	while ( TotalSize % pMap->SecSize )
	{
		TotalSize += ( pMap->SecSize - ( TotalSize % pMap->SecSize ) );
	}

	DWORD RequiredDirSize = 0x1C; // Fixed portion of header

	RequiredDirSize += rstrnlen( BigDirName, 255 ) + 1;

	while ( RequiredDirSize % 4 ) { RequiredDirSize++; }

	/* This will be the start of the file entries. */
	DWORD FileTablePtr = RequiredDirSize;

	/* This will be the start of the name heap */
	DWORD NameTablePtr = FileTablePtr + ( (DWORD) Files.size() * 0x1C );
	DWORD NameTableSize = 0;
	DWORD NameTableStart = NameTablePtr;

	NativeFileIterator iFile;

	for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		RequiredDirSize += 0x1C; // Fixed size part

		DWORD NameLen = rstrnlen( iFile->Filename, 255 ) + 1;

		RequiredDirSize += NameLen;

		while ( RequiredDirSize % 4 ) { RequiredDirSize++; }

		NameTableSize += NameLen;

		while ( NameTableSize % 4 ) { NameTableSize++; }
	}

	/* Add on the tail, and confirm the size */
	RequiredDirSize += 0x08;

	if ( TotalSize < RequiredDirSize )
	{
		/* we need more! */
		DWORD RequiredExtra = RequiredDirSize - TotalSize;

		DWORD RequiredSecs  = RequiredExtra % pMap->SecSize;

		if ( RequiredExtra % pMap->SecSize ) { RequiredSecs++; }

		/* This will allocate the space, but we want the whole set, so discard the result, except for disc full checking */
		TargetedFileFragments NewFrags = pMap->GetWriteFileFragments( RequiredSecs, DirSector >> 8, true );

		if ( NewFrags.Frags.size() == 0 )
		{
			return NUTSError( 0x3B, L"Disc full" );
		}

		Frags = pMap->GetFileFragments( DirSector );

		TotalSize = RequiredDirSize;
	}

	BYTE *DirBytes = (BYTE *) malloc( TotalSize );

	BYTE *Tail = &DirBytes[ TotalSize - 0x08 ];

	ZeroMemory( DirBytes, TotalSize );

	MasterSeq = ( ( MasterSeq / 16 ) * 10 ) + ( MasterSeq % 16 );
	MasterSeq++;
	MasterSeq = ( ( MasterSeq / 10 ) * 16 ) + ( MasterSeq % 10 );

	DirBytes[ 0x000 ] = MasterSeq;
	Tail[      0x04 ] = MasterSeq;

	BBCStringCopy( &DirBytes[ 0x1C ], BigDirName, 255 );

	DirBytes[ 0x04 ] = 'S'; Tail[ 0x000 ] = 'o';
	DirBytes[ 0x05 ] = 'B'; Tail[ 0x001 ] = 'v';
	DirBytes[ 0x06 ] = 'P'; Tail[ 0x002 ] = 'e';
	DirBytes[ 0x07 ] = 'r'; Tail[ 0x003 ] = 'n';

	for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		BBCStringCopy( &DirBytes[ NameTablePtr ], iFile->Filename, 255 );

		DWORD Attr = 0;

		/* Do this line first, as the indirect address is actually only 3 bytes, and the
		   effective MSByte of the DWORD is the attribute byte */
		* (DWORD *) &DirBytes[ FileTablePtr + 0x0C ] = iFile->SSector;

		if ( iFile->AttrRead != 0x00000000 )
		{
			Attr |= 1;
			Attr |= 16;
		}

		if ( iFile->AttrWrite != 0x00000000 )
		{
			Attr |= 2;
			Attr |= 32;
		}

		if ( iFile->AttrLocked != 0x00000000 )
		{
			Attr |= 4;
		}

		if ( iFile->Flags & FF_Directory )
		{
			Attr |= 8;
		}

		DWORD NameLen = rstrnlen( iFile->Filename, 255 );

		* (DWORD *) &DirBytes[ FileTablePtr + 0x00 ] = iFile->LoadAddr;
		* (DWORD *) &DirBytes[ FileTablePtr + 0x04 ] = iFile->ExecAddr;
		* (DWORD *) &DirBytes[ FileTablePtr + 0x08 ] = (DWORD) iFile->Length;
		* (DWORD *) &DirBytes[ FileTablePtr + 0x10 ] = Attr;
		* (DWORD *) &DirBytes[ FileTablePtr + 0x14 ] = NameLen;
		* (DWORD *) &DirBytes[ FileTablePtr + 0x18 ] = NameTablePtr - NameTableStart;
	
		FileTablePtr += 0x1C;
		NameTablePtr += NameLen + 1;

		while ( NameTablePtr % 4 ) { NameTablePtr++; }
	}

	* (DWORD *) &DirBytes[ 0x18 ] = ParentSector;
	* (DWORD *) &DirBytes[ 0x14 ] = NameTableSize;
	* (DWORD *) &DirBytes[ 0x10 ] = (DWORD) Files.size();
	* (DWORD *) &DirBytes[ 0x0C ] = TotalSize;
	* (DWORD *) &DirBytes[ 0x08 ] = rstrnlen( BigDirName, 255 );
	
	DirBytes[ 0x01 ] = 0;
	DirBytes[ 0x02 ] = 0;
	DirBytes[ 0x03 ] = 0;
	
	Tail[ 0x05 ] = 0;
	Tail[ 0x06 ] = 0;

	/* Calculate check byte */
	DWORD Accumulator = 0;

	/* Directory words, up to, but not including the end-of-files marker */
	for ( DWORD i=0; i < (TotalSize - 4); i+=4 )
	{
		DWORD v = * (DWORD *) &DirBytes[ i ] ;

		Accumulator = v ^ ( ( Accumulator >> 13 ) | ( Accumulator << 19 ) );
	}

	/* Trailing directory tail bytes, not including check byte itself */
	for ( DWORD i=TotalSize - 4; i < (TotalSize - 1); i++ )
	{
		DWORD v = DirBytes[ i ] ;

		Accumulator = v ^ ( ( Accumulator >> 13 ) | ( Accumulator << 19 ) );
	}

	Tail[ 0x07 ] = ( BYTE) ( ( Accumulator & 0xFF ) ^ ( ( Accumulator & 0xFF00 ) >> 8 ) ^ ( ( Accumulator & 0xFF0000 ) >> 16 ) ^ ( ( Accumulator &0xFF000000 ) >> 24 ) );

	DWORD WriteSize = 0;

	for ( iFrag = Frags.begin(); iFrag != Frags.end(); iFrag++ )
	{
		DWORD Sectors = iFrag->Length / pMap->SecSize;

		if ( iFrag->Length % pMap->SecSize ) { Sectors++; }

		for ( DWORD sec = 0; sec<Sectors; sec++ )
		{
			if ( WriteTranslatedSector( iFrag->Sector + sec, &DirBytes[ WriteSize ], pMap->SecSize, pSource ) != DS_SUCCESS )
			{
				return -1;
			}

			WriteSize += pMap->SecSize;

			if ( WriteSize >= TotalSize )
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

