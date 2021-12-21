#pragma once

#include "directory.h"
#include "ISODefs.h"

#include "FOP.h"

#include <map>

class ISO9660Directory :
	public Directory
{
public:
	ISO9660Directory( DataSource *pDataSource );
	~ISO9660Directory(void);

public:
	int	ReadDirectory(void);
	int	WriteDirectory(void);

public:
	ISOVolDesc *pPriVolDesc;
	ISOVolDesc *pJolietDesc;

	DWORD DirSector;
	DWORD DirLength;

	FOPTranslateFunction ProcessFOP;

	void *pSrcFS;

	bool CloneWars;

	std::map<DWORD, FOPReturn> FileFOPData;

private:
	NativeFile AssocFile;

private:
	bool RockRidge( BYTE *pEntry, NativeFile *pTarget );
};

