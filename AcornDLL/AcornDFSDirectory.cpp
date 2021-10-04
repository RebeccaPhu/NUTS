#include "StdAfx.h"
#include "AcornDFSDirectory.h"
#include "../NUTS/libfuncs.h"
#include "AcornDLL.h"
#include "Defs.h"

int	AcornDFSDirectory::ReadDirectory(void) {
	Files.clear();
	RealFiles.clear();

	BYTE	SectorData[512];

	pSource->ReadSectorCHS( 0, 0, 0, &SectorData[000] );
	pSource->ReadSectorCHS( 0, 0, 1, &SectorData[256] );

	int	offset	= 8;

	memcpy(&DiscTitle[0], &SectorData[0], 8);
	memcpy(&DiscTitle[8], &SectorData[256 + 0], 4);
	
	MasterSeq	= SectorData[256 + 4];

	NumSectors	= SectorData[256 + 7] | ((SectorData[256 + 6] & 0x3) << 8);
	Option		= (SectorData[256 + 6] & 0x30) >> 4;
	MasterSeq   = SectorData[ 256 + 4 ];

	NativeFile	file;
	DWORD       FileID = 0;

	std::map<BYTE,bool> Dirs;

	while (offset < 256) {
		if (offset >= SectorData[256 + 5] + 8)
			break;
	
		file.fileID = FileID;
		file.Flags  = 0;
		
		file.Filename = BYTEString( 16 );

		memcpy( (BYTE *) &file.Filename[0], &SectorData[offset], 7);

		BYTE ThisDir = SectorData[offset + 7] & 0x7f;
		
		if ( ( Dirs.find( ThisDir ) == Dirs.end() ) && ( ThisDir != CurrentDir ) )
		{
			if ( CurrentDir == '$' )
			{
				NativeFile FakeSubDir;

				FakeSubDir.EncodingID      = ENCODING_ACORN;
				FakeSubDir.fileID          = FileID;
				FakeSubDir.Filename        = BYTEString( &ThisDir, 1 );
				FakeSubDir.FSFileType      = FT_ACORN;
				FakeSubDir.HasResolvedIcon = false;
				FakeSubDir.XlatorID        = 0;
				FakeSubDir.Flags           = FF_Directory;
				FakeSubDir.Icon            = FT_Directory;
				FakeSubDir.Type            = FT_Directory;

				Files.push_back( FakeSubDir );

				FileID++;

				file.fileID = FileID;
			}
		}

		if ( ThisDir != '$' )
		{
			ExtraDirectories[ ThisDir ] = true;
		}

		Dirs[ ThisDir ] = true;

		file.AttrPrefix = ThisDir;
		file.AttrWrite  = 0x00000000;
		file.AttrLocked = 0x00000000;

		if (SectorData[offset+7] & 0x80)
			file.AttrLocked	= 0xFFFFFFFF;

		bool f = false;

		for (int i=0; i<12; i++) {
			if ( ( file.Filename[i] == 0 ) || ( file.Filename[ i ] == 32 ) || (f) ) {
				f	= true;

				file.Filename[ i ] = 0;
			}
		}

		file.LoadAddr = ((SectorData[offset + 256 + 6] & 0x0C) << 14) | (SectorData[offset+256 + 1] << 8) | SectorData[offset+256 + 0];
		file.ExecAddr = ((SectorData[offset + 256 + 6] & 0xC0) << 10) | (SectorData[offset+256 + 3] << 8) | SectorData[offset+256 + 2];
		file.Length   = ((SectorData[offset + 256 + 6] & 0x30) << 12) | (SectorData[offset+256 + 5] << 8) | SectorData[offset+256 + 4];
		file.SSector  = SectorData[offset + 256 + 7] | ((SectorData[offset+256 + 6] & 0x03) << 8);

		if (file.ExecAddr & 0xFFFF0000)
			file.ExecAddr |= 0xFFFF0000;

		if (file.LoadAddr & 0xFFFF0000)
			file.LoadAddr |= 0xFFFF0000;

		file.Type            = FT_Binary;
		file.XlatorID        = NULL;
		file.HasResolvedIcon = false;

		if (
			((file.ExecAddr & 0xFFFF) == 0x8023) ||
			((file.ExecAddr & 0xFFFF) == 0x802B)
			)
		{
			file.Type = FT_BASIC;
		}

		if (
			((file.ExecAddr & 0xFFFF) == 0xFFFF) &&
			((file.LoadAddr & 0xFFFF) == 0x0000)
			)
		{
			file.Type = FT_Spool;
			file.XlatorID = TUID_TEXT;
		}

		if (
			((file.ExecAddr & 0xFFFF) == 0xFFFF) &&
			((file.LoadAddr & 0xFFFF) == 0xFFFF)
			)
		{
			file.Type = FT_Data;
		}

		if ( ( file.Length == 0x5000 ) && ( (file.LoadAddr & 0xFFFF) == 0x3000 ) )
		{
			file.Type = FT_Graphic; // Mode 0, 1 or 2
		}

		if ( ( file.Length == 0x4000 ) && ( (file.LoadAddr & 0xFFFF) == 0x4000 ) )
		{
			file.Type = FT_Graphic; // Mode 3
		}

		if ( ( file.Length == 0x2800 ) && ( (file.LoadAddr & 0xFFFF) == 0x5800 ) )
		{
			file.Type = FT_Graphic; // Mode 4 or 5
		}

		if ( ( file.Length == 0x6000 ) && ( (file.LoadAddr & 0xFFFF) == 0x2000 ) )
		{
			file.Type = FT_Graphic; // Mode 6
		}

		if ( ( file.Length == 0x400 ) && ( (file.LoadAddr & 0xFFFF) == 0x7C00 ) )
		{
			file.Type = FT_Graphic; // Mode 7
		}

		if ( file.Type == FT_Graphic )
		{
			file.XlatorID = GRAPHIC_ACORN;
		}

		if ( file.Type == FT_BASIC )
		{
			file.XlatorID = BBCBASIC;
		}

		file.EncodingID = ENCODING_ACORN;
		file.FSFileType = FT_ACORN;
		file.Icon       = file.Type;

		if ( ThisDir == CurrentDir )
		{
			Files.push_back(file);
			FileID++;
		}

		RealFiles.push_back( file );

		offset	+= 8;
	}

	std::map<BYTE, bool>::iterator iExtra;

	for ( iExtra = ExtraDirectories.begin(); iExtra != ExtraDirectories.end(); iExtra++ )
	{
		if ( ( CurrentDir == '$' ) && ( Dirs.find( iExtra->first ) == Dirs.end() ) )
		{
			NativeFile FakeSubDir;

			FakeSubDir.EncodingID      = ENCODING_ACORN;
			FakeSubDir.fileID          = FileID;
			FakeSubDir.Filename        = BYTEString( &iExtra->first, 1 );
			FakeSubDir.FSFileType      = FT_ACORN;
			FakeSubDir.HasResolvedIcon = false;
			FakeSubDir.XlatorID        = 0;
			FakeSubDir.Flags           = FF_Directory;
			FakeSubDir.Icon            = FT_Directory;
			FakeSubDir.Type            = FT_Directory;

			Files.push_back( FakeSubDir );

			FileID++;

			file.fileID = FileID;
		}
	}

	return 0;
}

int	AcornDFSDirectory::WriteDirectory(void) {

	BYTE SectorBuf[ 512 ];

	ZeroMemory( SectorBuf, 512 );

	MasterSeq++;

	if ( ( MasterSeq & 0xF ) > 0x9 )
	{
		MasterSeq = ( MasterSeq & 0xF0 ) + 0x10;
	}

	memcpy( &SectorBuf[ 0 ],   &DiscTitle[ 0 ], 8 );
	memcpy( &SectorBuf[ 256 ], &DiscTitle[ 8 ], 4 );

	SectorBuf[ 256 + 4 ] = MasterSeq;
	SectorBuf[ 256 + 7 ] = NumSectors & 0xFF;
	SectorBuf[ 256 + 6 ] = ( ( NumSectors >> 8 ) & 0x3 ) | ( Option << 4 );
	SectorBuf[ 256 + 5 ] = RealFiles.size() * 8;

	NativeFileIterator iFile;
	DWORD iIndex = 1;

	for ( iFile = RealFiles.begin(); iFile != RealFiles.end(); iFile++ )
	{
		/* DFS uses filenames padded with spaces. This is awkward */
		rstrncpy( &SectorBuf[ iIndex * 8 ], iFile->Filename, 7 );

		WORD fn = rstrnlen( iFile->Filename, 7 );

		if ( fn < 7 )
		{
			memset( &SectorBuf[ ( iIndex * 8 ) + fn ], 0x20, 7 - fn );
		}

		SectorBuf[ ( iIndex * 8 ) + 7 ] = iFile->AttrPrefix;

		/* Now the attributes */
		SectorBuf[ ( iIndex * 8 ) + 256 + 0 ] =   iFile->LoadAddr & 0xFF;
		SectorBuf[ ( iIndex * 8 ) + 256 + 1 ] = ( iFile->LoadAddr >> 8 ) & 0xFF;
		SectorBuf[ ( iIndex * 8 ) + 256 + 2 ] =   iFile->ExecAddr & 0xFF;
		SectorBuf[ ( iIndex * 8 ) + 256 + 3 ] = ( iFile->ExecAddr >> 8 ) & 0xFF;
		SectorBuf[ ( iIndex * 8 ) + 256 + 4 ] =   iFile->Length & 0xFF;
		SectorBuf[ ( iIndex * 8 ) + 256 + 5 ] = ( iFile->Length >> 8 ) & 0xFF;

		SectorBuf[ ( iIndex * 8 ) + 256 + 7 ] =   iFile->SSector & 0xFF;

		/* This byte contains the top two bits of everything else */
		SectorBuf[ ( iIndex * 8 ) + 256 + 6 ] =
			( ( iFile->ExecAddr & 0x30000 ) >> 10 ) | ( ( iFile->Length  & 0x30000 ) >> 12 ) |
			( ( iFile->LoadAddr & 0x30000 ) >> 14 ) | ( ( iFile->SSector & 0x300 )   >> 8 );

		iIndex++;
	}

	/* Write the thing now */
	pSource->WriteSectorCHS( 0, 0, 0, &SectorBuf[ 000 ] );
	pSource->WriteSectorCHS( 0, 0, 1, &SectorBuf[ 256 ] );

	return 0;
}

