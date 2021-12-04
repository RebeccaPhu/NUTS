#pragma once

#include "ISO9660Directory.h"
#include "filesystem.h"

class ISO9660FileSystem :
	public FileSystem
{
public:
	ISO9660FileSystem( DataSource *pDataSource );
	~ISO9660FileSystem(void);
};

