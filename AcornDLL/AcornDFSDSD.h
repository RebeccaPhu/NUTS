#pragma once
#include "../nuts/filesystem.h"
#include "../NUTS/Directory.h"
#include "../NUTS/Defs.h"
#include "Defs.h"
#include "AcornDLL.h"

class AcornDSDDirectory : public Directory
{
public:
	AcornDSDDirectory( DataSource *pDataSource ) : Directory( pDataSource )
	{
		Files.clear();
	}

	~AcornDSDDirectory( void )
	{
	}

	int	ReadDirectory(void)
	{
		Files.clear();

		NativeFile Drive;

		Drive.EncodingID      = ENCODING_ACORN;
		Drive.fileID          = 0;
		Drive.Flags           = FF_Pseudo;
		Drive.FSFileType      = FT_ACORN;
		Drive.Icon            = FT_DiskImage;
		Drive.Type            = FT_MiscImage;
		Drive.Length          = 0;
		Drive.HasResolvedIcon = false;

		Drive.Filename = (BYTE *) "Drive 0";

		Files.push_back( Drive );

		Drive.Filename = (BYTE *) "Drive 2";

		Drive.fileID = 1;

		Files.push_back( Drive );

		return 0;
	}

	int	WriteDirectory(void)
	{
		return 0;
	}
};

class AcornDFSDSD :
	public FileSystem
{
public:
	AcornDFSDSD(DataSource *pDataSource) : FileSystem(pDataSource) {
		pDirectory	= new AcornDSDDirectory(pDataSource);

		FSID  = FSID_DFS_DSD;
		Flags = FSF_Size | FSF_ReadOnly;
	}

	~AcornDFSDSD(void) {
		delete pDirectory;
	}

	int	ReadFile(DWORD FileID, CTempFile &store)
	{
		return -1;
	}

	int	WriteFile(NativeFile *pFile, CTempFile &store)
	{
		return -1;
	}

	int  ReplaceFile(NativeFile *pFile, CTempFile &store);

	DataSource *FileDataSource( DWORD FileID );

	bool  IsRoot() { return true; }

	BYTE  *DescribeFile( DWORD FileIndex )
	{
		static BYTE Desc[64];

		if ( FileIndex == 0 )
		{
			rsprintf( Desc, "Drive 0 (Underside Surface)" );
		}

		if ( FileIndex == 1 )
		{
			rsprintf( Desc, "Drive 2 (Topside Surface)" );
		}

		return Desc;
	}

	BYTE *GetStatusString( int FileIndex, int SelectedItems )
	{
		static BYTE status[64];

		if ( SelectedItems == 0 )
		{
			rsprintf( status, "%d File System Objects", pDirectory->Files.size() );
		}
		else if ( SelectedItems > 1 )
		{
			rsprintf( status, "%d Items Selected", SelectedItems );
		}
		else if ( FileIndex == 0 )
		{
			rsprintf( status, "Drive 0 (Underside Surface)" );
		}
		else if ( FileIndex == 1 )
		{
			rsprintf( status, "Drive 2 (Topside Surface)" );
		}
		else
		{
			status[0] = 0;
		}

		return status;
	}

	BYTE *GetTitleString( NativeFile *pFile, DWORD Flags )
	{
		static BYTE title[64];

		strncpy_s( (char *) title, 64, (char *) "DFS", 6 );

		if ( pFile != nullptr )
		{
			rsprintf( title, "DFS::%c", pFile->Filename[ 6 ] );
		}

		return title;
	}

	EncodingIdentifier GetEncoding(void )
	{
		return ENCODING_ACORN;
	}

	FSHint Offer( BYTE *Extension )
	{
		FSHint hint;

		hint.FSID       = FSID;
		hint.Confidence = 0;

		if ( Extension != nullptr )
		{
			if ( rstrncmp( Extension, (BYTE *) "DSD", 3 ) )
			{
				hint.Confidence = 30;
			}
		}

		return hint;
	}
};

