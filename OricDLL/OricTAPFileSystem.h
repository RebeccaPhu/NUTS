#pragma once

#include "..\nuts\filesystem.h"
#include "OricTAPDirectory.h"
#include "OricDefs.h"

#define ORIC_TAPE_BUFFER_SIZE 32768

class OricTAPFileSystem :
	public FileSystem
{
public:
	OricTAPFileSystem( DataSource *pSrc );
	~OricTAPFileSystem( void );

public:
	FSHint Offer( BYTE *Extension );

	std::vector<AttrDesc> GetAttributeDescriptions( NativeFile *pFile = nullptr );

	int  ReadFile(DWORD FileID, CTempFile &store);
	int  WriteFile(NativeFile *pFile, CTempFile &store);
	int  SwapFile( DWORD FileID1, DWORD FileID2 );
	int  DeleteFile( DWORD FileID );
	int  Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  );
	int  SetProps( DWORD FileID, NativeFile *Changes );
	BYTE *OricTAPFileSystem::GetTitleString( NativeFile *pFile, DWORD Flags );
	BYTE *OricTAPFileSystem::DescribeFile( DWORD FileIndex );
	BYTE *OricTAPFileSystem::GetStatusString( int FileIndex, int SelectedItems );
	int  MakeAudio( std::vector<NativeFile> &Selection, TapeIndex &indexes, CTempFile &store );

	EncodingIdentifier GetEncoding(void )
	{
		return ENCODING_ORIC;
	}

private:
	int RewriteTAPFile( DWORD SpecialID, DWORD SwapID, BYTE *pName, int Reason );
	int	WriteAtStore(NativeFile *pFile, CTempFile &store, CTempFile *output, DWORD Offset);

	void WriteAudioBytes( BYTE *buf, DWORD Sz, CTempFile &store, bool FE );
	void WriteAudioBit( CTempFile &store, BYTE bit, bool FE );
	void WriteAudioLevel( CTempFile &store, BYTE level, double Len );
	void WriteAudioBuffer( CTempFile &store, BYTE *b, DWORD Sz );

private:
	OricTAPDirectory *pOricDir;

	double TapePartial;

	BYTE TapeBuffer[ ORIC_TAPE_BUFFER_SIZE ];

	DWORD TapePtr;
	DWORD TapeFilePtr;
};

