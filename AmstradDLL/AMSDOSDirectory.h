#pragma once

#include "../NUTS/Directory.h"
#include "../NUTS/CPMDirectory.h"

class AMSDOSDirectory :
	public CPMDirectory
{
public:
	AMSDOSDirectory( DataSource *pDataSource, CPMBlockMap *pMap, CPMDPB &dpb ) : CPMDirectory( pDataSource, pMap, dpb )
	{
	}

	~AMSDOSDirectory(void)
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

