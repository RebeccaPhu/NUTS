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
	BYTE *GetTitleString( NativeFile *pFile, DWORD Flags);

	int	ReadFile(DWORD FileID, CTempFile &store);
	int	WriteFile(NativeFile *pFile, CTempFile &store);
	int DeleteFile( DWORD FileID );
	int Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  );
	BYTE *DescribeFile( DWORD FileIndex );

	AttrDescriptors GetAttributeDescriptions( NativeFile *pFile = nullptr );

	int SwapFile( DWORD FileID1, DWORD FileID2 );
	int SetProps( DWORD FileID, NativeFile *Changes );

	int MakeAudio( std::vector<NativeFile> &Selection, TapeIndex &indexes, CTempFile &store );

private:
	BYTE TAPSum( BYTE *Blk, DWORD Bytes );

	int RewriteTAPFile( DWORD SpecialID, DWORD SwapID, BYTE *pName, int Reason );

	int	WriteAtStore(NativeFile *pFile, CTempFile &store, CTempFile *output, DWORD Offset);

	inline void MakeAudioPulses( DWORD PulseWidth, DWORD PulseCycles, CTempFile *output );
	inline void MakeAudioPulse( DWORD PulseWidth1, DWORD PulseWidth2, CTempFile *output );
	inline void MakeAudioBytes( BYTE *pData, DWORD lData, CTempFile *output );
	inline void MakePause( DWORD ms, CTempFile *output );
	inline void WriteAudio( CTempFile *output );

private:
	BYTE SignalLevel;

	BYTE   TapeAudio[ 16384 ];
	DWORD  TapePtr;
	DWORD  TapeLimit;
	double TapePartial;
	DWORD  AudioPtr;
};
