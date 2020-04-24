#pragma once

#include "../nuts/filesystem.h"
#include "SpriteFileDirectory.h"

class SpriteFile : public FileSystem
{
public:
	SpriteFile(DataSource *pDataSource) : FileSystem(pDataSource) {
		pSource = pDataSource;

		FSID  = FSID_SPRITE;
		Flags = FSF_Size;
	}

	~SpriteFile(void) {
		delete pSpriteDirectory;
	}

public:
	FSHint Offer( BYTE *Extension );

	int Init(void) {
		pSpriteDirectory = new SpriteFileDirectory( pSource );

		pSpriteDirectory->UseResolvedIcons = UseResolvedIcons;

		pDirectory = (Directory *) pSpriteDirectory;

		pDirectory->ReadDirectory();

		ResolveIcons();

		return 0;
	}

	bool IsRoot()
	{
		return true;
	}

	int ReadFile(DWORD FileID, CTempFile &store);

	BYTE *DescribeFile(DWORD FileIndex);
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *GetTitleString( NativeFile *pFile );

	virtual WCHAR *Identify( DWORD FileID )
	{
		static WCHAR *type = L"Sprite";

		return type;
	}

	int  Refresh( void )
	{
		pSpriteDirectory->UseResolvedIcons = UseResolvedIcons;

		pDirectory->ReadDirectory();

		ResolveIcons();

		return 0;
	}

	int  ResolveIcons( void );

	DWORD GetEncoding( void )
	{
		return ENCODING_ACORN;
	}

private:
	SpriteFileDirectory *pSpriteDirectory;
};

