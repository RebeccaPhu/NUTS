#include "StdAfx.h"
#include "AcornDFSDirectory.h"

#include "Defs.h"

int	AcornDFSDirectory::ReadDirectory(void) {
	Files.clear();

	BYTE	SectorData[512];

	pSource->ReadSector( 0, &SectorData[000], 256);
	pSource->ReadSector( 1, &SectorData[256], 256);

	int	offset	= 8;

	memcpy(&DiscTitle[0], &SectorData[0], 8);
	memcpy(&DiscTitle[8], &SectorData[256 + 0], 4);
	
	MasterSeq	= SectorData[256 + 4];

	NumSectors	= SectorData[256 + 7] | ((SectorData[256 + 6] & 0x3) << 8);
	Option		= (SectorData[256 + 6] & 0x30) >> 4;

	NativeFile	file;
	DWORD       FileID = 0;

	std::map<BYTE,bool> Dirs;

	while (offset < 256) {
		if (offset >= SectorData[256 + 5] + 8)
			break;
	
		file.fileID = FileID;
		file.Flags  = 0;
		
		memset(file.Filename, 0, 16);

		memcpy(&file.Filename[0], &SectorData[offset], 7);

		BYTE ThisDir = SectorData[offset + 7] & 0x7f;
		
		if ( ( Dirs.find( ThisDir ) == Dirs.end() ) && ( ThisDir != CurrentDir ) )
		{
			if ( CurrentDir == '$' )
			{
				NativeFile FakeSubDir;

				FakeSubDir.EncodingID      = ENCODING_ACORN;
				FakeSubDir.fileID          = FileID;
				FakeSubDir.Filename[0]     = ThisDir;
				FakeSubDir.Filename[1]     = 0;
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

		Dirs[ ThisDir ] = true;

		file.AttrRead	= 0x00000000;
		file.AttrWrite	= 0x00000000;
		file.AttrLocked	= 0x00000000;

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

		file.EncodingID = ENCODING_ACORN;
		file.FSFileType = FT_ACORN;
		file.Icon       = file.Type;

		if ( ThisDir == CurrentDir )
		{
			Files.push_back(file);
			FileID++;
		}

		offset	+= 8;
	}

	return 0;
}

int	AcornDFSDirectory::WriteDirectory(void) {

	return 0;
}

