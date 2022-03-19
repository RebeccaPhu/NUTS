#pragma once

#ifndef LIBARCHIVE_STATIC
#define LIBARCHIVE_STATIC
#endif

#include "FileSystem.h"
#include "DataSource.h"
#include "ZIPCommon.h"

#include "ZIPDirectory.h"
#include "FOP.h"

class ZIPFile : public FileSystem, ZIPCommon
{
public:
	ZIPFile( DataSource *pSrc ) : FileSystem( pSrc )
	{
		pSource = pSrc;

		pZDir = new ZIPDirectory( pSrc );
		pZDir->srcFS = (void *) this;

		pDirectory = (Directory *) pZDir;

		rstrncpy( cpath, (BYTE *) "", 2 );

		Flags =
			FSF_Creates_Image  | FSF_Formats_Image | FSF_Formats_Raw |
			FSF_DynamicSize | 
			FSF_Supports_Spaces | FSF_Supports_Dirs |
			FSF_Size |
			FSF_Uses_Extensions |
			FSF_Accepts_Sidecars | FSF_Supports_FOP | FSF_NoInPlaceAttrs;

		FSID = FSID_ZIP;

		TopicIcon = FT_Archive;

		CloneWars = false;

		pZDir->CloneWars  = CloneWars;
		pZDir->ProcessFOP = ProcessFOP;
		pZDir->LoadFOPFS  = LoadFOPFS;
	}

	ZIPFile( const ZIPFile &source ) : FileSystem( source.pSource )
	{
		pSource = source.pSource;

		pZDir = new ZIPDirectory( pSource );
		pZDir->srcFS = (void *) this;

		pDirectory = (Directory *) pZDir;

		pDirectory->Files = source.pDirectory->Files;

		Flags = FSF_Supports_Dirs | FSF_Creates_Image | FSF_ArbitrarySize | FSF_DynamicSize | FSF_Uses_Extensions | FSF_Accepts_Sidecars;

		rstrncpy( cpath, source.cpath, 255 );

		FSID = FSID_ZIP;

		TopicIcon = FT_Archive;

		CloneWars = true;

		ProcessFOP = source.ProcessFOP;
		LoadFOPFS  = source.LoadFOPFS;

		pZDir->CloneWars  = CloneWars;
		pZDir->ProcessFOP = ProcessFOP;
		pZDir->LoadFOPFS  = LoadFOPFS;
	}

	~ZIPFile( void )
	{
		delete pZDir;
	}

public:
	BYTE *GetTitleString( NativeFile *pFile, DWORD Flags );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *DescribeFile( DWORD FileIndex );

	int  ReadFile(DWORD FileID, CTempFile &store);
	int  WriteFile(NativeFile *pFile, CTempFile &store);
	int  DeleteFile( DWORD FileID );
	int  Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt  );

	int  ReplaceFile(NativeFile *pFile, CTempFile &store);

	int  ChangeDirectory( DWORD FileID );
	int	 CreateDirectory( NativeFile *pDir, DWORD CreateFlags );
	int  Parent();
	bool IsRoot();

	int  Format_Process( DWORD FT, HWND hWnd );

	int Init(void) {
		pZDir->ProcessFOP = ProcessFOP;

		pDirectory->ReadDirectory();

		return 0;
	}

	DataSource *GetSource()
	{
		return pSource;
	}

	FileSystem *Clone( void )
	{
		ZIPFile *CloneFS = new ZIPFile( *this );

		return CloneFS;
	}

	std::vector<AttrDesc> GetAttributeDescriptions( NativeFile *pFile = nullptr );

private:
	ZIPDirectory *pZDir;

	BYTE cpath[ 256 ];

	bool CloneWars;

private:
	int RenameIncomingDirectory( NativeFile *pDir, Directory *pDirectory );
};

class ZIPFromTemp : public ZIPCommon
{
public:
	ZIPFromTemp( CTempFile &tmp )
	{
		pSource = new ImageDataSource( tmp.Name() );
	}

	~ZIPFromTemp( )
	{
		DS_RELEASE( pSource );
	}

	DataSource *GetSource()
	{
		return pSource;
	}

private:
	DataSource *pSource;
};