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
		else
		{
			size_t dp = Filename.find_last_of( L"." ) ;

			if ( dp != std::wstring::npos )
			{
				if ( dp == ( Filename.length() - 4 ) )
				{
					file.Flags |= FF_Extension;
					
					std::wstring extn = Filename.substr( dp + 1 );

					strncpy_s( (char *) file.Extension, 4, AString( (WCHAR *) extn.c_str() ), 3 );

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
		}

		strncpy_s( (char *) file.Filename, 33, AString( (WCHAR *) Filename.c_str() ), 32 ); 

		file.Length	    = fdata.nFileSizeLow;
		file.EncodingID = ENCODING_ASCII;
		file.XlatorID   = NULL;
		file.HasResolvedIcon = false;

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
