#include "StdAfx.h"
#include "RootDirectory.h"
#include "libfuncs.h"
#include "Plugins.h"

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

	NativeFileIterator iFile;

	for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
	{
		if ( iFile->HasResolvedIcon )
		{
			free( ResolvedIcons[ iFile->fileID ].pImage );
		}
	}

	TCHAR dirString[4096];

	GetLogicalDriveStrings(4096, dirString);

	TCHAR dirEnt[17];
	TCHAR *p = dirString;
	int	  i;
	UINT  DType;
	DWORD FileID = 0;

	Files.clear();
	ResolvedIcons.clear();
	HookPairs.clear();

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

		file.Filename = BYTEString( (BYTE*) AString( dirEnt ) , 17 );

		file.fileID     = FileID++;
		file.Flags      = FF_NotRenameable;
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
			file.Flags           = FF_NotRenameable;
			file.FSFileType      = NULL;
			file.HasResolvedIcon = false;
			file.Icon            = FT_Directory;
			file.Type            = FT_MiscImage;
			file.Length          = 0;
			file.XlatorID        = NULL;

			file.Filename = BYTEString( (BYTE *) AString( (WCHAR *) Folders[ fIndex ].FolderName.c_str() ), 32 );

			Files.push_back( file );

			FileID++;
		}

		fIndex++;
	}

	/* Add the ROMDisk */
	{
		NativeFile file;

		file.EncodingID      = ENCODING_ASCII;
		file.fileID          = FileID;
		file.Flags           = FF_NotRenameable;
		file.FSFileType      = NULL;
		file.HasResolvedIcon = false;
		file.Icon            = FT_ROMDisk;
		file.Type            = FT_MiscImage;
		file.Length          = 0;
		file.XlatorID        = NULL;

		file.Filename = BYTEString( (BYTE *) "ROM Disk", 8 );

		Files.push_back( file );

		FileID++;
	}

	/* Add root hooks */
	RootHookList hooks = FSPlugins.GetRootHooks();

	RootHookIterator iHook;

	for ( iHook = hooks.begin(); iHook != hooks.end(); iHook++ )
	{
		NativeFile file;

		file.Attributes[ 0 ] = fIndex;
		file.EncodingID      = ENCODING_ASCII;
		file.fileID          = FileID;
		file.Flags           = FF_NotRenameable | FF_Pseudo;
		file.FSFileType      = NULL;
		file.HasResolvedIcon = false;
		file.Icon            = FT_Arbitrary;
		file.Type            = FT_MiscImage;
		file.Length          = 0;
		file.XlatorID        = NULL;

		file.Filename = BYTEString( (BYTE *) AString( (WCHAR *) iHook->FriendlyName.c_str() ) );

		TranslateIconToResolved( &file, iHook->HookIcon );

		HookPairs[ FileID ] = *iHook;

		Files.push_back( file );

		FileID++;
	}

	return 0;
}

int	RootDirectory::WriteDirectory(void) {
	return 0;
}


void RootDirectory::TranslateIconToResolved( NativeFile *pFile, HBITMAP hBitmap )
{
	BITMAP bmp;

	if ( !GetObject( hBitmap, sizeof(BITMAP), (LPSTR) &bmp ) )
	{
		return;
	}

	IconDef icon;

	/* If the bit depth is less than 24, give up on it */
	if ( bmp.bmBitsPixel < 24 )
	{
		return;
	}

	icon.bmi.biBitCount     = bmp.bmBitsPixel;
	icon.bmi.biClrImportant = 0;
	icon.bmi.biClrUsed      = 0;
	icon.bmi.biCompression  = BI_RGB;
	icon.bmi.biHeight       = bmp.bmHeight;
	icon.bmi.biPlanes       = bmp.bmPlanes;
	icon.bmi.biSize         = sizeof(BITMAPINFOHEADER);
	icon.bmi.biWidth        = bmp.bmWidth;
	icon.bmi.biSizeImage    = bmp.bmHeight * bmp.bmWidthBytes;

	icon.pImage = malloc( bmp.bmHeight * bmp.bmWidthBytes );

	if ( bmp.bmBits != nullptr )
	{
		memcpy( icon.pImage, bmp.bmBits, bmp.bmHeight * bmp.bmWidthBytes );
	}
	else
	{
		// Windows. Sigh.
		HDC hDC = GetDC( 0 );

		// WINDOWS. SIGH I SAY.
		BITMAPINFO bmi;

		bmi.bmiHeader = icon.bmi;

		GetDIBits( hDC, hBitmap, 0, bmp.bmHeight, icon.pImage, &bmi, DIB_RGB_COLORS );

		ReleaseDC( 0, hDC );
	}

	// This makes the icon look square.
	icon.Aspect = AspectRatio( 34, 28 );

	ResolvedIcons[ pFile->fileID ] = icon;

	pFile->HasResolvedIcon = true;
}