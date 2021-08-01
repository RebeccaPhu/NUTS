#pragma once
#include "e:\nuts\nuts\filesystem.h"
#include "TRDDirectory.h"

class TRDFileSystem :
	public FileSystem
{
public:
	TRDFileSystem(DataSource *pDataSource);
	~TRDFileSystem(void);

public:
	virtual FSHint Offer( BYTE *Extension );

	int Init(void) {
		SetShape();

		pDirectory	= (Directory *) new TRDDirectory( pSource );

		pDirectory->ReadDirectory();
		
		return 0;
	}

	int ReadFile(DWORD FileID, CTempFile &store);

private:
	void SetShape();
};

