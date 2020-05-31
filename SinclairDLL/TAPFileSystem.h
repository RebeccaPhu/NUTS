#pragma once

#include "../NUTS/FileSystem.h"

#include "TAPDirectory.h"

#define REASON_DELETE 0
#define REASON_RENAME 1
#define REASON_SWAP   2
#define REASON_PROPS  3

class TAPFileSystem :
	public FileSystem
{
public:
	TAPFileSystem(DataSource *pDataSource);
	~TAPFileSystem(void);

public:
	FSHint Offer( BYTE *Extension );

	int Init(void) {
		pDirectory	= (Directory *) new TAPDirectory( pSource );

		pDirectory->ReadDirectory();
		
		return 0;
	}

	DWORD GetEncoding(void )
	{
		return ENCODING_SINCLAIR;
	}

	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *GetTitleString( NativeFile *pFile);

	int	ReadFile(DWORD FileID, CTempFile &store);
	int	WriteFile(NativeFile *pFile, CTempFile &store);
	int DeleteFile( NativeFile *pFile, int FileOp );
	int Rename( DWORD FileID, BYTE *NewName );
	BYTE *DescribeFile( DWORD FileIndex );

	AttrDescriptors GetAttributeDescriptions( void );

	int SwapFile( DWORD FileID1, DWORD FileID2 );
	int SetProps( DWORD FileID, NativeFile *Changes );

private:
	BYTE TAPSum( BYTE *Blk, DWORD Bytes );

	int RewriteTAPFile( DWORD SpecialID, DWORD SwapID, BYTE *pName, int Reason );

	int	WriteAtStore(NativeFile *pFile, CTempFile &store, CTempFile *output, DWORD Offset);
};
