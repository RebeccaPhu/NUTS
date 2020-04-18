#pragma once

#include <vector>
#include "defs.h"
#include "FileSystem.h"

class CFileSystemIndex
{
public:
	typedef struct _FSD {
		_FSID		FSID;
		WCHAR		FSName[16];
		WCHAR		FSDesc[64];
		FSCreate	pCreateFunc;
		bool		HardDiskType;
		bool		FloppyDiskType;
		bool		Formattable;
		bool		DoubleSided;
		bool		Writable;
		bool		Subdirectories;
	} FSD;

	CFileSystemIndex(void) {
		FSD	fslist[]	= {
			{ FS_AcornDFS,	L"Acorn DFS",		L"Acorn 40-track or 80-track DFS disk",	AcornDFSFileSystem::createNew,
				false,	true,	true,	true,	true,	false	},

			{ FS_ADFS,		L"Acorn ADFS",		L"Acorn ADFS S, M or L format",			ADFSFileSystem::createNew,
				true,	true,	true,	false,	true,	true	},

			{ FS_D64,		L"Commodore D64",	L"Commodore 1541/D64 disk",				D64FileSystem::createNew,
				false,	true,	true,	false,	true,	false	},

			{ FS_IECATA,	L"IEC-ATA",			L"IEC-ATA for Commodore machines",		IECATAFileSystem::createNew,
				true,	false,	true,	false,	true,	true	},

			{ FS_Unknown,	L"",				L"",									NULL,
				false,	false,	false,	false,	false,	false	},
		};

		//	Store this in a vector so it can be iterated without having to update a counter every time code changes
		//	are made. Eventually, this will extend to dynamically loadable (DLL) filesystems too.
		for (int i=0; fslist[i].FSID != FS_Unknown; i++)
			FS.push_back(fslist[i]);
	}

	~CFileSystemIndex(void) {
	}

	std::vector<FSD>	FS;
};
