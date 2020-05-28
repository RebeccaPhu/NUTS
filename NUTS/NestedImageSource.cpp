#include "StdAfx.h"
#include "NestedImageSource.h"
#include "FileSystem.h"

#include <process.h>

NestedImageSource::NestedImageSource( void *pFS, NativeFile *pSource, std::wstring Path ) : ImageDataSource( Path )
{
	pSourceFS    = pFS;
	TempPath     = Path;
	SourceObject = *pSource;

	hObserverThread = NULL;

	hTerminate = CreateEvent( NULL, TRUE, FALSE, NULL );

	InitializeCriticalSection( &StatusLock );
}

NestedImageSource::~NestedImageSource(void)
{
	if ( hObserverThread != NULL )
	{
		SetEvent( hTerminate );

		if ( WaitForSingleObject( hObserverThread, 500 ) == WAIT_TIMEOUT )
		{
			TerminateThread( hObserverThread, 500 );
		}

		CloseHandle( hObserverThread );
	}

	CloseHandle( hTerminate );

	DeleteCriticalSection( &StatusLock );

	if ( Dirty )
	{
		FileSystem *pFS = (FileSystem *) pSourceFS;

		CTempFile FileObj( TempPath.c_str() );

		FileObj.Keep();

		pFS->ReplaceFile( &SourceObject, FileObj );
	}
}

int NestedImageSource::WriteSector(long Sector, void *pSectorBuf, long SectorSize)
{
	CreateObserver();

	if ( ImageDataSource::WriteSector( Sector, pSectorBuf, SectorSize ) != DS_SUCCESS )
	{
		return -1;
	}

	EnterCriticalSection( &StatusLock );

	DirtyTime = time( NULL );
	Dirty     = true;

	LeaveCriticalSection( &StatusLock );

	return 0;
}

int NestedImageSource::WriteRaw( QWORD Offset, DWORD Length, BYTE *pBuffer )
{
	CreateObserver();

	if ( ImageDataSource::WriteRaw( Offset, Length, pBuffer ) != DS_SUCCESS )
	{
		return -1;
	}

	EnterCriticalSection( &StatusLock );

	DirtyTime = time( NULL );
	Dirty     = true;

	LeaveCriticalSection( &StatusLock );

	return 0;
}

int NestedImageSource::Observe( void )
{
	while ( 1 )
	{
		if ( WaitForSingleObject( hTerminate, 1000 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		EnterCriticalSection( &StatusLock );

		time_t Now     = time( NULL );

		if ( ( ( Now - DirtyTime ) > 1 ) && ( Dirty ) )
		{
			FileSystem *pFS = (FileSystem *) pSourceFS;

			CTempFile FileObj( TempPath.c_str() );

			FileObj.Keep();

			pFS->ReplaceFile( &SourceObject, FileObj );

			DirtyTime = time( NULL );
			Dirty     = false;
		}

		LeaveCriticalSection( &StatusLock );
	}

	return 0;
}

unsigned int __stdcall ObserverThread(void *param) {
	NestedImageSource *pClass = (NestedImageSource *) param;

	return pClass->Observe();
}

void NestedImageSource::CreateObserver( void )
{
	if ( hObserverThread != NULL )
	{
		return;
	}

	DirtyTime = time( NULL );

	DWORD dwthreadid;

	hObserverThread = (HANDLE) _beginthreadex( NULL, NULL, ObserverThread, this, NULL, (unsigned int *) &dwthreadid );
}