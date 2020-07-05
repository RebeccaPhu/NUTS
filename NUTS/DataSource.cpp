#include "StdAfx.h"
#include "DataSource.h"
#include "DataSourceCollector.h"

extern DataSourceCollector *pCollector;

DataSource::DataSource(void)
{
	InitializeCriticalSection(&RefLock);

	pCollector->RegisterDataSource( this );

	RefCount = 1;

	Flags    = 0;
}

DataSource::~DataSource(void)
{
	DeleteCriticalSection(&RefLock);
}
