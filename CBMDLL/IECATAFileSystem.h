#pragma once
#include "../NUTS/filesystem.h"
#include "iecatadirectory.h"

#include <map>

class IECATAFileSystem :
	public FileSystem
{
public:
	IECATAFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		pIECDirectory = new IECATADirectory(pDataSource);
		pDirectory    = (Directory *) pIECDirectory;
		DirSector     = 1;

		pIECDirectory->SetDirSector( 1U );

		ParentLinks.clear();
		ParentLinks[ 1 ] = 0;

		pSource = pDataSource;

		FSID  = FSID_IECATA;
		Flags = 
			FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity |
			FSF_Supports_Dirs | FSF_Supports_Spaces |
			FSF_Uses_Extensions | FSF_Fake_Extensions | FSF_NoDir_Extensions;

		TopicIcon = FT_HardImage;
	}

	~IECATAFileSystem(void) {
		delete pIECDirectory;
	}

	static FileSystem *createNew(DataSource *pSource) {
		return (FileSystem *) new IECATAFileSystem(pSource);
	}

	int	ReadFile( DWORD FileID, CTempFile &store );
	int	WriteFile( NativeFile *pFile, CTempFile &store );

	BYTE *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *GetTitleString( NativeFile *pFile );
	WCHAR *Identify( DWORD FileID );

	int ChangeDirectory(NativeFile *pFile);
	int Parent();
	int CreateDirectory( NativeFile *pDir, DWORD CreateFlags );
	bool IsRoot();
	int DeleteFile( DWORD FileID );

	int Format_PreCheck(int FormatType, HWND hWnd);
	int Format_Process( FormatType FT, HWND hWnd );
	int CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );

	int MakeASCIIFilename( NativeFile *pFile );

	FSHint Offer( BYTE *Extension );

	DWORD GetEncoding(void )
	{
		return ENCODING_PETSCII;
	}

	std::vector<AttrDesc> GetAttributeDescriptions( void );

private:
	IECATADirectory *pIECDirectory;

	std::map< DWORD, DWORD > ParentLinks;

	DWORD DirSector;

	DataSource *pSource;

private:
	int	ReleaseBlocks( NativeFile *pFile );
};