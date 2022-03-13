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
	int  ReadDirectory(void);
	int  WriteDirectory(void);
	void WriteJDirectory( DWORD DirSector, DWORD JParentSector, DWORD JParentSize );

	DWORD ProjectedDirectorySize( bool Joliet = false );

	void  ConvertUnixTime( BYTE *pTimestamp, DWORD Unixtime );

	void  Push( DWORD Extent, DWORD Length );
	void  Pop();

public:
	ISOVolDesc *pPriVolDesc;
	ISOVolDesc *pJolietDesc;

	DWORD DirSector;
	DWORD DirLength;

	DWORD JDirSector;
	DWORD JDirLength;

	DWORD ParentSector;
	DWORD ParentLength;

	FSIdentifier FSID;

	FOPTranslateFunction ProcessFOP;

	void *pSrcFS;

	bool CloneWars;
	bool UsingJoliet;

	std::map<DWORD, FOPReturn> FileFOPData;

	std::vector<DWORD> PathStackExtent;
	std::vector<DWORD> PathStackSize;

private:
	NativeFile AssocFile;

private:
	bool RockRidge( BYTE *pEntry, NativeFile *pTarget );

	int	ReadJDirectory(void);

	void ClearAuxData();

	DWORD ConvertISOTime( BYTE *pTimestamp );

};

