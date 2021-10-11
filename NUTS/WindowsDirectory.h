#pragma once
#include "directory.h"

#include <string>

class WindowsDirectory :
	public Directory
{
public:
	WindowsDirectory(std::wstring path);

	~WindowsDirectory(void);

public:
	int	ReadDirectory(void);
	int	WriteDirectory(void);

private:
	typedef struct _FileTypeEntry {
		char *extn;
		WORD DisplayFileType;
	} FileTypeEntry;

	std::wstring folderPath;

	void TranslateFileType(NativeFile *file);

	DWORD FileTimeToUnixTime( FILETIME *ft );

public:
	std::vector<std::wstring> WindowsFiles;
};
