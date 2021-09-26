#pragma once
#include "directory.h"
#include "Plugins.h"

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
