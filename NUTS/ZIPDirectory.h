#pragma once

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "Directory.h"
#include "DataSource.h"
#include "ZIPCommon.h"
#include "FOP.h"

#include "libfuncs.h"

class ZIPDirectory : public Directory, ZIPCommon
{
public:
	ZIPDirectory( DataSource *pSrc ) : Directory( pSrc )
	{
		pSource = pSrc;

		rstrncpy( cpath, (BYTE *) "", 255 );

		CloneWars = false;
	}

	~ZIPDirectory( void )
	{
	}

public:
	int ReadDirectory(void);
	int WriteDirectory(void);

	BYTE cpath[ 256 ];

	DataSource *GetSource()
	{
		return pSource;
	}

	FOPTranslateFunction ProcessFOP;

	void *srcFS;
	bool CloneWars;

private:
	void TranslateFileType(NativeFile *file);
};