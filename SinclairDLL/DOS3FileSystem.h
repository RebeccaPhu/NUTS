#pragma once

#include "../NUTS/CPM.h"

class DOS3FileSystem :
	public CPMFileSystem
{
public:
	DOS3FileSystem(DataSource *pDataSource);

	~DOS3FileSystem(void)
	{
	}

public:
	bool IncludeHeader( BYTE *pHeader );

	FSHint Offer( BYTE *Extension );

	std::vector<AttrDesc> GetAttributeDescriptions( NativeFile *pFile = nullptr );

	bool GetCPMHeader( NativeFile *pFile, BYTE *pHeader );
	int  SetProps( DWORD FileID, NativeFile *Changes );

	int  Format_Process( DWORD FT, HWND hWnd );

	int EnhanceFileData( NativeFile *pFile );

	void CPMPreWriteCheck( NativeFile *pFile );

	int Imaging( DataSource *pImagingSource, DataSource *pImagingTarget, HWND ProgressWnd );
};

