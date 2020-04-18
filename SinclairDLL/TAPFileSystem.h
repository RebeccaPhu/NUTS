#pragma once

#include "../NUTS/FileSystem.h"

#include "TAPDirectory.h"

class TAPFileSystem :
	public FileSystem
{
public:
	TAPFileSystem(DataSource *pDataSource);
	~TAPFileSystem(void);

public:
	FSHint Offer( BYTE *Extension );

	int Init(void) {
		pDirectory	= (Directory *) new TAPDirectory( pSource );

		pDirectory->ReadDirectory();
		
		return 0;
	}

	DWORD GetEncoding(void )
	{
		return ENCODING_SINCLAIR;
	}

	BYTE *GetStatusString(int FileIndex);
	BYTE *GetTitleString( NativeFile *pFile);

	int	ReadFile(DWORD FileID, CTempFile &store);
	int	WriteFile(NativeFile *pFile, CTempFile &store);
	BYTE *DescribeFile( DWORD FileIndex );

	AttrDescriptors GetAttributeDescriptions( void );
};
