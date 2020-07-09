#pragma once

#include "e:\nuts\nuts\filesystem.h"
#include "T64Directory.h"

#include "CBMDefs.h"

#define T64_REGEN_WRITE  0
#define T64_REGEN_DELETE 1
#define T64_REGEN_SWAP   2

class T64FileSystem :
	public FileSystem
{
public:
	T64FileSystem( DataSource *pDataSource) : FileSystem( pDataSource )
	{
		pDir = new T64Directory( pDataSource );

		pDirectory = (Directory *) pDir;

		FSID  = FSID_T64;
		Flags = FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_DynamicSize | FSF_Reorderable | FSF_Prohibit_Nesting;
	}

	~T64FileSystem(void)
	{
	}

public:
	FSHint Offer( BYTE *Extension );
	DWORD  GetEncoding(void ) { return ENCODING_PETSCII; }
	int    ReadFile(DWORD FileID, CTempFile &store);
	int    WriteFile(NativeFile *pFile, CTempFile &store);
	int    DeleteFile( NativeFile *pFile, int FileOp );
	int    SwapFile( DWORD FileID1, DWORD FileID2 );
	int    Rename( DWORD FileID, BYTE *NewName );

	BYTE   *GetTitleString( NativeFile *pFile = nullptr );
	BYTE   *DescribeFile( DWORD FileIndex );
	BYTE   *GetStatusString( int FileIndex, int SelectedItems );
	WCHAR  *Identify( DWORD FileID );

	int    MakeASCIIFilename( NativeFile *pFile );

	std::vector<AttrDesc> GetAttributeDescriptions( void );
	std::vector<AttrDesc> GetFSAttributeDescriptions( void );
	int    SetProps( DWORD FileID, NativeFile *Changes );
	int    SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );

	int    RegenerateSource( DWORD id1, DWORD id2, NativeFile *pIncoming, CTempFile *pStore, int Reason );

	int    Format_Process( FormatType FT, HWND hWnd );

private:
	T64Directory *pDir;
};

