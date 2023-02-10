#pragma once

#include "..\nuts\directory.h"


#define ORIC_TAP_FILE_BASIC 0x000000
#define ORIC_TAP_FILE_MCODE 0x000080
#define ORIC_TAP_FILE_AR_IN 0x800040
#define ORIC_TAP_FILE_AR_RL 0x000040
#define ORIC_TAP_FILE_AR_ST 0x00FF40

class OricTAPDirectory :
	public Directory
{
public:
	OricTAPDirectory( DataSource *pSrc );
	~OricTAPDirectory( void );

public:
	int ReadDirectory( void );
	int WriteDirectory( void );
};

