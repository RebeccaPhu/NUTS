#pragma once

#include "../nuts/TZXDirectory.h"
#include "SinclairDefs.h"

extern TZXPB SinclairPB;

class SinclairTZXDirectory :
	public TZXDirectory
{
public:
	SinclairTZXDirectory( DataSource *pDataSource, TZXPB &pb ) : TZXDirectory( pDataSource, pb )
	{
	}

	~SinclairTZXDirectory(void)
	{
	}

	void ResolveFiles( void );
};

