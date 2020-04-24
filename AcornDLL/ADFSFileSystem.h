#pragma once
#include "../NUTS/filesystem.h"
#include "ADFSDirectory.h"
#include "OldFSMap.h"

#include "Defs.h"

#include <vector>


class ADFSFileSystem :
	public FileSystem
{
public:
	ADFSFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		pSource = pDataSource;

		FSID  = FSID_ADFS_H;
		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Dirs;

		rstrncpy( path, (BYTE *) "$", 512 );

		UseLFormat = false;
		UseDFormat = false;

		SecSize    = 256;

		pFSMap = nullptr;
	}

	ADFSFileSystem( const ADFSFileSystem &source ) : FileSystem( source.pSource )
	{
		pSource = source.pSource;
		FSID    = source.FSID;
		Flags   = source.Flags;

		rstrncpy( path, (BYTE *) source.path, 512 );
		rstrncpy( DiscName, (BYTE *) source.DiscName, 10 );

		pFSMap  = nullptr;

		UseLFormat = source.UseLFormat;
		UseDFormat = source.UseDFormat;

		pADFSDirectory = new ADFSDirectory( pSource );

		pADFSDirectory->Files = source.pADFSDirectory->Files;

		if ( pADFSDirectory->UseLFormat ) { source.pADFSDirectory->SetLFormat(); }
		if ( pADFSDirectory->UseDFormat ) { source.pADFSDirectory->SetDFormat(); }

		pFSMap = new OldFSMap( pSource );

		pDirectory = (Directory *) pADFSDirectory;

		pFSMap->ReadFSMap();

		pADFSDirectory->DirSector    = source.pADFSDirectory->DirSector;
		pADFSDirectory->ParentSector = source.pADFSDirectory->ParentSector;

		pDirectory->ReadDirectory();
	}

	~ADFSFileSystem(void) {
		delete pDirectory;
		delete pFSMap;
	}

	int  Init(void);
	int  ReadFile(DWORD FileID, CTempFile &store);
	int  WriteFile(NativeFile *pFile, CTempFile &store);
	int  ChangeDirectory( DWORD FileID );
	int  Parent();
	int  CreateDirectory( BYTE *Filename );
	bool IsRoot();
	int  CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );
	int  Refresh( void );

	DataSource *FileDataSource( DWORD FileID );

	AttrDescriptors GetAttributeDescriptions( void );
	AttrDescriptors GetFSAttributeDescriptions( void );

	void SetLFormat(void) {
		UseLFormat = true;

		FSID = FSID_ADFS_L;
	}

	void SetDFormat(void) {
		UseDFormat = true;
		SecSize    = 1024;

		FSID = FSID_ADFS_D;
	}

	DWORD GetEncoding(void )
	{
		return ENCODING_ACORN;
	}

	BYTE *ADFSFileSystem::GetTitleString( NativeFile *pFile );
	BYTE *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	WCHAR *Identify( DWORD FileID );

	OldFSMap *pFSMap;

	BYTE path[512];
	BYTE DiscName[ 11 ];

	FSHint Offer( BYTE *Extension );

	FSToolList GetToolsList( void );

private:
	DataSource *pSource;

	ADFSDirectory *pADFSDirectory;

	bool UseLFormat;
	bool UseDFormat;

	DWORD SecSize;

private:
	DWORD TranslateSector(DWORD InSector);

	int ResolveAppIcons( void );
};

