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
		
		if ( pSource != nullptr )
		{
			DS_RETAIN( pSource );
		}
	}

	virtual ~Directory(void) {
		if ( pSource != nullptr )
		{
			DS_RELEASE( pSource );
		}

		Files.clear();
	}

	virtual	int	ReadDirectory(void) = 0;
	virtual	int	WriteDirectory(void) = 0;

	std::vector<NativeFile>	Files;

	ResolvedIconList ResolvedIcons;

protected:
	DataSource *pSource;

};
