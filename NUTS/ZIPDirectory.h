#pragma once

#include "Directory.h"
#include "DataSource.h"

class ZIPDirectory : public Directory
{
public:
	ZIPDirectory( DataSource *pSrc ) : Directory( pSrc )
	{
		pSource = pSrc;
	}

	~ZIPDirectory( void )
	{
	}

public:
	int ReadDirectory(void);
	int WriteDirectory(void);

};