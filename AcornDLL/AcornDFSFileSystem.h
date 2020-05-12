#pragma once
#include "../NUTS/filesystem.h"
#include "AcornDFSDirectory.h"

#include "Defs.h"

class AcornDFSFileSystem :
	public FileSystem
{
public:
	AcornDFSFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		pDFSDirectory	= new AcornDFSDirectory(pDataSource);
		pDirectory      = (Directory *) pDFSDirectory;

		FSID  = FSID_DFS_80;
		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Dirs;

		strncpy_s(path, 8192, "$", 8192);
	}

	~AcornDFSFileSystem(void) {
		delete pDFSDirectory;
	}

	int	 ReadFile(DWORD FileID, CTempFile &store);
	int  WriteFile(NativeFile *pFile, CTempFile &store);
	int  DeleteFile( NativeFile *pFile, int FileOp );
	int  ChangeDirectory( DWORD FileID );
	int	 Parent();
	int	 CreateDirectory( BYTE *Filename, bool EnterAfter);
	bool IsRoot();
	BYTE *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );

	BYTE *GetTitleString( NativeFile *pFile = nullptr ) {
		static BYTE title[512];

		if ( pParentFS->FSID == FSID_DFS_DSD )
		{
			rsprintf( title, ".%c", pDFSDirectory->CurrentDir );
		}
		else
		{
			rsprintf( title, "DFS::0.%c", pDFSDirectory->CurrentDir );
		}

		return title;
	}
	int CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );

	AttrDescriptors GetFSAttributeDescriptions( void );
	AttrDescriptors GetAttributeDescriptions( void );

	DWORD GetEncoding(void )
	{
		return ENCODING_ACORN;
	}

	char	path[8192];

	FSHint Offer( BYTE *Extension );

	int Format_Process( FormatType FT, HWND hWnd );

private:
	AcornDFSDirectory *pDFSDirectory;
};
