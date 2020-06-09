#include "stdafx.h"

#include "ZipFuncs.h"
#include "libfuncs.h"

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "archive.h"

WCHAR *ZIPError( archive *a )
{
	static WCHAR Error[ 256 ];

	WCHAR *pErr = UString( (char *) archive_error_string( a ) );

	wcscpy_s( Error, pErr );

	return Error;
}