#pragma once

#include "../nuts/filesystem.h"
#include "NewFSMap.h"
#include "ADFSEDirectory.h"
#include "SpriteFile.h"
#include "ADFSCommon.h"

class ADFSEFileSystem : public FileSystem, ADFSCommon
{
public:
	ADFSEFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		FSID  = FSID_ADFS_H;
		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Dirs | FSF_Exports_Sidecars;

		rstrncpy( path, (BYTE *) "$", 512 );

		pEDirectory = nullptr;
		pDirectory  = nullptr;
		pFSMap      = nullptr;
	}

	ADFSEFileSystem( const ADFSEFileSystem &source ) : FileSystem( source.pSource )
	{
		pSource = source.pSource;
		FSID    = source.FSID;
		Flags   = source.Flags;

		rstrncpy( path, (BYTE *) source.path, 512 );

		pFSMap  = nullptr;

		pEDirectory = new ADFSEDirectory( pSource );

		pEDirectory->Files        = source.pEDirectory->Files;
		pEDirectory->DirSector    = source.pEDirectory->DirSector;
		pEDirectory->ParentSector = source.pEDirectory->ParentSector;

		pFSMap = new NewFSMap( pSource );

		pEDirectory->pMap = pFSMap;

		pDirectory = (Directory *) pEDirectory;

		pFSMap->ReadFSMap();

		pEDirectory->DirSector    = source.pEDirectory->DirSector;
		pEDirectory->ParentSector = source.pEDirectory->ParentSector;

		pDirectory->ReadDirectory();
	}

	~ADFSEFileSystem(void) {
		if ( pEDirectory != nullptr )
		{
			delete pEDirectory;
		}

		if ( pFSMap != nullptr )
		{
			delete pFSMap;
		}
	}

	NewFSMap *pFSMap;

public:
	ADFSEDirectory *pEDirectory;

	BYTE path[512];

	FSHint Offer( BYTE *Extension );

	int  Init(void);
	int  ChangeDirectory( DWORD FileID );
	int	 Parent();
	int  Refresh( void );

	bool IsRoot()
	{
		if ( pEDirectory->DirSector == pFSMap->RootLoc )
		{
			return true;
		}

		return false;
	}

	int ReadFile(DWORD FileID, CTempFile &store);
	int	WriteFile(NativeFile *pFile, CTempFile &store);
	int	CreateDirectory( BYTE *Filename, bool EnterAfter );
	int DeleteFile( NativeFile *pFile, int FileOp );

	BYTE *DescribeFile(DWORD FileIndex);
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *GetTitleString( NativeFile *pFile );

	FileSystem *FileFilesystem( DWORD FileID );
	WCHAR *Identify( DWORD FileID );
	int  CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );

	AttrDescriptors GetAttributeDescriptions( void );
	AttrDescriptors GetFSAttributeDescriptions( void );

	int ResolveAuxFileType( NativeFile *pSprite, NativeFile *pFile, SpriteFile &spriteFile );

	DWORD GetEncoding(void )
	{
		return ENCODING_RISCOS;
	}

	int SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );

	int Format_Process( FormatType FT, HWND hWnd );
	int Format_PreCheck( int FormatType, HWND hWnd );	

	bool IncludeEmuHeader;

	int SetProps( DWORD FileID, NativeFile *Changes );

	int ReplaceFile(NativeFile *pFile, CTempFile &store);

private:
	TargetedFileFragments FindSpace( DWORD Length, bool ForDir );
};

