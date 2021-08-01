#pragma once

#include "../nuts/TZXDirectory.h"
#include "Defs.h"

extern TZXPB AmstradPB;

class AmstradCDTDirectory :
	public TZXDirectory
{
public:
	AmstradCDTDirectory( DataSource *pDataSource, TZXPB &pb ) : TZXDirectory( pDataSource, pb )
	{
	}

	~AmstradCDTDirectory(void)
	{
	}
};

