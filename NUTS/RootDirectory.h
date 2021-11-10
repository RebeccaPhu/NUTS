#pragma once
#include "directory.h"
#include "Plugins.h"

#define ROOT_OBJECT_WINDOWS_VOLUME 0
#define ROOT_OBJECT_RAW_DEVICE     1
#define ROOT_OBJECT_SPECIAL_FOLDER 2
#define ROOT_OBJECT_HOOK           3
#define ROOT_OBJECT_ROMDISK        4

typedef struct _FolderPair
{
	DWORD FolderID;
	std::wstring FolderName;
} FolderPair;

typedef std::map<DWORD, RootHook> HookPair;

class RootDirectory : public Directory
{
public:
	RootDirectory(void) : Directory(NULL) {
	};

	~RootDirectory(void) {
		HookPairs.clear();
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);

	static FolderPair Folders[];
	
	HookPair HookPairs;

private:
	void TranslateIconToResolved( NativeFile *pFile, HBITMAP hBitmap );
};
