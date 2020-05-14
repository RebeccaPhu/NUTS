#include "StdAfx.h"
#include "RootDirectory.h"
#include "libfuncs.h"

#include <ShlObj.h>

FolderPair RootDirectory::Folders[] = {
	{ CSIDL_DESKTOP,     L"Desktop" },
	{ CSIDL_MYDOCUMENTS, L"Documents" },
	{ CSIDL_MYMUSIC,     L"Music" },
	{ CSIDL_MYPICTURES,  L"Pictures" },
	{ CSIDL_MYVIDEO,     L"Video" },

	{ 0xFFFFFFFF, L"" }
};

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

	/* Add on special folder paths */
	int fIndex = 0;

	WCHAR path[ MAX_PATH + 1 ];

	while ( Folders[ fIndex ].FolderID != 0xFFFFFFFF )
	{
		if ( SHGetFolderPath( NULL, Folders[ fIndex ].FolderID, NULL, SHGFP_TYPE_CURRENT, path ) == S_OK )
		{
			NativeFile file;

			file.Attributes[ 0 ] = fIndex;
			file.EncodingID      = ENCODING_ASCII;
			file.fileID          = FileID;
			file.Flags           = 0;
			file.FSFileType      = NULL;
			file.HasResolvedIcon = false;
			file.Icon            = FT_Directory;
			file.Type            = FT_MiscImage;
			file.Length          = 0;
			file.XlatorID        = NULL;

			rstrncpy( file.Filename, (BYTE *) AString( (WCHAR *) Folders[ fIndex ].FolderName.c_str() ), 32 );

			Files.push_back( file );

			FileID++;
		}

		fIndex++;
	}

	return 0;
}

int	RootDirectory::WriteDirectory(void) {
	return 0;
}

