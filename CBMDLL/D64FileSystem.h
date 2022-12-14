#pragma once
#include "../NUTS/filesystem.h"
#include "d64directory.h"
#include "BAM.h"

extern const BYTE spt[41];

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

		SetShape();

		PreferredArbitraryExtension = (BYTE *) "PRG";
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
	BYTE *GetTitleString( NativeFile *pFile, DWORD Flags );
	WCHAR *Identify( DWORD FileID );

	AttrDescriptors GetAttributeDescriptions( NativeFile *pFile = nullptr );
	AttrDescriptors GetFSAttributeDescriptions( void );

	int  SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );

	int  Format_Process( DWORD FT, HWND hWnd );

	EncodingIdentifier GetEncoding(void )
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

	void SetShape( void )
	{
		DS_ComplexShape Shape;

		Shape.Head1 = 0;
		Shape.Heads = 1;
		Shape.Interleave = false;
		Shape.Track1     = 1;

		for ( DWORD Track = 1; Track <= 40; Track++ )
		{
			DS_TrackDef trk;

			trk.TrackID = Track;
			trk.HeadID  = 0;
			trk.Sector1 = 0;

			for ( DWORD Sector = 0; Sector < spt[ Track ]; Sector ++ )
			{
				trk.SectorSizes.push_back( 256 );
			}

			Shape.TrackDefs.push_back( trk );
		}

		Shape.Tracks = 35; // Only for D64!

		pSource->SetComplexDiskShape( Shape );
	}

	int Imaging( DataSource *pImagingSource, DataSource *pImagingTarget, HWND ProgressWnd );

public:
	D64Directory *pDir;
	DWORD        Drive;
	bool         IsOpenCBM;
	BAM          *pBAM;

private:

	int MarkChain( TSLink Loc );
};
