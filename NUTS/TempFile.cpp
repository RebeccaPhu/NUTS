#include "StdAfx.h"
#include "TempFile.h"


CTempFile::CTempFile(void)
{
	CHAR TempPath[ MAX_PATH + 1 ];
	CHAR TempName[ MAX_PATH + 1 ];

	GetTempPathA( MAX_PATH, TempPath );

	GetTempFileNameA( TempPath, "NUTS", 0, TempName );

	PathName = std::string( TempName );

	Ptr = 0;

	bKeep = false;
}

CTempFile::CTempFile( std::string exFile )
{
	PathName = exFile;

	Ptr = 0;

	bKeep = false;
}

CTempFile::~CTempFile(void)
{
	if ( !bKeep )
	{
		_unlink( PathName.c_str() );
	}
}

void CTempFile::Seek( QWORD NewPtr )
{
	Ptr = NewPtr;
}

QWORD CTempFile::Ext( void )
{
	FILE *f = nullptr;
	
	fopen_s( &f, PathName.c_str(), "rb" );

	if ( f == nullptr )
	{
		return 0U;
	}

	_fseeki64( f, 0, SEEK_END );

	QWORD ext = _ftelli64( f );

	fclose( f );

	return ext;
}

void CTempFile::Write( void *Buffer, DWORD Length )
{
	FILE *f = nullptr;
	
	fopen_s( &f, PathName.c_str(), "rb+" );

	if ( f == nullptr )
	{
		fopen_s( &f, PathName.c_str(), "wb" );
	}

	if ( f == nullptr )
	{
		return;
	}

	_fseeki64( f, Ptr, SEEK_SET );

	fwrite( Buffer, 1, Length, f );

	Ptr += Length;

	fclose( f );
}

void CTempFile::Read( void *Buffer, DWORD Length )
{
	FILE *f = nullptr;
	
	fopen_s( &f, PathName.c_str(), "rb" );

	if ( f == nullptr )
	{
		return;
	}

	_fseeki64( f, Ptr, SEEK_SET );

	fread( Buffer, 1, Length, f );

	Ptr += Length;

	fclose( f );
}

void CTempFile::SetExt( QWORD NewPtr )
{
	FILE *f = nullptr;
	
	fopen_s( &f, PathName.c_str(), "rb+" );

	if ( f == nullptr )
	{
		fopen_s( &f, PathName.c_str(), "wb" );
	}

	if ( f == nullptr )
	{
		return;
	}

	_fseeki64( f, NewPtr - 1, SEEK_SET );

	BYTE Dummy[1] = { 0 };

	fwrite( Dummy, 1, 1, f );

	fclose( f );	 
}

void CTempFile::Keep( void )
{
	bKeep = true;
}
