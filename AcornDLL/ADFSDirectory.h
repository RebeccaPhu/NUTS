#pragma once
#include "ADFSDirectoryCommon.h"
#include "../NUTS/directory.h"

#include "Defs.h"

class ADFSDirectory :
	public Directory, public ADFSDirectoryCommon
{
public:
	ADFSDirectory::ADFSDirectory(DataSource *pDataSource) : Directory(pDataSource) {
		DirSector  = 0;
		UseLFormat = false;
		UseDFormat = false;
		SecSize    = 256;
		FSID       = FS_Null;
	}

	~ADFSDirectory(void)
	{
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);

	unsigned char	MasterSeq;

	BYTE DirString[11];
	BYTE DirTitle[20];

	DWORD DirSector;
	DWORD ParentSector;
	FSIdentifier FSID;

	bool  UseLFormat;
	bool  UseDFormat;

public:
	void SetSector( DWORD Sector );

	DWORD GetSector( void )
	{
		return DirSector;
	}

	DWORD GetParentSector( void )
	{
		return ParentSector;
	}
	
	void SetLFormat(void)
	{
		UseLFormat = true;
	}

	void SetDFormat(void)
	{
		UseDFormat = true;

		SecSize    = 1024;
	}

private:
	DWORD SecSize;
};
