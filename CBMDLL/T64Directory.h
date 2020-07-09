#pragma once
#include "e:\nuts\nuts\directory.h"
class T64Directory :
	public Directory
{
public:
	T64Directory( DataSource *pDataSource) : Directory( pDataSource )
	{
	}

	~T64Directory(void)
	{
	}

public:
	WORD TapeVersion;

	BYTE Container[ 18 ];

public:
	int ReadDirectory( void );

	int WriteDirectory( void ) { return 0; }
};

