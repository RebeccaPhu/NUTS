#pragma once
#include "../NUTS/directory.h"

#ifndef FT_C64
#define FT_C64 0xCC00F17E
#endif

#define FSID_IECATA      0x001ECA7A

#ifndef ENCODING_PETSCII
#define ENCODING_PETSCII 0xCC0E5C11
#endif

#define AttrLocked Attributes[1]
#define AttrClosed Attributes[2]
#define AttrType   Attributes[3]

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

	void PETSCII(unsigned char *pptr, int chars);
	void Shorten(unsigned char *sptr, unsigned char *dptr, int chars);
	
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
