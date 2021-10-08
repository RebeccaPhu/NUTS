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
		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Dirs | FSF_Exports_Sidecars;

		strncpy_s(path, 8192, "$", 8192);

		Override = false;

		/* Set the disk shape to 40 track for now, it may get reset when initialised properly */
		DiskShape shape;

		shape.Heads            = 1;
		shape.InterleavedHeads = false;
		shape.Sectors          = 10;
		shape.SectorSize       = 256;
		shape.Tracks           = 40;

		pSource->SetDiskShape( shape );
	}

	~AcornDFSFileSystem(void) {
		delete pDFSDirectory;
	}

	int Init( void )
	{
		DiskShape shape;

		shape.Heads            = 1;
		shape.InterleavedHeads = false;
		shape.Sectors          = 10;
		shape.SectorSize       = 256;
		shape.Tracks           = 40;

		if ( MYFSID == FSID_DFS_80 )
		{
			shape.Tracks = 80;
		}

		pSource->SetDiskShape( shape );

		return FileSystem::Init();
	}

	int	 ReadFile(DWORD FileID, CTempFile &store);
	int  WriteFile(NativeFile *pFile, CTempFile &store);
	int  ReplaceFile(NativeFile *pFile, CTempFile &store);
	int  DeleteFile( DWORD FileID );
	int  ChangeDirectory( DWORD FileID );
	int	 Parent();
	int	 CreateDirectory( NativeFile *pDir, DWORD CreateFlags );
	bool IsRoot();
	BYTE *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );

	BYTE *GetTitleString( NativeFile *pFile = nullptr ) {
		static BYTE title[512];

		if ( ( pParentFS->FSID & 0xFFFF ) == FSID_DFS_DSD )
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

	int Format_Process( DWORD FT, HWND hWnd );

	int SetProps( DWORD FileID, NativeFile *Changes )
	{
		NativeFileIterator iFile;

		for ( iFile = pDFSDirectory->RealFiles.begin(); iFile != pDFSDirectory->RealFiles.end(); iFile++ )
		{
			if ( iFile->fileID = FileID )
			{
				for ( BYTE i=0; i<16; i++ )
				{
					iFile->Attributes[ i ] = Changes->Attributes[ i ];
				}
			}
		}

		pDirectory->WriteDirectory();
		pDirectory->ReadDirectory();

		return 0;
	}

	int SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );

	int ExportSidecar( NativeFile *pFile, SidecarExport &sidecar );
	int ImportSidecar( NativeFile *pFile, SidecarImport &sidecar, CTempFile *obj );

	FSToolList GetToolsList( void );
	int RunTool( BYTE ToolNum, HWND ProgressWnd );
	int Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  );

private:
	AcornDFSDirectory *pDFSDirectory;

	NativeFile OverrideFile;
	bool Override;
};
