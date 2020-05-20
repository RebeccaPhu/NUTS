#pragma once

#include "../nuts/directory.h"
#include "NewFSMap.h"
#include "Defs.h"

class ADFSEDirectory : public Directory
{

public:
	ADFSEDirectory::ADFSEDirectory(DataSource *pDataSource) : Directory(pDataSource) {
		DirSector    = 0;
		ParentSector = 0;
	}

	~ADFSEDirectory(void)
	{
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);

	NewFSMap *pMap;

	DWORD DirSector;
	DWORD ParentSector;

	BYTE MasterSeq;
	BYTE DirTitle[ 19 ];
	BYTE DirName[  10 ];

private:
	void TranslateType( NativeFile *file );
};

