#include "StdAfx.h"
#include "TempFile.h"


CTempFile::CTempFile(void)
{
	Ptr = 0;

	bKeep = false;

	MemorySize = 0;
	InMemory   = true;
	pMemory    = nullptr;
}

CTempFile::CTempFile( std::wstring exFile )
{
	PathName = exFile;

	Ptr = 0;

	bKeep = false;

	InMemory   = false;
	MemorySize = 0;
	pMemory    = nullptr;
}

CTempFile::~CTempFile(void)
{
	if ( !bKeep )
	{
		_wunlink( PathName.c_str() );
	}

	if ( pMemory )
	{
		free( pMemory );
	}

	pMemory = nullptr;
}

void CTempFile::DumpMemory()
{
	if ( InMemory )
	{
		WCHAR TempPath[ MAX_PATH + 1 ];
		WCHAR TempName[ MAX_PATH + 1 ];

		GetTempPathW( MAX_PATH, TempPath );

		GetTempFileNameW( TempPath, L"NUTS", 0, TempName );

		PathName = std::wstring( TempName );

		FILE *f = nullptr;
	
		_wfopen_s( &f, PathName.c_str(), L"wb" );

		if ( f == nullptr )
		{
			return;
		}

		if ( ( MemorySize > 0 ) && ( pMemory != nullptr ) )
		{
			fwrite( pMemory, 1, MemorySize, f );
		}

		fclose( f );

		InMemory   = false;
		MemorySize = 0;

		if ( pMemory != nullptr )
		{
			free( pMemory );
		}

		pMemory  = nullptr;
	}
}

void CTempFile::Seek( QWORD NewPtr )
{
	Ptr = NewPtr;
}

QWORD CTempFile::Ext( void )
{
	if ( InMemory )
	{
		return (QWORD) MemorySize;
	}

	FILE *f = nullptr;
	
	_wfopen_s( &f, PathName.c_str(), L"rb" );

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
	if ( InMemory )
	{
		if ( ( Ptr + Length ) > MAX_MEMORY_SIZE )
		{
			DumpMemory();
		}
		else
		{
			if ( ( Ptr + Length ) > MemorySize )
			{
				MemorySize = Ptr+Length;

				pMemory = (BYTE *) realloc( pMemory, MemorySize );
			}

			memcpy( &pMemory[ Ptr ], Buffer, Length );

			Ptr += Length;

			return;
		}
	}

	FILE *f = nullptr;
	
	_wfopen_s( &f, PathName.c_str(), L"rb+" );

	if ( f == nullptr )
	{
		_wfopen_s( &f, PathName.c_str(), L"wb" );
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

void CTempFile::Dump()
{
	DumpMemory();
}

void CTempFile::Read( void *Buffer, DWORD Length )
{
	if ( InMemory )
	{
		DWORD CpyLen = Length;

		if ( ( CpyLen + Ptr ) > MemorySize )
		{
			CpyLen = MemorySize - Ptr;
		}

		if ( Ptr < MemorySize )
		{
			memcpy( Buffer, &pMemory[ Ptr ], CpyLen );
		}

		Ptr += Length;

		return;
	}

	FILE *f = nullptr;
	
	_wfopen_s( &f, PathName.c_str(), L"rb" );

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
	if ( NewPtr == Ext() )
	{
		return;
	}

	if ( NewPtr > MAX_MEMORY_SIZE )
	{
		DumpMemory();
	}
	else if ( InMemory )
	{
		pMemory = (BYTE *) realloc( pMemory, NewPtr );

		MemorySize = NewPtr;

		return;
	}

	if ( NewPtr < Ext() )
	{
		HANDLE hFile;

		hFile = ::CreateFile(
			PathName.c_str(), 
			GENERIC_READ | GENERIC_WRITE, 
			NULL, 
			NULL, 
			OPEN_EXISTING, 
			FILE_ATTRIBUTE_NORMAL, 
			NULL
		); 

		LONG JesusWindows = (LONG) ( NewPtr >> 32 );

		if ( hFile != INVALID_HANDLE_VALUE )
		{
			if ( SetFilePointer( hFile, (LONG) ( NewPtr & 0xFFFFFFFF ), &JesusWindows, FILE_BEGIN ) == INVALID_SET_FILE_POINTER )
			{
				return;
			}

			if ( SetEndOfFile( hFile ) == FALSE )
			{
				return;
			}

			CloseHandle( hFile );
		}

		return;
	}

	FILE *f = nullptr;
	
	_wfopen_s( &f, PathName.c_str(), L"rb+" );

	if ( f == nullptr )
	{
		_wfopen_s( &f, PathName.c_str(), L"wb" );
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
	DumpMemory();

	bKeep = true;
}

void CTempFile::Unkeep( void )
{
	bKeep = false;
}

void CTempFile::KeepAs( std::wstring Filename )
{
	Keep();

	// Preemptive remove any existing file
	_wunlink( Filename.c_str() );

	_wrename( PathName.c_str(), Filename.c_str() );
}
