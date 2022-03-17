#pragma once
#include "..\nuts\directory.h"
#include "AppleDefs.h"
#include "..\NUTS\libfuncs.h"
#include "..\NUTS\TempFile.h"
#include "MFSFAT.h"

#include <vector>

class MacintoshMFSDirectory :
	public Directory
{
public:
	MacintoshMFSDirectory( DataSource *pSource );
	~MacintoshMFSDirectory(void);

public:
	int ReadDirectory(void);
	int WriteDirectory(void);

	void GetFinderCodes( DWORD FileID, BYTE *pType, BYTE *pFinder );

public:
	MFSVolumeRecord *pRecord;
	MFSFAT *pFAT;

	std::vector<AutoBuffer> FinderInfo;

private:
	void ReadResourceFork( DWORD FileID, CTempFile &fileobj );
	void ClearIcons();
	void SetFileType( NativeFile* pFile, BYTE *pFinderData );
};

