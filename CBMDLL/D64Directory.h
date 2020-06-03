#pragma once
#include "../NUTS/directory.h"

#include "BAM.h"

#ifndef ENCODING_PETSCII
#define ENCODING_PETSCII 0xCC0E5C11
#endif

#define FSID_D64         0xCC000D64

#ifndef FT_C64
#define FT_C64 0xCC00F17E
#endif

class D64Directory :
	public Directory
{
public:
	D64Directory(DataSource *pDataSource) : Directory(pDataSource) {
		NoLengthChase = false;
	}

	~D64Directory(void) {
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);

	void Shorten( unsigned char *dptr );

public:
	BAM *pBAM;

	bool NoLengthChase;
};
