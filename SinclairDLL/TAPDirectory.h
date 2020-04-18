#pragma once

#include "../NUTS/Directory.h"

class TAPDirectory :
	public Directory
{
public:
	TAPDirectory(DataSource *pDataSource);
	~TAPDirectory(void);

public:
	int ReadDirectory(void);
	int WriteDirectory(void);

};

