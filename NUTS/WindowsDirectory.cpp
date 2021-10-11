#include "StdAfx.h"
#include "WindowsDirectory.h"
#include "libfuncs.h"
#include "ExtensionRegistry.h"

#include <string>

WindowsDirectory::WindowsDirectory( std::wstring path ) : Directory(NULL) {
	folderPath = path;
}

WindowsDirectory::~WindowsDirectory(void) {
}

void WindowsDirectory::TranslateFileType(NativeFile *file) {
	if ( file->Flags & FF_Directory )
	{
		file->Type = FT_Directory;
		file->Icon = FT_Directory;

		return;
	}

	if ( ! ( file->Flags & FF_Extension ) )
	{
		file->Type = FT_Arbitrary;
		file->Icon = FT_Arbitrary;

		return;
	}

	std::wstring ThisExt = std::wstring( (WCHAR *) UString( (char *) file->Extension ) );

	ExtDef Desc = ExtReg.GetTypes( ThisExt );

	file->Type = Desc.Type;
	file->Icon = Desc.Icon;

	if ( file->Type == FT_Text ) { file->XlatorID = TUID_TEXT; }
}

int	WindowsDirectory::ReadDirectory(void) {
	Files.clear();
	WindowsFiles.clear();

	DWORD FileID = 0;

	WIN32_FIND_DATA	fdata;
	HANDLE			hFind;
	
	std::wstring findSpec = folderPath + L"\\*";

	hFind = FindFirstFile(findSpec.c_str(), &fdata);

	if (hFind == INVALID_HANDLE_VALUE)
		return -1;

	BOOL	FilesToGo = TRUE;

	while (FilesToGo) {
		NativeFile	file;

		file.Flags = 0;

		std::wstring Filename( fdata.cFileName );

		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			file.Flags |= FF_Directory;
			file.Icon   = FT_Directory;
		}

		size_t dp = Filename.find_last_of( L"." ) ;

		if ( dp != std::wstring::npos )
		{
			if ( dp == ( Filename.length() - 4 ) )
			{
				file.Flags |= FF_Extension;
					
				std::wstring extn = Filename.substr( dp + 1 );

				file.Extension = BYTEString( (BYTE*) AString( (WCHAR *) extn.c_str() ), 3 );

				Filename = Filename.substr( 0, dp );

				for ( BYTE i=0; i<3; i++ )
				{
					if ( ( file.Extension[ i ] >= 'a' ) && ( file.Extension[i] <= 'z' ) )
					{
						file.Extension[ i ] &= 0xDF;
					}
				}
			}
		}

		file.Filename = BYTEString( (BYTE *) AString( (WCHAR *) Filename.c_str() ) );

		file.Length = fdata.nFileSizeHigh;
		file.Length <<= 32;
		file.Length |= fdata.nFileSizeLow;

		file.EncodingID = ENCODING_ASCII;
		file.XlatorID   = NULL;
		file.HasResolvedIcon = false;

		// Copy some attributes - these are all read only
		file.Attributes[ 0 ] = (fdata.dwFileAttributes & FILE_ATTRIBUTE_READONLY)?0xFFFFFFFF:0x00000000;
		file.Attributes[ 1 ] = (fdata.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)?0xFFFFFFFF:0x00000000;
		file.Attributes[ 2 ] = (fdata.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)?0xFFFFFFFF:0x00000000;
		file.Attributes[ 3 ] = (fdata.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)?0xFFFFFFFF:0x00000000;

		file.Attributes[ 4 ] = FileTimeToUnixTime( &fdata.ftCreationTime );
		file.Attributes[ 5 ] = FileTimeToUnixTime( &fdata.ftLastAccessTime );
		file.Attributes[ 6 ] = FileTimeToUnixTime( &fdata.ftLastWriteTime );

		TranslateFileType(&file);

		if ((fdata.nFileSizeHigh == 0) && (wcscmp(fdata.cFileName,L"..") != 0) && (wcscmp(fdata.cFileName, L".") != 0)) {
			file.fileID = FileID++;

			Files.push_back(file);

			WindowsFiles.push_back( fdata.cFileName );
		}

		FilesToGo	= FindNextFile(hFind, &fdata);
	}

	FindClose(hFind);

	return 0;
}

int	WindowsDirectory::WriteDirectory(void) {

	return 0;
}

#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600

DWORD WindowsDirectory::FileTimeToUnixTime( FILETIME *ft )
{
	QWORD RTicks = ft->dwHighDateTime;

	RTicks <<= 32;

	RTicks |= ft->dwLowDateTime;

	return (DWORD) ( ( RTicks / WINDOWS_TICK ) - SEC_TO_UNIX_EPOCH );
}