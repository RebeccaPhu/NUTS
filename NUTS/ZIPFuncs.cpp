#include "stdafx.h"

#include "ZipFuncs.h"
#include "libfuncs.h"

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "archive.h"
#include "zip.h"

/* Converts a libarchive error to a static error string for use with NUTSError */
WCHAR *ZIPError( archive *a )
{
	static WCHAR Error[ 256 ];

//	WCHAR *pErr = UString( (char *) archive_error_string( a ) );

	wcscpy_s( Error, L"moo" ); // pErr );

	return Error;
}

/* Confirm if the current filename in an archive matches the current path */
bool IsCPath( BYTE *path, BYTE *name )
{
	/* Get the position of the last forward slash */
	WORD p=0xFFFF;

	for ( WORD n=0; n<256; n++ )
	{
		if ( name[ n ] == 0 )
		{
			break;
		}

		if ( name[ n ] == '/' )
		{
			p = n;
		}
	}

	WORD l = rstrnlen( path, 255 );

	/* This ends in a slash, it's the path itself */
	if ( p == ( rstrnlen( name, 255 ) - 1 ) )
	{
		return false;
	}

	if ( ( p == 0xFFFF ) && ( l == 0 ) )
	{
		return true;
	}

	if ( p > l )
	{
		return false;
	}

	if ( rstrnicmp( path, name, l ) )
	{
		return true;
	}

	return false;
}

/* Confirm if the current filename is a subpath of the current path,
   and return the top-level subpath if it is. Returns nullptr otherwise. */
BYTE *ZIPSubPath( BYTE *path, BYTE *name )
{
	WORD l = rstrnlen( path, 255 );

	if ( rstrnlen( name, 255 ) < l )
	{
		return nullptr;
	}

	if ( !rstrnicmp( name, path, l ) )
	{
		return nullptr;
	}

	/* Assumes that IsCPath has already been used */
	BYTE *pStart = &name[ l ];

	/* Get the position of the next forward slash */
	WORD p=0xFFFF;

	for ( WORD n=0; n<256; n++ )
	{
		if ( pStart[ n ] == 0 )
		{
			break;
		}

		if ( pStart[ n ] == '/' )
		{
			p = n;

			break;
		}
	}

	if ( p == 0xFFFF )
	{
		return nullptr;
	}

	static BYTE dpath[ 256 ];

	rstrncpy( dpath, pStart, p );

	return dpath;
}

BYTE *ZIPSubName( BYTE *path, BYTE *name )
{
	WORD l = rstrnlen( path, 255 );

	BYTE *pStart  = &name[ l ];

	/* Get the position of the next forward slash */
	WORD p=0xFFFF;

	for ( WORD n=0; n<256; n++ )
	{
		if ( pStart[ n ] == 0 )
		{
			break;
		}

		if ( pStart[ n ] == '/' )
		{
			p = n;

			break;
		}
	}

	if ( p == 0xFFFF )
	{
		return pStart;
	}

	static BYTE ShortPath[ 256 ];

	rstrncpy( ShortPath, pStart, p );

	return ShortPath;
}
