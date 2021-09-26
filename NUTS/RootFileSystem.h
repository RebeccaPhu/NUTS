#pragma once
#include "filesystem.h"
#include "RootDirectory.h"
#include "DataSource.h"

class RootFileSystem :
	public FileSystem
{
public:
	RootFileSystem(void) : FileSystem(NULL) {
		pRootDirectory = new RootDirectory();
		pDirectory     = (Directory *) pRootDirectory;

		pDirectory->ReadDirectory();

		FSID = FS_Root;

		TopicIcon = FT_System;

		Flags = FSF_Prohibit_Nesting;
	}

	~RootFileSystem(void) {
		delete pRootDirectory;
	}

	bool	IsRoot() {
		return false;
	}

	BYTE *GetTitleString( NativeFile *pFile = nullptr ) {
		static char *title = "My Computer";

		return (BYTE *) title;
	}

	BYTE  *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );

	DataSource *FileDataSource( DWORD FileID );
	FileSystem *FileFilesystem( DWORD FileID );

private:
	RootDirectory *pRootDirectory;
};
