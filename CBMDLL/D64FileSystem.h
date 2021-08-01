#pragma once
#include "../NUTS/filesystem.h"
#include "d64directory.h"
#include "BAM.h"

class D64FileSystem :
	public FileSystem
{
public:
	D64FileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		pBAM        = new BAM( pDataSource );
		pDir        = new D64Directory( pDataSource );
		pDir->pBAM  = pBAM;
		pDirectory	= ( Directory * ) pDir;

		FSID  = FSID_D64;
		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Spaces | FSF_Reorderable | FSF_Uses_Extensions | FSF_Fake_Extensions;
		Drive = 0;

		IsOpenCBM = false;

		DS_ComplexShape Shape;

		Shape.Head1 = 0;
		Shape.Heads = 1;
		Shape.Interleave = false;
		Shape.SectorSize = 256;
		Shape.Track1     = 1;

		const BYTE spt[41] = {
			0,    // Track 0 doesn't exist (it's used as a "no more" marker)
			21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,   // Tracks 1  - 17
			19, 19, 19, 19, 19, 19, 19,                                           // Tracks 18 - 24
			18, 18, 18, 18, 18, 18,                                               // Tracks 25 - 30
			17, 17, 17, 17, 17, /* 40-track disks from here */ 17, 17, 17, 17, 17 // Tracks 30 - 40
		};

		for ( DWORD Track = 1; Track <= 40; Track++ )
		{
			DS_TrackDef trk;

			trk.Sector1 = 0;
			trk.Sectors = spt[ Track ];

			Shape.TrackDefs.push_back( trk );
		}

		Shape.Tracks = 35; // Only for D64!

		pDataSource->SetComplexDiskShape( Shape );
	}

	~D64FileSystem(void) {
		delete pDirectory;
		delete pBAM;
	}

	virtual int Init(void) {
		pBAM->ReadBAM();
		pDirectory->ReadDirectory();

		return 0;
	}

	int ReadFile(DWORD FileID, CTempFile &store);
	int WriteFile(NativeFile *pFile, CTempFile &store);

	BYTE *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *GetTitleString( NativeFile *pFile );
	WCHAR *Identify( DWORD FileID );

	AttrDescriptors GetAttributeDescriptions( void );
	AttrDescriptors GetFSAttributeDescriptions( void );

	int  SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );

	int  Format_Process( FormatType FT, HWND hWnd );

	DWORD GetEncoding(void )
	{
		return ENCODING_PETSCII;
	}

	FSHint Offer( BYTE *Extension );

	int MakeASCIIFilename( NativeFile *pFile );

	int CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );
	int ReplaceFile(NativeFile *pFile, CTempFile &store);
	int DeleteFile( DWORD FileID );

	FSToolList GetToolsList( void );
	int RunTool( BYTE ToolNum, HWND ProgressWnd );

public:
	D64Directory *pDir;
	DWORD        Drive;
	bool         IsOpenCBM;
	BAM          *pBAM;

private:

	int MarkChain( TSLink Loc );
};
