#pragma once
#include "../NUTS/filesystem.h"
#include "ADFSDirectory.h"
#include "OldFSMap.h"
#include "ADFSCommon.h"
#include "TranslatedSector.h"

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

		UseLFormat = false;
		UseDFormat = false;

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

		UseLFormat = source.UseLFormat;
		UseDFormat = source.UseDFormat;

		FloppyFormat = source.FloppyFormat;
		MediaShape   = source.MediaShape;

		pADFSDirectory = new ADFSDirectory( pSource );

		pADFSDirectory->FSID  = FSID;
		pADFSDirectory->Files = source.pADFSDirectory->Files;

		if ( source.pADFSDirectory->UseLFormat ) { pADFSDirectory->SetLFormat(); }
		if ( source.pADFSDirectory->UseDFormat ) { pADFSDirectory->SetDFormat(); }

		pFSMap = new OldFSMap( pSource );

		pDirectory = (Directory *) pADFSDirectory;

		pFSMap->ReadFSMap();

		pADFSDirectory->DirSector    = source.pADFSDirectory->DirSector;
		pADFSDirectory->ParentSector = source.pADFSDirectory->ParentSector;
		pADFSDirectory->FloppyFormat = source.pADFSDirectory->FloppyFormat;
		pADFSDirectory->MediaShape   = source.pADFSDirectory->MediaShape;

		pDirectory->ReadDirectory();
	}

	~ADFSFileSystem(void) {
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

	AttrDescriptors GetAttributeDescriptions( void );
	AttrDescriptors GetFSAttributeDescriptions( void );

	void SetLFormat(void) {
		UseLFormat = true;

		FSID = FSID_ADFS_L;
	}

	void SetDFormat(void) {
		UseDFormat = true;
		SecSize    = 1024;

		FSID = FSID_ADFS_D;
	}

	DWORD GetEncoding(void )
	{
		if ( ( FSID == FSID_ADFS_L2 ) || ( FSID == FSID_ADFS_D ) || ( FSID == FSID_ADFS_HO ) )
		{
			return ENCODING_RISCOS;
		}

		return ENCODING_ACORN;
	}

	BYTE *ADFSFileSystem::GetTitleString( NativeFile *pFile );
	BYTE *DescribeFile( DWORD FileIndex );
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	WCHAR *Identify( DWORD FileID );

	int Format_Process( FormatType FT, HWND hWnd );
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

private:
	ADFSDirectory *pADFSDirectory;

	bool UseLFormat;
	bool UseDFormat;

	DWORD SecSize;
	DWORD ValidateItems;
	DWORD ValidatedItems;
	HWND  ValidateWnd;

private:
	int ValidateDirectory( DWORD DirSector, DWORD ParentSector, DWORD &FixedParents, DWORD &FixedSigs );
	int CompactImage( void );
	
	CompactionObject FindCompactableObject( DWORD DirSector, DWORD SeekSector );
};

