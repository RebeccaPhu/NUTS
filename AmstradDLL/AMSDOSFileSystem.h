#pragma once

#include "../NUTS/FileSystem.h"
#include "../NUTS/CPM.h"
#include "AMSDOSDirectory.h"

#include "Defs.h"

class AMSDOSFileSystem :
	public CPMFileSystem
{
public:
	AMSDOSFileSystem(DataSource *pDataSource);

	~AMSDOSFileSystem(void)
	{
	}

public:
	bool IncludeHeader( BYTE *pHeader );

	FSHint Offer( BYTE *Extension );
};

