#pragma once
#include "filesystem.h"
#include "WindowsDirectory.h"

class WindowsFileSystem :
	public FileSystem
{
public:
	WindowsFileSystem( std::wstring rootDrive );
	~WindowsFileSystem(void);

public:
	int	ReadFile(DWORD FileID, CTempFile &store);
	int	WriteFile(NativeFile *pFile, CTempFile &store);
	int ChangeDirectory(DWORD FileID);
	int	Parent();
	int	CreateDirectory( NativeFile *pDir, DWORD CreateFlags );

	bool IsRoot() {
		if ( folderPath.length() < 4)
		{
			return true;
		}

		return false;
	}

	DataSource *FileDataSource( DWORD FileID );

	BYTE *GetTitleString( NativeFile *pFile = nullptr );

	BYTE *GetStatusString( int FileIndex, int SelectedItems )
	{
		static BYTE status[64];

		if ( SelectedItems == 0 )
		{
			rsprintf( status, "%d File System Objects", pDirectory->Files.size() );
		}
		else if ( SelectedItems > 1 )
		{
			rsprintf( status, "%d Items Selected", SelectedItems );
		}
		else if ( pDirectory->Files[FileIndex].Flags & FF_Directory )
			rsprintf( status, "%s - Folder", (BYTE *) pDirectory->Files[FileIndex].Filename );
		else
			rsprintf( status, "%s - %d bytes", (BYTE *) pDirectory->Files[FileIndex].Filename, pDirectory->Files[FileIndex].Length );

		return status;
	}

private:
	std::wstring folderPath;

public:
	WindowsDirectory *pWindowsDirectory;
};
