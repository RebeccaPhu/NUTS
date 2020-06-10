#pragma once

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "Directory.h"
#include "DataSource.h"

#include "libfuncs.h"

class ZIPDirectory : public Directory
{
public:
	ZIPDirectory( DataSource *pSrc ) : Directory( pSrc )
	{
		pSource = pSrc;

		rstrncpy( cpath, (BYTE *) "", 255 );
	}

	~ZIPDirectory( void )
	{
	}

public:
	int ReadDirectory(void);
	int WriteDirectory(void);

	BYTE cpath[ 256 ];
};