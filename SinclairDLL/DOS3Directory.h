#pragma once

#include "../NUTS/CPMDirectory.h"

class DOS3Directory :
	public CPMDirectory
{
public:
	DOS3Directory( DataSource *pDataSource, CPMBlockMap *pMap, CPMDPB &dpb ) : CPMDirectory( pDataSource, pMap, dpb )
	{
	}

	~DOS3Directory(void)
	{
	}

	int ReadDirectory(void)
	{
		return CPMDirectory::ReadDirectory();
	}

	int WriteDirectory(void)
	{
		return CPMDirectory::WriteDirectory();
	}

	void ExtraReadDirectory( NativeFile *pFile );
};

