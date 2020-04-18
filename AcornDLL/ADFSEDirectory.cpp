#include "StdAfx.h"
#include "ADFSEDirectory.h"
#include "RISCOSIcons.h"
#include "Defs.h"

int	ADFSEDirectory::ReadDirectory( void ) {

	FileFragments  Frags = pMap->GetFileFragments( DirSector );

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

	BYTE *DirBytes = (BYTE *) malloc( TotalSize );

	DWORD ReadSize = 0;

	for ( iFrag = Frags.begin(); iFrag != Frags.end(); iFrag++ )
	{
		DWORD Sectors = iFrag->Length / 1024;

		if ( iFrag->Length % 1024 ) { Sectors++; }

		for ( DWORD sec = 0; sec<Sectors; sec++ )
		{
			pSource->ReadSector( iFrag->Sector + sec, &DirBytes[ ReadSize ], 1024 );

			ReadSize += 1024;
		}
	}

	Files.clear();
	ResolvedIcons.clear();

	int	ptr,c;

	ptr	= 5;

	DWORD FileID = 0;

	NativeFile file;

//	MasterSeq = DirBytes[0];

//	memcpy( DirString, &DirBytes[0x4cc], 10);
//	memcpy( DirTitle,  &DirBytes[0x4d9], 18);

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
		file.SeqNum   = DirBytes[ptr + 25];
		file.XlatorID = NULL;
		file.HasResolvedIcon = false;

		TranslateType( &file );

		file.EncodingID = ENCODING_ACORN;
		file.FSFileType = FT_ACORN;

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

int	ADFSEDirectory::WriteDirectory(void)
{

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

