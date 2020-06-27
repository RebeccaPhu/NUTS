#pragma once

#include "../NUTS/CPM.h"

class DOS3FileSystem :
	public CPMFileSystem
{
public:
	DOS3FileSystem(DataSource *pDataSource);

	~DOS3FileSystem(void)
	{
	}

public:
	bool IncludeHeader( BYTE *pHeader );

	FSHint Offer( BYTE *Extension );
};

