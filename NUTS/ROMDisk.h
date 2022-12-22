#pragma once

#include "FileSystem.h"
#include "ROMDiskDirectory.h"
#include <Shlwapi.h>
#include <ShlObj.h>

#define FSID_ROMDisk L"ROMDisk_ScratchPad"

class ROMDisk :
	public FileSystem
{

public:
	ROMDisk( DataSource *pDataSource) : FileSystem( pDataSource )
	{
		pROMDiskDir = new ROMDiskDirectory( pDataSource );

		pDirectory = (Directory *) pROMDiskDir;

		FSID = FSID_ROMDisk;

		TopicIcon = FT_ROMDisk;

		// Get the program data path.
		WCHAR Path[MAX_PATH];

		if ( SUCCEEDED( SHGetFolderPath( NULL, CSIDL_COMMON_APPDATA, NULL, 0, Path ) ) )
		{
			ProgramDataPath = std::wstring( Path ) + L"\\NUTS";
		}

		pROMDiskDir->ProgramDataPath = ProgramDataPath;

		pROMDiskDir->pSrcFS     = (void *) this;

		VirtualPath = L"/";

		PathStack.push_back( std::make_pair<DWORD, std::wstring>( 0, VirtualPath ) );

		Flags =
			FSF_Supports_Spaces | FSF_Supports_Dirs |
			FSF_SupportFreeSpace | FSF_Capacity |
			FSF_Uses_Extensions |
			FSF_Supports_FOP
		;
	}

	ROMDisk( ROMDisk &source ) : FileSystem( source.pSource )
	{
		pROMDiskDir = new ROMDiskDirectory( source.pSource );

		pDirectory = (Directory *) pROMDiskDir;

		FSID = FSID_ROMDisk;

		TopicIcon = FT_ROMDisk;

		ProgramDataPath = source.ProgramDataPath;

		pROMDiskDir->ProgramDataPath = ProgramDataPath;

		pROMDiskDir->pSrcFS = (void *) this;

		VirtualPath = source.VirtualPath;
		PathStack   = source.PathStack;

		Flags =
			FSF_Supports_Spaces | FSF_Supports_Dirs |
			FSF_SupportFreeSpace | FSF_Capacity |
			FSF_Uses_Extensions |
			FSF_Supports_FOP
		;

		pDirectory->Files = source.pDirectory->Files;

		ProcessFOP = source.ProcessFOP;
		LoadFOPFS  = source.LoadFOPFS;

		pROMDiskDir->ProcessFOP = ProcessFOP;
	}

	~ROMDisk(void)
	{
		delete pROMDiskDir;

		pROMDiskDir = nullptr;
		pDirectory  = nullptr;
	}

public:
	int   Init(void);
	int   ReadFile(DWORD FileID, CTempFile &store);
	int   ReadFork( DWORD FileID, WORD ForkID, CTempFile &forkObj );
	int   WriteFile(NativeFile *pFile, CTempFile &store);
	BYTE  *GetTitleString( NativeFile *pFile, DWORD Flags );
	BYTE  *DescribeFile( DWORD FileIndex );
	BYTE  *GetStatusString( int FileIndex, int SelectedItems );
	int   CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );
	WCHAR *Identify( DWORD FileID );
	int   CreateDirectory( NativeFile *pDir, DWORD CreateFlags );
	int   ChangeDirectory( DWORD FileID );
	int   Parent();
	int   SetProps( DWORD FileID, NativeFile *Changes );
	int   Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt );
	int   DeleteFile( DWORD FileID );
	int   ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd );
	int   ReplaceFile(NativeFile *pFile, CTempFile &store);

	bool IsRoot()
	{
		if ( pROMDiskDir->DirectoryID == 0U )
		{
			return true;
		}

		return false;
	}

	FileSystem *Clone( void )
	{
		return (FileSystem *) new ROMDisk( *this );
	}

	int DeleteFile( NativeFile *pFile )
	{
		// This is called by FileOps and NewImageWizard to delete a conflicting image.
		// Since we don't do filename clashes, this is a no-op and in theory, should
		// never be called.

		return NUTS_SUCCESS;
	}

	std::vector<AttrDesc> ROMDisk::GetAttributeDescriptions( NativeFile *pFile );

	FileSystem *FileFilesystem( DWORD FileID );

	LocalCommands GetLocalCommands( void );

private:
	ROMDiskDirectory *pROMDiskDir;

	std::wstring ProgramDataPath;

	std::wstring VirtualPath;

	std::deque<std::pair<DWORD, std::wstring>> PathStack;
};

class CROMDiskLock 
{
public:
	CROMDiskLock()
	{
		InitializeCriticalSection( &RDLock );

		// Do this here so we don't need links to all sorts.
		WCHAR Path[MAX_PATH];

		if ( SUCCEEDED( SHGetFolderPath( NULL, CSIDL_COMMON_APPDATA, NULL, 0, Path ) ) )
		{
			ProgramDataPath = std::wstring( Path ) + L"\\NUTS";
		}
	}

	~CROMDiskLock()
	{
		DeleteCriticalSection( &RDLock );
	}

public:
	DWORD GetNextFileIndex()
	{
		EnterCriticalSection( &RDLock );

		DWORD index = 0;

		std::wstring FileIndexFile = ProgramDataPath + L"\\DirectoryIndex";

		CTempFile FileIndex( FileIndexFile );

		FileIndex.Keep();

		FileIndex.Seek( 0 );

		FileIndex.Read( (void *) &index, sizeof( DWORD ) );

		index++;

		FileIndex.Seek( 0 );

		FileIndex.Write( (void *) &index, sizeof( DWORD ) );

		LeaveCriticalSection( &RDLock );

		return index;
	}

private:
	CRITICAL_SECTION RDLock;

	std::wstring ProgramDataPath;
};

extern CROMDiskLock ROMDiskLock;