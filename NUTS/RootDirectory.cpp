#include "StdAfx.h"
#include "RootDirectory.h"
#include "libfuncs.h"

int	RootDirectory::ReadDirectory(void) {
	TCHAR dirString[4096];

	GetLogicalDriveStrings(4096, dirString);

	TCHAR dirEnt[17];
	TCHAR *p = dirString;
	int	  i;
	UINT  DType;
	DWORD FileID = 0;

	Files.clear();
	ResolvedIcons.clear();

	while (1) {
		if (!*p)
			break;

		i=0;

		while (*p) {
			dirEnt[i]	= *p;

			p++;
			i++;
		}

		dirEnt[i]	= *p;
		p++;

		NativeFile	file;

		strncpy_s( (char *) file.Filename, 32, AString( dirEnt ) , 17 );

		file.fileID     = FileID++;
		file.Flags      = 0U;
		file.FSFileType = FT_ROOT;
		file.EncodingID = ENCODING_ASCII;
		file.XlatorID   = NULL;
		file.HasResolvedIcon = false;

		DType	= GetDriveType(dirEnt);

		file.Icon = FT_HardDisc;

		if (DType == DRIVE_REMOVABLE)
			file.Icon = FT_Floppy;

		if (DType == DRIVE_CDROM)
			file.Icon = FT_CDROM;

		file.Type = FT_MiscImage;

		Files.push_back(file);
	}

	return 0;
}

int	RootDirectory::WriteDirectory(void) {
	return 0;
}

