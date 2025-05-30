#pragma once
#include "cpmdirectory.h"
class P2000Directory :
	public CPMDirectory
{
public:
	P2000Directory( DataSource *pDataSource, CPMBlockMap *pMap, CPMDPB &dpb ) : CPMDirectory( pDataSource, pMap, dpb )
	{
	}

	~P2000Directory(void)
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
};

