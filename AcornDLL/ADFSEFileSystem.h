#pragma once

#include "../nuts/filesystem.h"
#include "NewFSMap.h"
#include "ADFSEDirectory.h"
#include "SpriteFile.h"

class ADFSEFileSystem : public FileSystem
{
public:
	ADFSEFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		pSource = pDataSource;

		FSID  = FSID_ADFS_H;
		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Dirs;

		rstrncpy( path, (BYTE *) "$", 512 );

		pFSMap = nullptr;
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
		delete pDirectory;
		delete pFSMap;
	}

public:
	ADFSEDirectory *pEDirectory;

	BYTE path[512];

	FSHint Offer( BYTE *Extension );

	int Init(void) {
		pEDirectory	= new ADFSEDirectory( pSource );
		pDirectory = (Directory *) pEDirectory;

		pFSMap  = new NewFSMap( pSource );

		pEDirectory->pMap = pFSMap;

		pFSMap->ReadFSMap();

		pEDirectory->DirSector = pFSMap->RootLoc;

		pDirectory->ReadDirectory();

		ResolveAppIcons();

		return 0;
	}

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

	BYTE *DescribeFile(DWORD FileIndex);
	BYTE *GetStatusString(int FileIndex);
	BYTE *GetTitleString( NativeFile *pFile );

	DataSource *FileDataSource( DWORD FileID );
	FileSystem *FileFilesystem( DWORD FileID );
	WCHAR *Identify( DWORD FileID );
	int  CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );

	AttrDescriptors GetAttributeDescriptions( void );
	AttrDescriptors GetFSAttributeDescriptions( void );

	int ResolveAppIcons( void );
	int ResolveAuxFileType( NativeFile *pSprite, NativeFile *pFile, SpriteFile &spriteFile );

	DWORD GetEncoding(void )
	{
		return ENCODING_ACORN;
	}

private:
	NewFSMap *pFSMap;
};

