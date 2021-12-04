#include "StdAfx.h"
#include "ISO9660Directory.h"


ISO9660Directory::ISO9660Directory( DataSource *pDataSource ) : Directory( pDataSource )
{
}


ISO9660Directory::~ISO9660Directory(void)
{
}

int	ISO9660Directory::ReadDirectory(void)
{
	return 0;
}

int	ISO9660Directory::WriteDirectory(void)
{
	return 0;
}
