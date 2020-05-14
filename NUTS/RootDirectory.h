#pragma once
#include "directory.h"

typedef struct _FolderPair
{
	DWORD FolderID;
	std::wstring FolderName;
} FolderPair;

class RootDirectory : public Directory
{
public:
	RootDirectory(void) : Directory(NULL) {
	};

	~RootDirectory(void) {
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);

	static FolderPair Folders[];
};
