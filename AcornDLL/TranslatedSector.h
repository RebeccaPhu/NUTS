#ifndef _TRANSLATEDSECTOR_H
#define _TRANSLATEDSECTOR_H

#include "stdafx.h"

#include "../NUTS/DataSource.h"

class TranslatedSector {

public:
	TranslatedSector();

public:
	bool FloppyFormat;

	DiskShape MediaShape;

protected:
	int ReadTranslatedSector( QWORD LBA, BYTE *Buffer, WORD SectorSize, DataSource *pSource );
	int WriteTranslatedSector( QWORD LBA, BYTE *Buffer, WORD SectorSize, DataSource *pSource );
};

#endif