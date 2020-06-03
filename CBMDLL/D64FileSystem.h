#pragma once
#include "../NUTS/filesystem.h"
#include "d64directory.h"
#include "BAM.h"

class D64FileSystem :
	public FileSystem
{
public:
	D64FileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		pBAM        = new BAM( pDataSource );
		pDir        = new D64Directory( pDataSource );
		pDir->pBAM  = pBAM;
		pDirectory	= ( Directory * ) pDir;

		FSID  = FSID_D64;
		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Spaces | FSF_Reorderable;
	}

	~D64FileSystem(void) {
		delete pDirectory;
		delete pBAM;
	}

	virtual int Init(void) {
		pBAM->ReadBAM();
		pDirectory->ReadDirectory();

		return 0;
	}

	int ReadFile(DWORD FileID, CTempFile &store);
	int WriteFile(NativeFile *pFile, CTempFile &store);

	BYTE *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *GetTitleString( NativeFile *pFile );

	AttrDescriptors GetAttributeDescriptions( void );
	AttrDescriptors GetFSAttributeDescriptions( void );

	int  SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );

	int  Format_Process( FormatType FT, HWND hWnd );

	DWORD GetEncoding(void )
	{
		return ENCODING_PETSCII;
	}

	FSHint Offer( BYTE *Extension );

	int MakeASCIIFilename( NativeFile *pFile );

	int CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );
	int ReplaceFile(NativeFile *pFile, CTempFile &store);
	int DeleteFile( NativeFile *pFile, int FileOp );

	FSToolList GetToolsList( void );
	int RunTool( BYTE ToolNum, HWND ProgressWnd );

private:
	BAM *pBAM;

	D64Directory *pDir;

	int MarkChain( TSLink Loc );
};
