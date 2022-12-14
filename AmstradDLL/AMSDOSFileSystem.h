#pragma once

#include "../NUTS/FileSystem.h"
#include "../NUTS/CPM.h"
#include "AMSDOSDirectory.h"

#include "Defs.h"

class AMSDOSFileSystem :
	public CPMFileSystem
{
public:
	AMSDOSFileSystem(DataSource *pDataSource);

	~AMSDOSFileSystem(void)
	{
	}

public:
	bool IncludeHeader( BYTE *pHeader );

	FSHint Offer( BYTE *Extension );

	int Format_Process( DWORD FT, HWND hWnd );

	std::vector<AttrDesc> GetAttributeDescriptions( NativeFile *pFile = nullptr );
	int SetProps( DWORD FileID, NativeFile *Changes );

	int EnhanceFileData( NativeFile *pFile );

	bool GetCPMHeader( NativeFile *pFile, BYTE *pHeader );

	int Imaging( DataSource *pImagingSource, DataSource *pImagingTarget, HWND ProgressWnd );
};

