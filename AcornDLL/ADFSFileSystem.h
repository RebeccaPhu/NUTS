#pragma once
#include "../NUTS/filesystem.h"
#include "ADFSDirectory.h"
#include "OldFSMap.h"
#include "ADFSCommon.h"
#include "TranslatedSector.h"
#include "AcornDLL.h"
#include "Defs.h"

#include <vector>

/* This is the "Old Map" ADFS file system that requires whole-files (no fragmentation)
   and has a where-and-how-big free space map */

class ADFSFileSystem :
	public FileSystem, ADFSCommon, TranslatedSector
{
public:
	ADFSFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		FSID  = FSID_ADFS_H;
		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Dirs | FSF_Exports_Sidecars;

		rstrncpy( path, (BYTE *) "$", 512 );

		SecSize    = 256;

		pFSMap = nullptr;

		pDirectory     = nullptr;
		pADFSDirectory = nullptr;
		pFSMap         = nullptr;
		Override       = false;

		/* Set the disk shape to 40 track for now, it may get reset when initialised properly */
		DiskShape shape;

		shape.Heads            = 1;
		shape.InterleavedHeads = false;
		shape.Sectors          = 16;
		shape.SectorSize       = 256;
		shape.Tracks           = 40;

		pSource->SetDiskShape( shape );
	}

	ADFSFileSystem( const ADFSFileSystem &source ) : FileSystem( source.pSource )
	{
		pSource = source.pSource;
		FSID    = source.FSID;
		Flags   = source.Flags;

		CloneWars = true;

		rstrncpy( path, (BYTE *) source.path, 512 );
		rstrncpy( DiscName, (BYTE *) source.DiscName, 10 );

		pFSMap  = nullptr;

		FloppyFormat = source.FloppyFormat;
		MediaShape   = source.MediaShape;

		pADFSDirectory = new ADFSDirectory( pSource );

		pADFSDirectory->FSID  = FSID;
		pADFSDirectory->Files = source.pADFSDirectory->Files;

		if ( source.pADFSDirectory->UseLFormat ) { pADFSDirectory->SetLFormat(); }
		if ( source.pADFSDirectory->UseDFormat ) { pADFSDirectory->SetDFormat(); }

		pFSMap = new OldFSMap( pSource );

		pDirectory = (Directory *) pADFSDirectory;

		pFSMap->FloppyFormat = source.FloppyFormat;

		if ( FSID == FSID_ADFS_D )
		{
			pFSMap->UseDFormat =  true;
		}

		pFSMap->ReadFSMap();

		pADFSDirectory->DirSector    = source.pADFSDirectory->DirSector;
		pADFSDirectory->ParentSector = source.pADFSDirectory->ParentSector;
		pADFSDirectory->FloppyFormat = source.pADFSDirectory->FloppyFormat;
		pADFSDirectory->MediaShape   = source.pADFSDirectory->MediaShape;

		pDirectory->ReadDirectory();
	}

	~ADFSFileSystem(void) {
		if ( pDirectory != nullptr )
		{
			FreeAppIcons( pDirectory );
		}

		if ( pADFSDirectory != nullptr )
		{
			delete pADFSDirectory;
		}

		if ( pFSMap != nullptr )
		{
			delete pFSMap;
		}
	}

	int  Init(void);
	void SetShape(void);
	int  ReadFile(DWORD FileID, CTempFile &store);
	int  WriteFile(NativeFile *pFile, CTempFile &store);
	int  DeleteFile( DWORD FileID );
	int  ReplaceFile(NativeFile *pFile, CTempFile &store);
	int  ChangeDirectory( DWORD FileID );
	int  Parent();
	int  CreateDirectory( NativeFile *pDir, DWORD CreateFlags );
	bool IsRoot();
	int  CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );
	int  Refresh( void );

	AttrDescriptors GetAttributeDescriptions( NativeFile *pFile = nullptr );
	AttrDescriptors GetFSAttributeDescriptions( void );

	EncodingIdentifier GetEncoding(void )
	{
		if ( FSID.substr( 0, 6 ) == L"RiscOS" )
		{
			return ENCODING_RISCOS;
		}

		return ENCODING_ACORN;
	}

	BYTE *GetTitleString( NativeFile *pFile, DWORD Flags );
	BYTE *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	WCHAR *Identify( DWORD FileID );

	int Format_Process( DWORD FT, HWND hWnd );
	int Format_PreCheck( int FormatType, HWND hWnd );
	
	OldFSMap *pFSMap;

	BYTE path[512];
	BYTE DiscName[ 11 ];

	FSHint Offer( BYTE *Extension );

	FSToolList GetToolsList( void );

	int RunTool( BYTE ToolNum, HWND ProgressWnd );

	FileSystem *FileFilesystem( DWORD FileID );

	int SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );
	int SetProps( DWORD FileID, NativeFile *Changes );

	int ExportSidecar( NativeFile *pFile, SidecarExport &sidecar )
	{
		return ADFSCommon::ExportSidecar( pFile, sidecar );
	}

	int ImportSidecar( NativeFile *pFile, SidecarImport &sidecar, CTempFile *obj )
	{
		return ADFSCommon::ImportSidecar( pFile, sidecar, obj );
	}

	FileSystem *Clone( void )
	{
		ADFSFileSystem *CloneFS = new ADFSFileSystem( *this );

		return CloneFS;
	}

	std::vector<WrapperIdentifier> RecommendWrappers()
	{
		static std::vector<WrapperIdentifier> dsk;

		dsk.clear();

		if ( ! (pSource->Flags & DS_RawDevice) )
		{
			if ( ( FSID == FSID_ADFS_HO ) || ( FSID == FSID_ADFS_H ) )  // H8 is for 8-bit IDE raw devices only.
			{
				dsk.push_back( WID_EMUHDR ); 
			}
		}

		return dsk;
	}


	int Imaging( DataSource *pImagingSource, DataSource *pImagingTarget, HWND ProgressWnd )
	{
		SetShape();

		if ( FSID == FSID_ADFS_S )
		{
			return _ImagingFunc( pImagingSource, pImagingTarget, 1, 40, 10, true, 256, ProgressWnd );
		}

		if ( FSID == FSID_ADFS_M )
		{
			return _ImagingFunc( pImagingSource, pImagingTarget, 1, 80, 10, true, 256, ProgressWnd );
		}

		if ( ( FSID == FSID_ADFS_L ) || ( FSID == FSID_ADFS_L2 ) )
		{
			return _ImagingFunc( pImagingSource, pImagingTarget, 2, 80, 10, true, 256, ProgressWnd );
		}

		if ( FSID == FSID_ADFS_D )
		{
			return _ImagingFunc( pImagingSource, pImagingTarget, 2, 80, 5, true, 1024, ProgressWnd );
		}

		if ( ( FSID == FSID_ADFS_H8 ) || ( FSID == FSID_ADFS_HO ) || ( FSID == FSID_ADFS_H ) )
		{
			return _ImagingFunc( pImagingSource, pImagingTarget, 0, 0, 0, false, 256, ProgressWnd );
		}

		return NUTSError( 0xC01, L"Imaging not supported" );
	}

private:
	ADFSDirectory *pADFSDirectory;

	DWORD SecSize;
	DWORD ValidateItems;
	DWORD ValidatedItems;
	HWND  ValidateWnd;

private:
	int ValidateDirectory( DWORD DirSector, DWORD ParentSector, DWORD &FixedParents, DWORD &FixedSigs );
	int CompactImage( void );
	
	CompactionObject FindCompactableObject( DWORD DirSector, DWORD SeekSector );
};

