#pragma once

#include "../nuts/filesystem.h"
#include "SpriteFileDirectory.h"

class SpriteFile : public FileSystem
{
public:
	SpriteFile(DataSource *pDataSource) : FileSystem(pDataSource) {
		pSource = pDataSource;

		FSID  = FSID_SPRITE;
		Flags = FSF_Size | FSF_Prohibit_Nesting;

		pSpriteDirectory = nullptr;
	}

	~SpriteFile(void) {
		if ( pSpriteDirectory != nullptr )
		{
			delete pSpriteDirectory;
		}
	}

public:
	FSHint Offer( BYTE *Extension );

	int Init(void) {
		pSpriteDirectory = new SpriteFileDirectory( pSource );

		pSpriteDirectory->UseResolvedIcons = UseResolvedIcons;

		pDirectory = (Directory *) pSpriteDirectory;

		FreeIcons();

		pDirectory->ReadDirectory();

		ResolveIcons();

		return 0;
	}

	int ReadFile(DWORD FileID, CTempFile &store);
	int WriteFile(NativeFile *pFile, CTempFile &store);

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

	int Rename( DWORD FileID, BYTE *NewName );

	int  ResolveIcons( void );
	int  FreeIcons( void );

	DWORD GetEncoding( void )
	{
		return ENCODING_RISCOS;
	}

	AttrDescriptors GetAttributeDescriptions( void );

	int Format_Process( FormatType FT, HWND hWnd );

private:
	SpriteFileDirectory *pSpriteDirectory;
};

