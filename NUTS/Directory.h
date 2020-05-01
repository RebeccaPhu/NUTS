#pragma once
#include "DataSource.h"
#include "Defs.h"
#include <vector>

using namespace std;

class Directory
{
public:
	Directory(DataSource *pDataSource) {
		pSource	= pDataSource;
		
		if ( pSource != nullptr ) { pSource->Retain(); }
	}

	virtual ~Directory(void) {
		if ( pSource != nullptr ) { pSource->Release(); }
	}

	virtual	int	ReadDirectory(void) = 0;
	virtual	int	WriteDirectory(void) = 0;

	std::vector<NativeFile>	Files;

	ResolvedIconList ResolvedIcons;

protected:
	DataSource *pSource;

};
