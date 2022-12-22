#pragma once

#include "Directory.h"
#include "NUTSConstants.h"
#include "TempFile.h"
#include "FOP.h"

#define ROMDiskFSType L"ROMDisk_Extraneous"

class ROMDiskDirectory :
	public Directory
{
public:
	ROMDiskDirectory( DataSource *pDataSource ) : Directory( pDataSource )
	{
		DirectoryID = 0;
	}

	~ROMDiskDirectory(void)
	{
		CleanFiles();
	}

	int ReadDirectory(void);
	int WriteDirectory(void);

public:
	std::wstring( ProgramDataPath );

	QWORD DirectoryID;

	void *pSrcFS;

	FOPTranslateFunction ProcessFOP;

	std::map<DWORD, FOPReturn> FileFOPData;

	void Empty()
	{
		DirectoryID = 0;

		FileFOPData.clear();
		ResolvedIcons.clear();
	}

private:
	void ReadNativeFile( QWORD FileIndex, NativeFile *pFile );
	BYTEString ReadString( CTempFile &file );

	void WriteNativeFile( QWORD FileIndex, NativeFile *pFile );
	void WriteString( CTempFile &file, BYTEString &strng );

	void CleanFiles();
};

