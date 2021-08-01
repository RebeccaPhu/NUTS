#pragma once
#include "e:\nuts\nuts\directory.h"

class TRDDirectory :
	public Directory
{
public:
	TRDDirectory(DataSource *pDataSource) : Directory( pDataSource ) {
	}

	virtual ~TRDDirectory(void) {
	}

public:
	int ReadDirectory(void);
	int WriteDirectory(void);
};

