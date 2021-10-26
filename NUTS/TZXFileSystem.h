#pragma once

#include "TZXDirectory.h"
#include "../nuts/filesystem.h"

#include "../TZXConfig/TZXConfig.h"

#ifndef TZX_REGEN

#define TZX_INSERT  0
#define TZX_REPLACE 1
#define TZX_SWAP    2
#define TZX_DELETE  3
#define TZX_REGEN   4

#endif

class TZXFileSystem :
	public FileSystem
{
public:
	TZXFileSystem( DataSource *pDataSource, TZXPB &pb ) : FileSystem( pDataSource )
	{
		pDir = new TZXDirectory( pDataSource, pb );

		pDirectory = (Directory *) pDir;

		FSID = pb.FSID;

		tzxpb = pb;

		Flags = FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize | FSF_Reorderable | FSF_Prohibit_Nesting;
	}

	~TZXFileSystem(void)
	{
		delete pDir;
	}

	virtual FSHint Offer( BYTE *Extension );
	virtual DWORD GetEncoding(void ) { return tzxpb.Encoding; }

	virtual int ReadFile(DWORD FileID, CTempFile &store);
	virtual int ReadBlock(DWORD FileID, CTempFile &store);
	virtual int WriteFile(NativeFile *pFile, CTempFile &store);
	virtual int DeleteFile( DWORD FileID );
	virtual int Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  );

	virtual	int Format_Process( DWORD FT, HWND hWnd );

	virtual int SwapFile( DWORD FileID1, DWORD FileID2 );

	virtual LocalCommands GetLocalCommands( void );
	virtual int ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd );

	virtual BYTE *GetTitleString( NativeFile *pFile, DWORD Flags );
	virtual BYTE *DescribeFile( DWORD FileIndex );
	virtual BYTE *GetStatusString( int FileIndex, int SelectedItems );

	virtual int MakeAudio( std::vector<NativeFile> &Selection, TapeIndex &indexes, CTempFile &store );

protected:
	void RegenerateTZXSource( BYTE TZXOp, DWORD Pos1, DWORD Pos2, CTempFile *src );

	virtual void RequestResolvedFileWrite( CTempFile &Output, NativeFile *pFile );
	virtual void DescribeResolvedFile( BYTE *Buffer, NativeFile *pFile );
	virtual void RenameResolvedFile( NativeFile *pFile, BYTE *NewName, BYTE *NewExt  );
	virtual void MakeResolvedAudio( NativeFile *pFile, CTempFile &output, TapeIndex &indexes );

	virtual WCHAR *Identify( DWORD FileID );

	virtual void SpeedlockBitFix( DWORD FileID );

	inline void MakeAudioPulses( DWORD PulseWidth, DWORD PulseCycles, CTempFile *output );
	inline void MakeAudioPulse( DWORD PulseWidth1, DWORD PulseWidth2, CTempFile *output );
	inline void MakeAudioBytes( BYTE *pData, DWORD lData, CTempFile *output );
	inline void MakeAudioBytes( BYTE *pData, DWORD lData, WORD l1, WORD l2, BYTE br, bool LastBlock, CTempFile *output );
	inline void MakePause( DWORD ms, CTempFile *output );
	inline void WriteAudio( CTempFile *output );

	std::wstring GetZXFilename( BYTE *Header );

	DWORD AudioPtr;

private:
	int ReadTextBlock( NativeFile *pFile, CTempFile &store );

	void CopyTZX( CTempFile &Output, CTempFile *Source );
	void CopyTZX( CTempFile &Output, NativeFile *pSource );

	void GetBlockDescription( BYTE *Buffer, DWORD FileIndex );
	void ConvertBlockString( BYTE *ptr, BYTE *out );

	const WCHAR *DescribeBlock( BYTE BlockID );

	void  ProcessWrittenTZXBlock( CTempFile &BlockStore, DWORD Index1, DWORD Index2, int BlockOp );
	WORD  AdjustTZXJumpVector( WORD JumpVec, DWORD CIndex, DWORD Index1, DWORD Index2, int BlockOp );

public:
	TZXDirectory *pDir;

protected:
	TZXPB tzxpb;

private:
	bool SPFIX_Warning;

	BYTE SignalLevel;

	BYTE TapeAudio[ 16384 ];
	DWORD TapePtr;
	DWORD TapeLimit;
	double TapePartial;
};

