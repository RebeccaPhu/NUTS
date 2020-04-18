#pragma once
#include "directory.h"

class RootDirectory : public Directory
{
public:
	RootDirectory(void) : Directory(NULL) {
	};

	~RootDirectory(void) {
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);
};
