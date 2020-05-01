#pragma once

#include "../nuts/directory.h"
#include "Defs.h"

class SpriteFileDirectory : public Directory
{

public:
	SpriteFileDirectory(DataSource *pDataSource) : Directory(pDataSource) {
		UseResolvedIcons = false;
	}

	~SpriteFileDirectory(void)
	{
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);

public:
	bool UseResolvedIcons;

};

