#pragma once

#include "CPM.h"

class P2000FileSystem :
	public CPMFileSystem
{
public:
	P2000FileSystem( DataSource *pDataSource );
	~P2000FileSystem(void);

	FSHint Offer( BYTE *Extension );
};

