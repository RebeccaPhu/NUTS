#include "StdAfx.h"
#include "ISO9660FileSystem.h"


ISO9660FileSystem::ISO9660FileSystem( DataSource *pDataSource ) : FileSystem( pDataSource )
{
	pDirectory = new ISO9660Directory( pDataSource );
}


ISO9660FileSystem::~ISO9660FileSystem(void)
{
	delete pDirectory;
}
