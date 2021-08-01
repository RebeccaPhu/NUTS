#pragma once

#include "AmstradCDTDirectory.h"
#include "Defs.h"
#include "../nuts/TZXFileSystem.h"


class AmstradCDTFileSystem :
	public TZXFileSystem
{
public:
	AmstradCDTFileSystem( DataSource *pDataSource ) : TZXFileSystem( pDataSource, AmstradPB )
	{
		pAmstradDir = new AmstradCDTDirectory( pDataSource, AmstradPB );
		pDir        = (TZXDirectory *) pAmstradDir;
		pDirectory  = (Directory *) pDir;

		FSID = FSID_AMSTRAD_CDT;
	}

	~AmstradCDTFileSystem(void)
	{
	}

public:
	AmstradCDTDirectory *pAmstradDir;
};

