#pragma once
#include "../NUTS/directory.h"

#include "BAM.h"

#include "CBMDefs.h"

class D64Directory :
	public Directory
{
public:
	D64Directory(DataSource *pDataSource) : Directory(pDataSource) {
		NoLengthChase = false;
		IsOpenCBM     = false;
		Drive         = 0;
	}

	~D64Directory(void) {
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);

	void Shorten( unsigned char *dptr );

public:
	BAM *pBAM;
	BYTE Drive;

	bool NoLengthChase;
	bool IsOpenCBM;
};
