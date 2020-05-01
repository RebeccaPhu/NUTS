#pragma once
#include "../NUTS/filesystem.h"
#include "d64directory.h"

class D64FileSystem :
	public FileSystem
{
public:
	D64FileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		pDirectory	= new D64Directory(pDataSource);

		FSID  = FSID_D64;
		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Spaces;
	}

	~D64FileSystem(void) {
		delete pDirectory;
	}

	static FileSystem *createNew(DataSource *pSource) {
		return (FileSystem *) new D64FileSystem(pSource);
	}

	int	ReadFile(DWORD FileID, CTempFile &store);
	int	WriteFile(NativeFile *pFile, CTempFile &store, char *Filename);

	BYTE *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *GetTitleString( NativeFile *pFile );

	AttrDescriptors D64FileSystem::GetAttributeDescriptions( void );

	DWORD GetEncoding(void )
	{
		return ENCODING_PETSCII;
	}

	FSHint Offer( BYTE *Extension );

	int MakeASCIIFilename( NativeFile *pFile );
};
