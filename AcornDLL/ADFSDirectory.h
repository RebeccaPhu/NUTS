#pragma once
#include "../NUTS/directory.h"

#include "Defs.h"

class ADFSDirectory :
	public Directory
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

	char	DirString[11];
	char	DirTitle[20];

	DWORD DirSector;
	DWORD ParentSector;
	DWORD FSID;

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
	DWORD TranslateSector(DWORD InSector);
	void TranslateType( NativeFile *file );

	DWORD SecSize;
};
