#pragma once

#include "SinclairTZXDirectory.h"
#include "SinclairDefs.h"
#include "../nuts/TZXFileSystem.h"


class SinclairTZXFileSystem :
	public TZXFileSystem
{
public:
	SinclairTZXFileSystem( DataSource *pDataSource ) : TZXFileSystem( pDataSource, SinclairPB )
	{
		pSinclairDir = new SinclairTZXDirectory( pDataSource, SinclairPB );
		pDir         = (TZXDirectory *) pSinclairDir;
		pDirectory   = (Directory *) pSinclairDir;

		FSID = FSID_SPECTRUM_TZX;
	}

	~SinclairTZXFileSystem(void)
	{
	}

	AttrDescriptors GetAttributeDescriptions( void );
	int SetProps( DWORD FileID, NativeFile *Changes );

	int WriteFile(NativeFile *pFile, CTempFile &store);

	virtual LocalCommands GetLocalCommands( void );
	virtual int ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd );

protected:
	void RequestResolvedFileWrite( CTempFile &Output, NativeFile *pFile );
	void DescribeResolvedFile( BYTE *Buffer, NativeFile *pFile );
	void RenameResolvedFile( NativeFile *pFile, BYTE *NewName, BYTE *NewExt  );
	void MakeResolvedAudio( NativeFile *pFile, CTempFile &output, TapeIndex &indexes );

public:
	int ReadFile(DWORD FileID, CTempFile &store);

private:
	void MakeAudioFromBytes( BYTE Flag, TapeCue &Cue, QWORD Offset, DWORD Length, CTempFile &output, BYTE *Header );

public:
	SinclairTZXDirectory *pSinclairDir;

};

