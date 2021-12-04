#pragma once
#include "directory.h"
class ISO9660Directory :
	public Directory
{
public:
	ISO9660Directory( DataSource *pDataSource );
	~ISO9660Directory(void);

public:
	int	ReadDirectory(void);
	int	WriteDirectory(void);

};

