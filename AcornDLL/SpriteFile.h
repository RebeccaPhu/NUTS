#pragma once

#include "../nuts/filesystem.h"
#include "SpriteFileDirectory.h"
#include "AcornDLL.h"
#include "RISCOSIcons.h"

class SpriteFile : public FileSystem
{
public:
	SpriteFile(DataSource *pDataSource) : FileSystem(pDataSource) {
		pSource = pDataSource;

		FSID  = FSID_SPRITE;
		Flags = FSF_Size | FSF_Prohibit_Nesting;

		pSpriteDirectory = nullptr;

		TopicIcon = RISCOSIcons::GetIconForType( 0xFF9 );
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
	BYTE *GetTitleString( NativeFile *pFile, DWORD Flags );

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

	int Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  );

	int  ResolveIcons( void );
	int  FreeIcons( void );

	EncodingIdentifier GetEncoding( void )
	{
		return ENCODING_RISCOS;
	}

	AttrDescriptors GetAttributeDescriptions( NativeFile *pFile = nullptr );

	int Format_Process( DWORD FT, HWND hWnd );

private:
	SpriteFileDirectory *pSpriteDirectory;
};

