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

		BigDirName   = nullptr;
	}

	~ADFSEDirectory(void)
	{
	}

	int	ReadDirectory(void);
	int	ReadEDirectory(void);
	int	ReadPDirectory(void);

	int	WriteDirectory(void);
	int	WriteEDirectory(void);
	int	WritePDirectory(void);

	NewFSMap *pMap;

	DWORD DirSector;
	DWORD ParentSector;

	BYTE MasterSeq;
	BYTE DirTitle[ 19 ];
	BYTE DirName[  10 ];

	BYTE *BigDirName;

	DWORD SecSize;

private:
	void TranslateType( NativeFile *file );
};

