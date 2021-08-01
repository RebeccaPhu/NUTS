#pragma once

#include "TranslatedSector.h"
#include "../NUTS/DataSource.h"
#include "Defs.h"

class OldFSMap : public TranslatedSector
{
public:
	OldFSMap(DataSource *pDataSource) {
		pSource	= pDataSource;

		pSource->Retain();

		FloppyFormat = false;
		UseDFormat   = false;
	}

	~OldFSMap( void )
	{
		pSource->Release();
	}

	int	GetStartSector(FreeSpace &space);
	int	OccupySpace(FreeSpace &space);
	int ReleaseSpace(FreeSpace &space);
	int	ClaimSpace(FreeSpace &space);

	int	ReadFSMap();
	int	WriteFSMap();

	friend class ADFSFileSystem;

protected:
	std::vector<FreeSpace>	Spaces;

	DataSource	*pSource;

	long	TotalSectors;
	int		BootOption;
	unsigned short	DiscIdentifier;
	int		NextEntry;
	bool    FloppyFormat;
	bool	UseDFormat;

	BYTE DiscName0[ 5 ];
	BYTE DiscName1[ 5 ];
};

