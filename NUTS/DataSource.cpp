#include "StdAfx.h"
#include "DataSource.h"
#include "DataSourceCollector.h"

extern DataSourceCollector *pCollector;

DataSource::DataSource(void)
{
	pCollector->RegisterDataSource( this );

	RefCount = 1;
}

DataSource::~DataSource(void)
{
	RefCount--;
}
