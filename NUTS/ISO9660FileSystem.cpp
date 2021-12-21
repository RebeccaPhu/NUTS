#include "StdAfx.h"
#include "ISO9660FileSystem.h"

#include "libfuncs.h"
#include "ISOFuncs.h"

ISO9660FileSystem::ISO9660FileSystem( DataSource *pDataSource ) : FileSystem( pDataSource )
{
	pISODir = new ISO9660Directory( pDataSource );

	pDirectory = (Directory *) pISODir;

	pISODir->pPriVolDesc = &PriVolDesc;
	pISODir->CloneWars   = false;

	Flags  =
		FSF_Supports_Spaces | FSF_Supports_Dirs |
		FSF_DynamicSize | FSF_UseSectors |
		FSF_Creates_Image  | FSF_Formats_Image | FSF_Formats_Raw |
		FSF_Uses_Extensions | FSF_NoDir_Extensions |
		FSF_Size | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Capacity |
		FSF_Accepts_Sidecars;

	HasJoliet = false;
}

ISO9660FileSystem::ISO9660FileSystem( const ISO9660FileSystem &source ) : FileSystem( source.pSource )
{
	pISODir = new ISO9660Directory( pSource );

	pDirectory = (Directory *) pISODir;

	pISODir->pPriVolDesc = &PriVolDesc;

	pISODir->DirSector  = source.pISODir->DirSector;
	pISODir->DirLength  = source.pISODir->DirLength;
	pISODir->ProcessFOP = source.pISODir->ProcessFOP;
	pISODir->pSrcFS     = (void *) this;
	pISODir->CloneWars  = true;
	pISODir->Files      = source.pISODir->Files;

	PriVolDesc = source.PriVolDesc;
	DirLBAs    = source.DirLBAs;
	DirLENs    = source.DirLENs;
	CDPath     = source.CDPath;
	HasJoliet  = source.HasJoliet;

	Flags  =
		FSF_Supports_Spaces | FSF_Supports_Dirs |
		FSF_DynamicSize | FSF_UseSectors |
		FSF_Creates_Image  | FSF_Formats_Image | FSF_Formats_Raw |
		FSF_Uses_Extensions | FSF_NoDir_Extensions |
		FSF_Size | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Capacity |
		FSF_Accepts_Sidecars;

	HasJoliet = false;
}

ISO9660FileSystem::~ISO9660FileSystem(void)
{
	delete pISODir;
}

void ISO9660FileSystem::ReadVolumeDescriptors( void )
{
	BYTE Buffer[ 2048 ];

	QWORD VolDescLoc = 0x8000;

	while ( VolDescLoc <= 0x100000 )
	{
		if ( pSource->ReadRaw( VolDescLoc, 2048, Buffer ) != DS_SUCCESS )
		{
			return;
		}

		if ( Buffer[ 0 ] == 0xFF )
		{
			// No more descriptors
			break;
		}

		bool JolietBytes = false;

		if ( ( Buffer[ 0x58 ] == 0x25 ) && ( Buffer[ 0x59 ] == 0x2f ) )
		{
			if ( ( Buffer[ 0x5A ] == 0x040 ) || ( Buffer[ 0x5A ] == 0x43 ) || ( Buffer[ 0x5A ] == 0x45 ) )
			{
				JolietBytes = true;
			}
		}

		if ( Buffer[ 0 ] == 0x01 )
		{
			// Primary volume descriptor. We'll take this.
			memcpy( &PriVolDesc.SysID, &Buffer[  8 ], 32 );
			memcpy( &PriVolDesc.VolID, &Buffer[ 40 ], 32 );

			PriVolDesc.VolSize    = * (DWORD *) &Buffer[  80 ];
			PriVolDesc.VolSetSize = * (WORD *)  &Buffer[ 120 ];
			PriVolDesc.SeqNum     = * (WORD *)  &Buffer[ 124 ];
			PriVolDesc.SectorSize = * (WORD *)  &Buffer[ 128 ];

			PriVolDesc.PathTableSize = * (DWORD *) &Buffer[ 132 ];
			PriVolDesc.LTableLoc     = * (DWORD *) &Buffer[ 140 ];
			PriVolDesc.LTableLocOpt  = * (DWORD *) &Buffer[ 144 ];
			PriVolDesc.MTableLoc     = BEDWORD( &Buffer[ 148 ] );
			PriVolDesc.MTableLocOpt  = BEDWORD( &Buffer[ 152 ] );

			memcpy( &PriVolDesc.DirectoryRecord, &Buffer[ 156 ], 34 );
			memcpy( &PriVolDesc.VolSetID,        &Buffer[ 190 ], 128 );
			memcpy( &PriVolDesc.Publisher,       &Buffer[ 318 ], 128 );
			memcpy( &PriVolDesc.Preparer,        &Buffer[ 446 ], 128 );
			memcpy( &PriVolDesc.AppID,           &Buffer[ 574 ], 128 );

			memcpy( &PriVolDesc.CopyrightFilename,     &Buffer[ 702 ], 37 );
			memcpy( &PriVolDesc.AbstractFilename,      &Buffer[ 739 ], 37 );
			memcpy( &PriVolDesc.BibliographicFilename, &Buffer[ 776 ], 37 );

			memcpy( &PriVolDesc.CreationTime,  &Buffer[ 813 ], 17 );
			memcpy( &PriVolDesc.ModTime,       &Buffer[ 830 ], 17 );
			memcpy( &PriVolDesc.ExpireTime,    &Buffer[ 847 ], 17 );
			memcpy( &PriVolDesc.EffectiveTime, &Buffer[ 864 ], 17 );

			memcpy( &PriVolDesc.ApplicationBytes, &Buffer[ 883 ], 512 );

			// Terminate some strings
			ISOStrTerm( PriVolDesc.SysID, 32 );
			ISOStrTerm( PriVolDesc.VolID, 32 );
			ISOStrTerm( PriVolDesc.VolSetID, 128 );
			ISOStrTerm( PriVolDesc.Publisher, 128 );
			ISOStrTerm( PriVolDesc.Preparer, 128 );
			ISOStrTerm( PriVolDesc.AppID, 128 );
			ISOStrTerm( PriVolDesc.CopyrightFilename, 37 );
			ISOStrTerm( PriVolDesc.AbstractFilename, 37 );
			ISOStrTerm( PriVolDesc.BibliographicFilename, 37 );
			ISOStrTerm( PriVolDesc.CreationTime, 17 );
			ISOStrTerm( PriVolDesc.ModTime, 17 );
			ISOStrTerm( PriVolDesc.ExpireTime, 17 );
			ISOStrTerm( PriVolDesc.EffectiveTime, 17 );
		}

		if ( ( Buffer[ 0 ] == 0x02 ) && ( JolietBytes ) )
		{
			// Supplementary volume descriptor. Could be Joliet.
			memcpy( &JolietDesc.SysID, &Buffer[  8 ], 32 );
			memcpy( &JolietDesc.VolID, &Buffer[ 40 ], 32 );

			JolietDesc.VolSize    = * (DWORD *) &Buffer[  80 ];
			JolietDesc.VolSetSize = * (WORD *)  &Buffer[ 120 ];
			JolietDesc.SeqNum     = * (WORD *)  &Buffer[ 124 ];
			JolietDesc.SectorSize = * (WORD *)  &Buffer[ 128 ];

			JolietDesc.PathTableSize = * (DWORD *) &Buffer[ 132 ];
			JolietDesc.LTableLoc     = * (DWORD *) &Buffer[ 140 ];
			JolietDesc.LTableLocOpt  = * (DWORD *) &Buffer[ 144 ];
			JolietDesc.MTableLoc     = BEDWORD( &Buffer[ 148 ] );
			JolietDesc.MTableLocOpt  = BEDWORD( &Buffer[ 152 ] );

			memcpy( &JolietDesc.DirectoryRecord, &Buffer[ 156 ], 34 );
			memcpy( &JolietDesc.VolSetID,        &Buffer[ 190 ], 128 );
			memcpy( &JolietDesc.Publisher,       &Buffer[ 318 ], 128 );
			memcpy( &JolietDesc.Preparer,        &Buffer[ 446 ], 128 );
			memcpy( &JolietDesc.AppID,           &Buffer[ 574 ], 128 );

			memcpy( &JolietDesc.CopyrightFilename,     &Buffer[ 702 ], 37 );
			memcpy( &JolietDesc.AbstractFilename,      &Buffer[ 739 ], 37 );
			memcpy( &JolietDesc.BibliographicFilename, &Buffer[ 776 ], 37 );

			memcpy( &JolietDesc.CreationTime,  &Buffer[ 813 ], 17 );
			memcpy( &JolietDesc.ModTime,       &Buffer[ 830 ], 17 );
			memcpy( &JolietDesc.ExpireTime,    &Buffer[ 847 ], 17 );
			memcpy( &JolietDesc.EffectiveTime, &Buffer[ 864 ], 17 );

			memcpy( &JolietDesc.ApplicationBytes, &Buffer[ 883 ], 512 );

			// Terminate some strings
			JolietStrTerm( JolietDesc.SysID, 32 );
			JolietStrTerm( JolietDesc.VolID, 32 );
			JolietStrTerm( JolietDesc.VolSetID, 128 );
			JolietStrTerm( JolietDesc.Publisher, 128 );
			JolietStrTerm( JolietDesc.Preparer, 128 );
			JolietStrTerm( JolietDesc.AppID, 128 );
			JolietStrTerm( JolietDesc.CopyrightFilename, 37 );
			JolietStrTerm( JolietDesc.AbstractFilename, 37 );
			JolietStrTerm( JolietDesc.BibliographicFilename, 37 );
			ISOStrTerm( JolietDesc.CreationTime, 17 );
			ISOStrTerm( JolietDesc.ModTime, 17 );
			ISOStrTerm( JolietDesc.ExpireTime, 17 );
			ISOStrTerm( JolietDesc.EffectiveTime, 17 );

			HasJoliet = true;
		}

		VolDescLoc += 2048;
	}
}

int ISO9660FileSystem::Init( void )
{
	pISODir->ProcessFOP = ProcessFOP;
	pISODir->pSrcFS     = (void *) this;

	ReadVolumeDescriptors();

	// We should have the PVD by now. Do some interpretation of the superroot directory
	DWORD RootLBA = * (DWORD *) &PriVolDesc.DirectoryRecord[  2 ];
	DWORD RootSz  = * (DWORD *) &PriVolDesc.DirectoryRecord[ 10 ];
	BYTE  Flags   = PriVolDesc.DirectoryRecord[ 25 ];

	if ( ( Flags & 2 ) == 0 )
	{
		// Err.... the superroot isn't a directory. That's a problem.
		return NUTSError( 0xC01, L"SuperRoot on Primary Volume doesn't list a directory." );
	}

	pISODir->DirSector = RootLBA;
	pISODir->DirLength = RootSz;

	CDPath = "/";

	pDirectory->ReadDirectory();

	return 0;
}

int ISO9660FileSystem::ChangeDirectory( DWORD FileID ) {
	if ( !IsRoot() )
	{
		CDPath += "/";
	}

	DirLBAs.push_back( pISODir->DirSector );
	DirLENs.push_back( pISODir->DirLength );

	pISODir->DirSector = pDirectory->Files[ FileID ].Attributes[ ISOATTR_START_EXTENT ];
	pISODir->DirLength = pDirectory->Files[ FileID ].Length;

	CDPath += std::string( (char *) (BYTE *) pDirectory->Files[ FileID ].Filename );

	return pDirectory->ReadDirectory();
}

int ISO9660FileSystem::Parent()
{
	CDPath = CDPath.substr( 0, CDPath.find_last_of( '/' ) );

	pISODir->DirSector = DirLBAs.back();
	pISODir->DirLength = DirLENs.back();

	DirLBAs.pop_back();
	DirLENs.pop_back();

	if ( IsRoot() )
	{
		CDPath = "/";
	}

	return pDirectory->ReadDirectory();
}

bool ISO9660FileSystem::IsRoot() {
	DWORD RootLBA = * (DWORD *) &PriVolDesc.DirectoryRecord[  2 ];

	if ( pISODir->DirSector == RootLBA )
	{
		return true;
	}

	return false;
}

int ISO9660FileSystem::ReadFile(DWORD FileID, CTempFile &store) {
	QWORD Loc = pDirectory->Files[ FileID ].Attributes[ ISOATTR_START_EXTENT ];
	
	Loc *= PriVolDesc.SectorSize;

	DWORD BytesToGo = pDirectory->Files[ FileID ].Length;

	AutoBuffer Buffer( PriVolDesc.SectorSize );

	store.Seek( 0 );

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( PriVolDesc.SectorSize, BytesToGo );

		if ( pSource->ReadRaw( Loc, BytesRead, (BYTE *) Buffer ) != DS_SUCCESS )
		{
			return -1;
		}

		store.Write( (BYTE *) Buffer, BytesRead );

		BytesToGo -= BytesRead;
		Loc       += BytesRead;
	}

	return NUTS_SUCCESS;
}

BYTE *ISO9660FileSystem::GetTitleString( NativeFile *pFile, DWORD Flags ) {
	static BYTE ISOPath[512];

	rstrncpy( ISOPath, (BYTE *) "CDROM:", 511 );

	rstrncat( ISOPath, (BYTE *) CDPath.c_str(), 511 );

	if ( pFile != nullptr )
	{
		rstrncat( ISOPath, (BYTE *) "/", 511 );
		rstrncat( ISOPath, (BYTE *) pFile->Filename, 511 );

		if ( pFile->Flags & FF_Extension )
		{
			rstrncat( ISOPath, (BYTE *) ".", 511 );
			rstrncat( ISOPath, (BYTE *) pFile->Extension, 511 );
		}

		if ( Flags & TF_Titlebar )
		{
			if ( !(Flags & TF_Final ) )
			{
				static BYTE chevron[4] = { 0x20, 0xAF, 0x20, 0x00 };

				rstrncat( ISOPath, chevron, 511 );
			}
		}
	}

	return ISOPath;
}

BYTE *ISO9660FileSystem::DescribeFile( DWORD FileIndex )
{
	static BYTE status[ 128 ];

	NativeFile	*pFile	= &pDirectory->Files[FileIndex];

	if ( pISODir->FileFOPData.find( FileIndex ) != pISODir->FileFOPData.end() )
	{
		rstrncpy( status, pISODir->FileFOPData[ FileIndex ].Descriptor, 128 );
	}
	else if ( pFile->Flags & FF_Directory )
	{
		rsprintf( status, "Directory" );
	}
	else if ( pFile->ExtraForks > 0 )
	{
		rsprintf( status, "File, %d/%d bytes", pFile->Length, pFile->Attributes[ ISOATTR_FORK_LENGTH ] );
	}
	else
	{
		rsprintf( status, "File, %d bytes", pFile->Length );
	}
		
	return status;
}

BYTE *ISO9660FileSystem::GetStatusString( int FileIndex, int SelectedItems ) {
	static BYTE status[128];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "%d File System Objects", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d Items Selected", SelectedItems );
	}
	else if ( pISODir->FileFOPData.find( FileIndex ) != pISODir->FileFOPData.end() )
	{
		rstrncpy( status, pISODir->FileFOPData[ FileIndex ].StatusString, 128 );
	}
	else
	{
		NativeFile *pFile = &pDirectory->Files[FileIndex];

		if ( pFile->Flags & FF_Directory )
		{
			rsprintf( status, "%s, Directory", (BYTE *) pFile->Filename	);
		}
		else
		{
			if ( pFile->Flags & FF_Extension )
			{
				rsprintf( status, "%s.%s, %d bytes",
					(BYTE *) pFile->Filename, (BYTE *) pFile->Extension, (DWORD) pFile->Length
				);
			}
			else
			{
				rsprintf( status, "%s, %d bytes",
					(BYTE *) pFile->Filename, (DWORD) pFile->Length
				);
			}

			if ( pFile->ExtraForks > 0 )
			{
				rsprintf( &status[ rstrnlen( status, 128 ) ], " %d bytes in fork", pFile->Attributes[ ISOATTR_FORK_LENGTH ] );
			}
		}
	}
		
	return status;
}

std::vector<AttrDesc> ISO9660FileSystem::GetAttributeDescriptions( NativeFile *pFile )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	if ( pFile != nullptr )
	{
		FOPData fop;

		fop.DataType  = FOP_DATATYPE_CDISO;
		fop.Direction = FOP_ExtraAttrs;
		fop.lXAttr    = 0;
		fop.pXAttr    = (BYTE *) &Attrs;
		fop.pFile     = (void *) pFile;
		fop.pFS       = nullptr;
	
		ProcessFOP( &fop );
	}

	AttrDesc Attr;

	/* Start sector. Hex, visible, disabled */
	Attr.Index = 16;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile | AttrDir;
	Attr.Name  = L"Start sector";
	Attrs.push_back( Attr );

	/* Revision, editable */
	Attr.Index = 17;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrFile | AttrDec;
	Attr.Name  = L"Revision";
	Attrs.push_back( Attr );

	/* Fork Sector. Hex, visible, disabled */
	Attr.Index = 18;
	Attr.Type  = AttrVisible | AttrNumeric | AttrFile | AttrHex;
	Attr.Name  = L"Fork Sector";
	Attrs.push_back( Attr );

	/* Fork Length. Hex, visible, disabled */
	Attr.Index = 19;
	Attr.Type  = AttrVisible | AttrNumeric | AttrFile | AttrHex;
	Attr.Name  = L"Fork Length";
	Attrs.push_back( Attr );

	return Attrs;
}

LocalCommands ISO9660FileSystem::GetLocalCommands( void )
{
	LocalCommand c1 = { LC_Always, L"Add Joliet Volume Descriptor" }; 
	LocalCommand c2 = { LC_Always, L"Edit Volume Descriptors" };
	LocalCommand s  = { LC_IsSeparator, L"" };
	LocalCommand c3 = { LC_Always, L"Set Maximum Capacity" };
	LocalCommand c4 = { LC_Always, L"Enable Writing" };

	if ( HasJoliet )
	{
		c1.Flags = 0;
	}

	LocalCommands cmds;

	cmds.HasCommandSet = true;

	if ( FSID == FSID_ISO9660 )
	{
		cmds.Root = L"ISO9660";
	}

	if ( FSID == FSID_ISOHS )
	{
		cmds.Root = L"High Sierra";
	}

	cmds.CommandSet.push_back( c1 );
	cmds.CommandSet.push_back( c2 );
	cmds.CommandSet.push_back( s );
	cmds.CommandSet.push_back( c3 );
	cmds.CommandSet.push_back( s );
	cmds.CommandSet.push_back( c4 );

	return cmds;
}

WCHAR *ISO9660FileSystem::Identify( DWORD FileID )
{
	if ( pISODir->FileFOPData.find( FileID ) != pISODir->FileFOPData.end() )
	{
		return (WCHAR *) pISODir->FileFOPData[ FileID ].Identifier.c_str();
	}

	return FileSystem::Identify( FileID );
}

FileSystem *ISO9660FileSystem::FileFilesystem( DWORD FileID )
{
	if ( pISODir->FileFOPData.find( FileID ) != pISODir->FileFOPData.end() )
	{
		DWORD FSID = pISODir->FileFOPData[ FileID ].ProposedFS;

		DataSource *pSource = FileDataSource( FileID );

		if ( pSource == nullptr )
		{
			return nullptr;
		}

		FileSystem *pFS = (FileSystem *) LoadFOPFS( FSID, pSource );

		DS_RELEASE( pSource );

		return pFS;
	}

	return nullptr;
}

int ISO9660FileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	ResetEvent( hCancelFree );

	static FSSpace Map;

	DWORD NumBlocks = pSource->PhysicalDiskSize / PriVolDesc.SectorSize;

	NumBlocks &= 0xFFFFFF;

	Map.Capacity  = NumBlocks * PriVolDesc.SectorSize;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = Map.Capacity;

	double BlockRatio = ( (double) NumBlocks / (double) TotalBlocks );

	if ( BlockRatio < 1.0 ) { BlockRatio = 1.0 ; }
	
	memset( pBlockMap, BlockAbsent, TotalBlocks );
	memset( pBlockMap, BlockUsed, (DWORD) ( NumBlocks / BlockRatio ) );

	DWORD BlkNum;

	DWORD FixedBlks = 0x8000 / PriVolDesc.SectorSize;

	for ( DWORD FixedBlk=0; FixedBlk != FixedBlks; FixedBlk++ )
	{
		BlkNum = (DWORD) ( (double) FixedBlk / BlockRatio );
	
		pBlockMap[ BlkNum ] = (BYTE) BlockFixed;
	}

	/*
	for ( iMap = pFSMap->Spaces.begin(); iMap != pFSMap->Spaces.end(); iMap++ )
	{
		if ( WaitForSingleObject( hCancelFree, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		if ( iMap->Length != 0U )
		{
			Map.UsedBytes -= iMap->Length * 256;

			for ( DWORD Blk = iMap->StartSector; Blk != iMap->StartSector + iMap->Length; Blk++ )
			{
				BlkNum = (DWORD) ( (double) Blk / BlockRatio );

				if ( ( BlkNum < TotalBlocks ) && ( pBlockMap[ BlkNum ] != BlockFixed ) )
				{
					pBlockMap[ BlkNum ] = BlockFree;
				}
			}

			SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );
		}
	}
	*/

	SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

	return 0;
}