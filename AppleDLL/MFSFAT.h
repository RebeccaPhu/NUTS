#pragma once

#include "..\NUTS\DataSource.h"
#include "AppleDefs.h"

#include <map>

class MFSFAT
{
public:
	MFSFAT( DataSource *pSrc );
	~MFSFAT(void);

public:
	void ReadFAT();
	WORD NextAllocBlock( WORD ThisAllocBlock );
	WORD GetFreeAllocs();
	int  ClearAllocs( WORD StartAlloc );
	WORD GetFileAllocCount( WORD StartAlloc );
	int  WriteFAT();
	void CreateFAT( WORD Entries );

	std::vector<WORD> AllocateBlocks( WORD RequiredBlocks );

public:
	MFSVolumeRecord *pRecord;

private:
	DataSource *pSource;

	std::map<WORD,WORD> FAT;
};

