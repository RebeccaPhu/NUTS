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
	int	CreateDirectory(BYTE *Filename);

	bool IsRoot() {
		if ( folderPath.length() < 4)
		{
			return true;
		}

		return false;
	}

	DataSource *FileDataSource( DWORD FileID );

	BYTE *GetTitleString( NativeFile *pFile = nullptr );

	BYTE *GetStatusString(int FileIndex) {
		static BYTE status[ 128 ];

		if ( pDirectory->Files[FileIndex].Flags & FF_Directory )
			rsprintf( status, "%s - Folder", pDirectory->Files[FileIndex].Filename );
		else
			rsprintf( status, "%s - %d bytes", pDirectory->Files[FileIndex].Filename, pDirectory->Files[FileIndex].Length );

		return status;
	}

private:
	std::wstring folderPath;

	WindowsDirectory *pWindowsDirectory;
};
