#pragma once

#include "stdafx.h"
#include <vector>
#include "DataSource.h"
#include <string>

class DataSourceCollector
{
public:
	DataSourceCollector(void)
	{
		sources.clear();

		InitializeCriticalSection( &StupidLock );
	}

	~DataSourceCollector(void)
	{
		DeleteCriticalSection( &StupidLock );
	}

	void RegisterDataSource( DataSource *pSource )
	{
		EnterCriticalSection( &StupidLock );

		sources.push_back( pSource );

		LeaveCriticalSection( &StupidLock );
	}

	void ReleaseSources( void )
	{
		/* Nobody cares MS */
#pragma warning( disable : 4996 )

#ifdef _DEBUG
		char dbg[256];

		sprintf( dbg, "Evaluating %d sources\n", sources.size() );
		OutputDebugStringA( dbg );
#endif

		std::vector<DataSource *>::iterator iSource;

		EnterCriticalSection( &StupidLock );

		for (iSource=sources.begin(); iSource != sources.end(); )
		{
			if ( (*iSource)->RefCount == 0 )
			{
#ifdef _DEBUG
				sprintf( dbg, "Deleted source at %08X\n", *iSource );
				OutputDebugStringA( dbg );
#endif

				delete *iSource;

				iSource = sources.erase( iSource );
			}
			else
			{
#ifdef _DEBUG
				sprintf( dbg, "Source at %08X has count %d\n", *iSource, (*iSource)->RefCount );
				OutputDebugStringA( dbg );
#endif

				iSource++;
			}
		}

		LeaveCriticalSection( &StupidLock );

#ifdef _DEBUG
		sprintf( dbg, "%d sources remain\n", sources.size() );
		OutputDebugStringA( dbg );
#endif

#pragma warning( default : 4996 )
	}

private:
	CRITICAL_SECTION StupidLock;

	std::vector<DataSource *> sources;
};
