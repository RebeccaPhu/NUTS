#pragma once

#include "../nuts/filesystem.h"
#include "T64Directory.h"

#include "CBMDefs.h"

#define T64_REGEN_WRITE  0
#define T64_REGEN_DELETE 1
#define T64_REGEN_SWAP   2

#define T64_TAPEAUDIO_BUFFER_SIZE 16384

class T64FileSystem :
	public FileSystem
{
public:
	T64FileSystem( DataSource *pDataSource) : FileSystem( pDataSource )
	{
		pDir = new T64Directory( pDataSource );

		pDirectory = (Directory *) pDir;

		FSID  = FSID_T64;
		Flags = FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_DynamicSize | FSF_Reorderable | FSF_Prohibit_Nesting | FSF_Uses_Extensions | FSF_Fake_Extensions;

		PreferredArbitraryExtension = (BYTE *) "PRG";
	}

	~T64FileSystem(void)
	{
		delete pDir;
	}

public:
	FSHint Offer( BYTE *Extension );
	DWORD  GetEncoding(void ) { return ENCODING_PETSCII; }
	int    ReadFile(DWORD FileID, CTempFile &store);
	int    WriteFile(NativeFile *pFile, CTempFile &store);
	int    DeleteFile( DWORD FileID );
	int    SwapFile( DWORD FileID1, DWORD FileID2 );
	int    Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  );

	BYTE   *GetTitleString( NativeFile *pFile, DWORD Flags );
	BYTE   *DescribeFile( DWORD FileIndex );
	BYTE   *GetStatusString( int FileIndex, int SelectedItems );
	WCHAR  *Identify( DWORD FileID );

	int    MakeASCIIFilename( NativeFile *pFile );

	std::vector<AttrDesc> GetAttributeDescriptions( NativeFile *pFile = nullptr );
	std::vector<AttrDesc> GetFSAttributeDescriptions( void );
	int    SetProps( DWORD FileID, NativeFile *Changes );
	int    SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );

	int    RegenerateSource( DWORD id1, DWORD id2, NativeFile *pIncoming, CTempFile *pStore, int Reason );

	int    Format_Process( DWORD FT, HWND hWnd );

	int    MakeAudio( std::vector<NativeFile> &Selection, TapeIndex &indexes, CTempFile &store );

private:
	void   WriteShortPulse( CTempFile &output );
	void   WriteMediumPulse( CTempFile &output );
	void   WriteLongPulse( CTempFile &output );
	void   WriteAudio( CTempFile &output, BYTE sig, DWORD Length );
	void   WriteAudio0( CTempFile &output )
		{
			WriteShortPulse( output );
			WriteMediumPulse( output );
		}

	void   WriteAudio1( CTempFile &output )
		{
			WriteMediumPulse( output );
			WriteShortPulse( output );
		}

	void   WriteAudioDM( CTempFile &output )
		{
			WriteLongPulse( output );
			WriteMediumPulse( output );
		}

	void   WriteAudioED( CTempFile &output )
		{
			WriteLongPulse( output );
			WriteShortPulse( output );
		}

	void   WriteAudioBuffer( CTempFile &output, BYTE *pBuffer, DWORD Length );
private:
	T64Directory *pDir;

	BYTE   TapeLevel;
	DWORD  TapePtr;
	DWORD  TapeBufferPtr;
	BYTE   TapeBuffer[ T64_TAPEAUDIO_BUFFER_SIZE ];
	BYTE   TapeChecksum;
};

