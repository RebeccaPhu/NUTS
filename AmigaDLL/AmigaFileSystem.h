#pragma once

#include "AmigaDirectory.h"
#include "../NUTS/FileSystem.h"
#include "../NUTS/Defs.h"
#include "Defs.h"

class AmigaFileSystem :
	public FileSystem
{
public:
	AmigaFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		pAmigaDirectory	= new AmigaDirectory(pDataSource);
		pDirectory      = (Directory *) pAmigaDirectory;

		BYTE Header[4];

		pSource->ReadRaw( 0, 4, Header );

		if ( Header[ 3] & 1 )
		{
			FSID = FSID_AMIGAF;
		}
		else
		{
			FSID = FSID_AMIGAO;
		}

		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Spaces;

		path = "/";

		BYTE HTSector[ 512 ];

		pSource->ReadSectorLBA( pAmigaDirectory->DirSector, HTSector, 512 );

		VolumeName = rstrndup( &HTSector[ 0x1B1 ], HTSector[ 0x1B0 ] );
	}

	~AmigaFileSystem(void)
	{
		delete pAmigaDirectory;
	}

public:
	FSHint Offer( BYTE *Extension );

	int	ReadFile(DWORD FileID, CTempFile &store);

	bool IsRoot() {
		return ( pAmigaDirectory->DirSector == 880 ) ;
	}

	int ChangeDirectory( DWORD FileID );
	int Parent();
	DWORD GetEncoding( void ) { return ENCODING_AMIGA; }

	int Init(void) {
		pDirectory->ReadDirectory();

		ResolveIcons();

		return 0;
	}

	int Refresh( void )
	{
		pDirectory->ReadDirectory();

		ResolveIcons();

		return 0;
	}	

	BYTE *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *GetTitleString( NativeFile *pFile );

	AttrDescriptors GetAttributeDescriptions( void );

private:
	int ReadFileBlocks( DWORD FileHeaderBlock, CTempFile &store );

	DWORD ReadingLength;

	AmigaDirectory *pAmigaDirectory;

	void ResolveIcons( void );

	std::string path;

	BYTE *VolumeName;
};

