#pragma once
#include "../NUTS/directory.h"

#include "CBMDefs.h"

#define AttrType   Attributes[1]
#define AttrLocked Attributes[2]
#define AttrClosed Attributes[3]

#include <vector>

using namespace std;

class IECATADirectory :
	public Directory
{
public:
	IECATADirectory(DataSource *pDataSource) : Directory(pDataSource) {
	}

	~IECATADirectory(void) {
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);

	int  GetFreeBlock();
	void ReleaseBlock( std::vector<DWORD> *pBlocks );

	void SetDirSector( DWORD Sector )
	{
		DirSector = Sector;
	}

private:
	std::vector<int>	DirectorySectorChain;

	DWORD DirSector;
};
