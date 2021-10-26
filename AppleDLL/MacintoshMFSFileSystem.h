#pragma once
#include "..\nuts\filesystem.h"
#include "..\NUTS\DataSource.h"

#include "MacintoshMFSDirectory.h"
#include "MFSFAT.h"
#include "..\NUTS\libfuncs.h"

#include "AppleDefs.h"

class MacintoshMFSFileSystem :
	public FileSystem
{
public:
	MacintoshMFSFileSystem( DataSource *pSource );
	~MacintoshMFSFileSystem(void);

private:
	void SetShape();
	void ReadVolumeRecord();
	int  WriteVolumeRecord();
	int  WriteForkData( CTempFile &store, std::vector<WORD> *pBlocks );
	void CleanupImportForks();

private:
	MFSVolumeRecord VolRecord;

	MacintoshMFSDirectory *pMacDirectory;
	
	MFSFAT *pFAT;

	CTempFile *OverrideFork;
	AutoBuffer *OverrideFinder;

public:
	int Init(void);
	FSHint Offer( BYTE *Extension );
	BYTE *GetTitleString( NativeFile *pFile, DWORD Flags );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	std::vector<AttrDesc> GetAttributeDescriptions( void );
	int ReadFile(DWORD FileID, CTempFile &store);
	int CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );
	AttrDescriptors MacintoshMFSFileSystem::GetFSAttributeDescriptions( void );
	int SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );
	FSToolList GetToolsList( void );
	int RunTool( BYTE ToolNum, HWND ProgressWnd );
	BYTE *DescribeFile( DWORD FileIndex );
	int ReadFork( DWORD FileID, WORD ForkID, CTempFile &forkObj );
	int SetProps( DWORD FileID, NativeFile *Changes );
	int DeleteFile( DWORD FileID );
	int WriteFile(NativeFile *pFile, CTempFile &store);
	LocalCommands GetLocalCommands( void );
	int ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd );
	int ExportSidecar( NativeFile *pFile, SidecarExport &sidecar );
	int ImportSidecar( NativeFile *pFile, SidecarImport &sidecar, CTempFile *obj );
	int	Format_PreCheck( int FormatType, HWND hWnd );
	int Format_Process( DWORD FT, HWND hWnd );
	int WriteCleanup( void );

	DWORD GetEncoding(void )
	{
		return ENCODING_MACINTOSH;
	}

	int MakeASCIIFilename( NativeFile *pFile )
	{
		// No conversion necessary, so just change the encoding directly
		pFile->EncodingID = ENCODING_ASCII;

		return 0;
	}

	int	ReplaceFile(NativeFile *pFile, CTempFile &store)
	{
		pFile->Length = store.Ext();

		// Just delete and store
		NativeFile newFile = *pFile;

		// As we have forks, we need to deal with this.
		CTempFile forkObj;

		ReadFork( pFile->fileID, 0, forkObj );

		int r = FileSystem::DeleteFile( pFile );

		if ( r == NUTS_SUCCESS )
		{
			r = WriteFork( 0, &forkObj );
		}

		if ( r == NUTS_SUCCESS )
		{
			r = WriteFile( &newFile, store );

			WriteCleanup();
		}

		return r;
	}

public:
	static INT_PTR CALLBACK FinderWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK FormatWindowProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

