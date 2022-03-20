#pragma once

#include "../NUTS/Directory.h"

class AmigaDirectory :
	public Directory
{
public:
	AmigaDirectory(DataSource *pDataSource) : Directory(pDataSource) {
		DirSector    = 880;
		ParentSector = 880;
	}

	~AmigaDirectory(void) {
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);

public:
	DWORD DirSector;
	DWORD ParentSector;

	void ProcessHashChain( DWORD NSector );

	DWORD FileID;
};

