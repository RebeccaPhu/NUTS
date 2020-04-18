#pragma once
#include "../NUTS/directory.h"
#include "../NUTS/DataSource.h"

#include "Defs.h"

class AcornDFSDirectory :
	public Directory
{
public:
	AcornDFSDirectory(DataSource *pSource) : Directory(pSource) {
		CurrentDir = '$';
	}

	~AcornDFSDirectory(void) {
	}

	int	ReadDirectory(void);
	int	WriteDirectory(void);

	unsigned char	MasterSeq;

	BYTE	Option;
	WORD	NumSectors;

	BYTE	DiscTitle[12];

	BYTE    CurrentDir;
};
