#pragma once

#include "../nuts/filesystem.h"
#include "NewFSMap.h"
#include "ADFSEDirectory.h"
#include "SpriteFile.h"
#include "ADFSCommon.h"
#include "TranslatedSector.h"

class ADFSEFileSystem : public FileSystem, ADFSCommon, TranslatedSector
{
public:
	ADFSEFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) {
		FSID  = FSID_ADFS_H;
		Flags = FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Supports_Dirs | FSF_Exports_Sidecars;

		rstrncpy( path, (BYTE *) "$", 512 );

		pEDirectory = nullptr;
		pDirectory  = nullptr;
		pFSMap      = nullptr;
	}

	ADFSEFileSystem( const ADFSEFileSystem &source ) : FileSystem( source.pSource )
	{
		pSource = source.pSource;
		FSID    = source.FSID;
		Flags   = source.Flags;

		CloneWars = true;

		FloppyFormat = source.FloppyFormat;
		MediaShape   = source.MediaShape;

		rstrncpy( path, (BYTE *) source.path, 512 );

		pFSMap  = nullptr;

		pEDirectory = new ADFSEDirectory( *source.pEDirectory );

		pFSMap = new NewFSMap( pSource );

		pEDirectory->pMap = pFSMap;

		pDirectory = (Directory *) pEDirectory;

		pFSMap->ReadFSMap();

		pEDirectory->Files = source.pEDirectory->Files;
	}

	~ADFSEFileSystem(void) {
		if ( pDirectory != nullptr )
		{
			FreeAppIcons( pDirectory );
		}

		if ( pEDirectory != nullptr )
		{
			delete pEDirectory;
		}

		if ( pFSMap != nullptr )
		{
			delete pFSMap;
		}
	}

	NewFSMap *pFSMap;

public:
	ADFSEDirectory *pEDirectory;

	BYTE path[512];

	FSHint Offer( BYTE *Extension );

	int  Init(void);
	int  ChangeDirectory( DWORD FileID );
	int	 Parent();
	int  Refresh( void );

	bool IsRoot()
	{
		if ( pEDirectory->DirSector == pFSMap->RootLoc )
		{
			return true;
		}

		return false;
	}

	int ReadFile(DWORD FileID, CTempFile &store);
	int	WriteFile(NativeFile *pFile, CTempFile &store);
	int	CreateDirectory( NativeFile *pDir, DWORD CreateFlags );
	int DeleteFile( DWORD FileID );

	BYTE *DescribeFile(DWORD FileIndex);
	BYTE *GetStatusString( int FileIndex, int SelectedItems );
	BYTE *GetTitleString( NativeFile *pFile, DWORD Flags );

	FileSystem *FileFilesystem( DWORD FileID );
	WCHAR *Identify( DWORD FileID );
	int  CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd );

	AttrDescriptors GetAttributeDescriptions( NativeFile *pFile = nullptr );
	AttrDescriptors GetFSAttributeDescriptions( void );

	int ResolveAuxFileType( NativeFile *pSprite, NativeFile *pFile, SpriteFile &spriteFile );

	EncodingIdentifier GetEncoding(void )
	{
		return ENCODING_RISCOS;
	}

	int SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal );

	int Format_Process( DWORD FT, HWND hWnd );
	int Format_PreCheck( int FormatType, HWND hWnd );	

	bool IncludeEmuHeader;

	int SetProps( DWORD FileID, NativeFile *Changes );

	int ReplaceFile(NativeFile *pFile, CTempFile &store);

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
		ADFSEFileSystem *CloneFS = new ADFSEFileSystem( *this );

		return CloneFS;
	}

	std::vector<WrapperIdentifier> RecommendWrappers()
	{
		static std::vector<WrapperIdentifier> dsk;

		dsk.clear();

		if ( ! (pSource->Flags & DS_RawDevice) )
		{
			if ( ( FSID == FSID_ADFS_HN ) || ( FSID == FSID_ADFS_HP )  || ( FSID == FSID_ADFS_HO ) || ( FSID == FSID_ADFS_H ) )
			{
				dsk.push_back( WID_EMUHDR ); 
			}
		}

		return dsk;
	}

private:
	TargetedFileFragments FindSpace( DWORD Length, bool ForDir );

	void SetShape(void);
	void UpdateShape( DWORD SecSize, DWORD SecsTrack );
};

