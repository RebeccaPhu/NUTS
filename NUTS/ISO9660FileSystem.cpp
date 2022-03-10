#include "StdAfx.h"
#include "ISO9660FileSystem.h"

#include "libfuncs.h"
#include "ISOFuncs.h"
#include "FileDialogs.h"

#include "resource.h"
#include "EncodingEdit.h"

#include <commctrl.h>
#include <WindowsX.h>

#define WRITABLE_TEST() if ( !WritableTest() ) { return -1; }

ISO9660FileSystem::ISO9660FileSystem( DataSource *pDataSource ) : FileSystem( pDataSource )
{
	pISODir = new ISO9660Directory( pDataSource );

	pDirectory = (Directory *) pISODir;

	pISODir->pPriVolDesc = &PriVolDesc;
	pISODir->pJolietDesc = &JolietDesc;
	pISODir->CloneWars   = false;

	Flags  =
		FSF_Supports_Spaces | FSF_Supports_Dirs |
		FSF_DynamicSize | FSF_UseSectors |
		FSF_Creates_Image  | FSF_Formats_Image | FSF_Formats_Raw |
		FSF_Uses_Extensions | FSF_NoDir_Extensions |
		FSF_Size | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Capacity |
		FSF_Accepts_Sidecars;

	HasJoliet   = false;
	UsingJoliet = false;
	Writable    = false;

	pISODir->UsingJoliet = false;

	TopicIcon = FT_CDImage;

	FreeSectors = 0;

	pPathTable = nullptr;
}

ISO9660FileSystem::ISO9660FileSystem( const ISO9660FileSystem &source ) : FileSystem( source.pSource )
{
	pISODir = new ISO9660Directory( pSource );

	pDirectory = (Directory *) pISODir;

	PriVolDesc = source.PriVolDesc;
	JolietDesc = source.JolietDesc;

	pPathTable = nullptr;

	pISODir->pPriVolDesc = &PriVolDesc;
	pISODir->pJolietDesc = &JolietDesc;

	pISODir->DirSector   = source.pISODir->DirSector;
	pISODir->DirLength   = source.pISODir->DirLength;
	pISODir->JDirSector  = source.pISODir->JDirSector;
	pISODir->JDirLength  = source.pISODir->JDirLength;
	pISODir->ProcessFOP  = source.pISODir->ProcessFOP;
	pISODir->pSrcFS      = (void *) this;
	pISODir->CloneWars   = true;
	pISODir->Files       = source.pISODir->Files;
	pISODir->UsingJoliet = source.pISODir->UsingJoliet;
	pISODir->FSID        = source.pISODir->FSID;

	pISODir->PathStackExtent = source.pISODir->PathStackExtent;
	pISODir->PathStackSize   = source.pISODir->PathStackSize;

	// We must clone any aux data too, otherwise the clone FS will free ours!
	for ( NativeFileIterator iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( iFile->lAuxData > 0 )
		{
			BYTE *pNewAuxData = new BYTE[ iFile->lAuxData ];

			memcpy( pNewAuxData, iFile->pAuxData, iFile->lAuxData );

			iFile->pAuxData = pNewAuxData;
		}
	}

	PriVolDesc  = source.PriVolDesc;
	CDPath      = source.CDPath;
	HasJoliet   = source.HasJoliet;
	UsingJoliet = source.UsingJoliet;
	Writable    = source.Writable;
	FreeSectors = source.FreeSectors;

	Flags  =
		FSF_Supports_Spaces | FSF_Supports_Dirs |
		FSF_DynamicSize | FSF_UseSectors |
		FSF_Creates_Image  | FSF_Formats_Image | FSF_Formats_Raw |
		FSF_Uses_Extensions | FSF_NoDir_Extensions |
		FSF_Size | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Capacity |
		FSF_Accepts_Sidecars;

	FSID = source.FSID;

	HasJoliet = false;
}

ISO9660FileSystem::~ISO9660FileSystem(void)
{
	if ( pPathTable != nullptr )
	{
		delete pPathTable;
	}

	delete pISODir;

	pPathTable = nullptr;
	pISODir    = nullptr;
}

void ISO9660FileSystem::ReadVolumeDescriptors( void )
{
	BYTE Buffer[ 2048 ];

	DWORD Sect = 16;

	while ( Sect <= 32 )
	{
		ZeroMemory( Buffer, 2048 );

		if ( pSource->ReadSectorLBA( Sect, Buffer, 2048 ) != DS_SUCCESS ) // Fixed size for VDs.
		{
			return;
		}

		BYTE SecType = Buffer[ 0 ];

		if ( FSID == FSID_ISOHS )
		{
			SecType = Buffer[ 8 ];
		}

		if ( SecType == 0xFF )
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

		if ( FSID == FSID_ISO9660 )
		{
			if ( SecType == 0x01 )
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

				PriVolDesc.SourceSector = Sect;
				PriVolDesc.IsJoliet     = false;
			}

			if ( ( SecType == 0x02 ) && ( JolietBytes ) )
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

				JolietDesc.SourceSector = Sect;
				JolietDesc.IsJoliet     = true;
			}
		}

		if ( FSID == FSID_ISOHS )
		{
			if ( SecType == 0x01 )
			{
				// Primary volume descriptor. We'll take this.
				memcpy( &PriVolDesc.SysID, &Buffer[ 16 ], 32 );
				memcpy( &PriVolDesc.VolID, &Buffer[ 48 ], 32 );

				PriVolDesc.VolSize    = * (DWORD *) &Buffer[  88 ];
				PriVolDesc.VolSetSize = * (WORD *)  &Buffer[ 128 ];
				PriVolDesc.SeqNum     = * (WORD *)  &Buffer[ 132 ];
				PriVolDesc.SectorSize = * (WORD *)  &Buffer[ 136 ];

				PriVolDesc.PathTableSize = * (DWORD *) &Buffer[ 140 ];
				PriVolDesc.LTableLoc     = * (DWORD *) &Buffer[ 148 ];
				PriVolDesc.LTableLocOpt  = * (DWORD *) &Buffer[ 152 ];
				PriVolDesc.LTableLocOptA = * (DWORD *) &Buffer[ 156 ];
				PriVolDesc.LTableLocOptB = * (DWORD *) &Buffer[ 160 ];
				PriVolDesc.MTableLoc     = BEDWORD( &Buffer[ 164 ] );
				PriVolDesc.MTableLocOpt  = BEDWORD( &Buffer[ 168 ] );
				PriVolDesc.MTableLocOptA = BEDWORD( &Buffer[ 172 ] );
				PriVolDesc.MTableLocOptB = BEDWORD( &Buffer[ 176 ] );

				memcpy( &PriVolDesc.DirectoryRecord, &Buffer[ 180 ], 34 );
				memcpy( &PriVolDesc.VolSetID,        &Buffer[ 214 ], 128 );
				memcpy( &PriVolDesc.Publisher,       &Buffer[ 342 ], 128 );
				memcpy( &PriVolDesc.Preparer,        &Buffer[ 470 ], 128 );
				memcpy( &PriVolDesc.AppID,           &Buffer[ 598 ], 128 );

				memcpy( &PriVolDesc.CopyrightFilename,     &Buffer[ 726 ], 37 );
				memcpy( &PriVolDesc.AbstractFilename,      &Buffer[ 758 ], 37 );

				memcpy( &PriVolDesc.CreationTime,  &Buffer[ 790 ], 17 );
				memcpy( &PriVolDesc.ModTime,       &Buffer[ 806 ], 17 );
				memcpy( &PriVolDesc.ExpireTime,    &Buffer[ 822 ], 17 );
				memcpy( &PriVolDesc.EffectiveTime, &Buffer[ 838 ], 17 );

				memcpy( &PriVolDesc.ApplicationBytes, &Buffer[ 856 ], 512 );

				// Terminate some strings
				ISOStrTerm( PriVolDesc.SysID, 32 );
				ISOStrTerm( PriVolDesc.VolID, 32 );
				ISOStrTerm( PriVolDesc.VolSetID, 128 );
				ISOStrTerm( PriVolDesc.Publisher, 128 );
				ISOStrTerm( PriVolDesc.Preparer, 128 );
				ISOStrTerm( PriVolDesc.AppID, 128 );
				ISOStrTerm( PriVolDesc.CopyrightFilename, 37 );
				ISOStrTerm( PriVolDesc.AbstractFilename, 37 );
				ISOStrTerm( PriVolDesc.CreationTime, 17 );
				ISOStrTerm( PriVolDesc.ModTime, 17 );
				ISOStrTerm( PriVolDesc.ExpireTime, 17 );
				ISOStrTerm( PriVolDesc.EffectiveTime, 17 );

				PriVolDesc.SourceSector = Sect;
				PriVolDesc.IsJoliet     = false;
			}
		}

		Sect++;
	}

	if ( pPathTable != nullptr )
	{
		delete pPathTable;
	}

	pPathTable = new ISOPathTable( pSource, PriVolDesc.SectorSize );

	pPathTable->ReadPathTable( PriVolDesc.LTableLoc, PriVolDesc.PathTableSize, FSID );
}

int ISO9660FileSystem::Init( void )
{
	pISODir->ProcessFOP = ProcessFOP;
	pISODir->pSrcFS     = (void *) this;
	pISODir->FSID       = FSID;

	HasJoliet   = false;
	UsingJoliet = false;

	ReadVolumeDescriptors();

	// We should have the PVD by now. Do some interpretation of the superroot directory
	DWORD RootLBA = * (DWORD *) &PriVolDesc.DirectoryRecord[  2 ];
	DWORD RootSz  = * (DWORD *) &PriVolDesc.DirectoryRecord[ 10 ];
	BYTE  Flags   = PriVolDesc.DirectoryRecord[ 25 ];

	if ( FSID == FSID_ISOHS )
	{
		Flags = PriVolDesc.DirectoryRecord[ 24 ];
	}

	if ( ( Flags & 2 ) == 0 )
	{
		// Err.... the superroot isn't a directory. That's a problem.
		return NUTSError( 0xC01, L"SuperRoot on Primary Volume doesn't list a directory." );
	}

	pISODir->DirSector = RootLBA;
	pISODir->DirLength = RootSz;

	CDPath = "/";

	DWORD JRootLBA = * (DWORD *) &JolietDesc.DirectoryRecord[  2 ];
	DWORD JRootSz  = * (DWORD *) &JolietDesc.DirectoryRecord[ 10 ];

	pISODir->JDirSector = JRootLBA;
	pISODir->JDirLength = JRootSz;

	pISODir->ParentSector = RootLBA;
	pISODir->ParentLength = RootSz;

	pDirectory->ReadDirectory();

	return 0;
}

int ISO9660FileSystem::ChangeDirectory( DWORD FileID ) {
	if ( !IsRoot() )
	{
		CDPath += "/";
	}

	pISODir->Push(
		pDirectory->Files[ FileID ].Attributes[ ISOATTR_START_EXTENT ],
		(DWORD) pDirectory->Files[ FileID ].Length
	);

	CDPath += std::string( (char *) (BYTE *) pDirectory->Files[ FileID ].Filename );

	return pDirectory->ReadDirectory();
}

int ISO9660FileSystem::Parent()
{
	if ( !IsRoot() )
	{
		CDPath = CDPath.substr( 0, CDPath.find_last_of( '/' ) );

		pISODir->Pop();

		if ( IsRoot() )
		{
			CDPath = "/";
		}

		return pDirectory->ReadDirectory();
	}
	
	return NUTS_SUCCESS;
}

bool ISO9660FileSystem::IsRoot() {
	DWORD RootLBA  = * (DWORD *) &PriVolDesc.DirectoryRecord[  2 ];
	DWORD JRootLBA = * (DWORD *) &JolietDesc.DirectoryRecord[  2 ];

	if ( ( pISODir->DirSector == RootLBA ) && ( !UsingJoliet ) )
	{
		return true;
	}

	if ( ( pISODir->JDirSector == JRootLBA ) && ( UsingJoliet ) )
	{
		return true;
	}

	return false;
}

int ISO9660FileSystem::ReadFile(DWORD FileID, CTempFile &store) {
	QWORD Loc = pDirectory->Files[ FileID ].Attributes[ ISOATTR_START_EXTENT ];
	
	Loc *= PriVolDesc.SectorSize;

	DWORD BytesToGo = (DWORD) pDirectory->Files[ FileID ].Length;

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

int ISO9660FileSystem::ReadFork( DWORD FileID, WORD ForkID, CTempFile &forkObj )
{
	if ( ( FileID >= pDirectory->Files.size() ) || ( pDirectory->Files[ FileID ].ExtraForks == 0 ) )
	{
		return NUTSError( 0x808, L"No fork to read" );
	}

	QWORD Loc = pDirectory->Files[ FileID ].Attributes[ ISOATTR_FORK_EXTENT ];
	
	Loc *= PriVolDesc.SectorSize;

	DWORD BytesToGo = pDirectory->Files[ FileID ].Attributes[ ISOATTR_FORK_LENGTH ];

	AutoBuffer Buffer( PriVolDesc.SectorSize );

	forkObj.Seek( 0 );

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( PriVolDesc.SectorSize, BytesToGo );

		if ( pSource->ReadRaw( Loc, BytesRead, (BYTE *) Buffer ) != DS_SUCCESS )
		{
			return -1;
		}

		forkObj.Write( (BYTE *) Buffer, BytesRead );

		BytesToGo -= BytesRead;
		Loc       += BytesRead;
	}

	return NUTS_SUCCESS;
}

int ISO9660FileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	WRITABLE_TEST();

	// Take a copy of pFile so that we don't upset FileOps.
	NativeFile *pOrig   = pFile;
	NativeFile FileCopy = *pFile;

	pFile = &FileCopy;

	// We need to test a few things first:
	DWORD NeededSectors1 = ( (DWORD) pFile->Length + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;
	DWORD NeededSectors2 = 0;
	
	if ( Forks.size() > 0 )
	{
		NeededSectors2 = ( (DWORD) Forks[ 0 ]->Ext() + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;
	}

	DWORD TotalNeeded = NeededSectors1 + NeededSectors2;

	if ( FreeSectors < TotalNeeded )
	{
		return NUTSError( 0x808, L"Insufficient free space in the image. If you have not already done so, please set the anticipated disc capacity using the context menu." );
	}

	FOPData fop;

	BYTE SUSPData[ 256 ];

	fop.DataType  = FOP_DATATYPE_CDISO;
	fop.Direction = FOP_WriteEntry;
	fop.lXAttr    = 256;
	fop.pXAttr    = SUSPData;
	fop.pFile     = (void *) pFile;
	fop.pFS       = (void *) this;
	
	// If there is existing data, we need to import it and process it if it came from an ISO. Otherwise,
	// we must disregard it and create our own if FOP processed the data.
	if ( !ProcessFOP( &fop ) )
	{
		if ( FSID != FSID_ISOHS )
		{
			// Generate Rock Ridge SUSP
			MakeRockRidge( pFile );
		}
		else
		{
			pFile->pAuxData = nullptr;
			pFile->lAuxData = 0;
		}
	}
	else
	{
		// The pointers in the object belong to the source, so don't free them. Make a copy
		if ( fop.lXAttr != 0 )
		{
			pFile->pAuxData = new BYTE[ fop.lXAttr ];
			pFile->lAuxData = fop.lXAttr;

			memcpy( pFile->pAuxData, fop.pXAttr, fop.lXAttr );
		}
	}

	// Blip the AvoidSidecar bit back to the original so FileOps doesn't get narked.
	if ( pFile->Flags & FF_AvoidSidecar )
	{
		pOrig->Flags |= FF_AvoidSidecar;
	}

	// Fill in other ISO attrs
	pFile->Attributes[ ISOATTR_VOLSEQ ] = 1;
	pFile->Attributes[ ISOATTR_START_EXTENT ] = PriVolDesc.VolSize;
		
	if ( Forks.size() > 0 )
	{
		pFile->Attributes[ ISOATTR_FORK_EXTENT ] = PriVolDesc.VolSize + NeededSectors1;
		pFile->Attributes[ ISOATTR_FORK_LENGTH ] = (DWORD) Forks[ 0 ]->Ext();
	}

	if ( pFile->EncodingID != ENCODING_ASCII )
	{
		// Plugins that use FOP to translate the file will set the encoding back to ASCII.
		delete pFile->pAuxData;

		pFile->pAuxData = nullptr;

		return FILEOP_NEEDS_ASCII;
	}

	if ( pFile->FSFileTypeX != FT_ISO )
	{
		pFile->Attributes[ ISOATTR_TIMESTAMP ] = (DWORD) time( NULL );
		pFile->Attributes[ ISOATTR_REVISION  ] = 1;
	}

	// We'll push this onto the directory pre-emptively, but any early return requires popping it back
	// otherwise the directory is ... weird.
	pDirectory->Files.push_back( *pFile );

	int Conflict = FilenameConflictCheck( FILEOP_ISFILE );
	
	if ( Conflict != NUTS_SUCCESS )
	{
		delete pFile->pAuxData;

		pFile->pAuxData = nullptr;

		pDirectory->Files.pop_back();

		return Conflict;
	}

	if ( DirectoryResize() != NUTS_SUCCESS )
	{
		delete pFile->pAuxData;

		pFile->pAuxData = nullptr;

		pDirectory->Files.pop_back();

		return -1;
	}

	DWORD StartSect = pDirectory->Files.back().Attributes[ ISOATTR_START_EXTENT ];

	if ( pDirectory->WriteDirectory() != NUTS_SUCCESS )
	{
		delete pFile->pAuxData;

		pFile->pAuxData = nullptr;

		pDirectory->Files.pop_back();

		return -1;
	}

	AutoBuffer DataSector( PriVolDesc.SectorSize );

	for ( int f = 0; f < ( 1 + pFile->ExtraForks ); f++ )
	{
		CTempFile *src = &store;

		if ( f > 0 )
		{
			src = Forks[ f - 1 ];
		}

		DWORD BytesToGo = (DWORD) src->Ext();
		
		src->Seek( 0 );

		while ( BytesToGo > 0 )
		{
			DWORD BytesLeft = min( BytesToGo, PriVolDesc.SectorSize );

			ZeroMemory( (BYTE *) DataSector, PriVolDesc.SectorSize );

			src->Read( (BYTE *) DataSector, BytesLeft );

			SetISOHints( pSource, ( BytesToGo <= PriVolDesc.SectorSize ), ( BytesToGo <= PriVolDesc.SectorSize ) );

			if ( pSource->WriteSectorLBA( StartSect, (BYTE *) DataSector, PriVolDesc.SectorSize ) != DS_SUCCESS )
			{
				return -1;
			}

			BytesToGo -= BytesLeft;
			StartSect++;
		}
	}

	(void) ISOExtendDataArea( pSource, PriVolDesc.SectorSize, TotalNeeded, FSID );

	FreeSectors -= TotalNeeded;

	ReadVolumeDescriptors();

	return pDirectory->ReadDirectory();
}

int ISO9660FileSystem::Rename( DWORD FileID, BYTE *NewName, BYTE *NewExt )
{
	// This is a copy of the underlying Rename function, but because the filename
	// length is dynamically sized in the directory entry, renaming a file might
	// cause us to need another sector.

	WRITABLE_TEST();
	
	for ( NativeFileIterator iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		bool Same = false;

		if ( ( rstrcmp( iFile->Filename, NewName ) ) && ( FileID != iFile->fileID ) )
		{
			if ( iFile->Flags & FF_Extension )
			{
				if ( ( rstrcmp( iFile->Extension, NewExt ) ) && ( FileID != iFile->fileID ) )
				{
					Same = true;
				}
			}
			else
			{
				Same = true;
			}
		}

		if ( Same )
		{
			return NUTSError( 0x206, L"An object with that name already exists" );
		}
	}

	pDirectory->Files[ FileID ].Filename = BYTEString( NewName );

	if ( pDirectory->Files[ FileID ].Flags & FF_Extension )
	{
		if ( rstrnlen( NewExt, 256 ) == 0 )
		{
			pDirectory->Files[ FileID ].Flags &= ~FF_Extension;
		}
		else
		{
			pDirectory->Files[ FileID ].Extension = BYTEString( NewExt );
		}
	}
	else
	{
		if ( rstrnlen( NewExt, 256 ) != 0 )
		{
			if ( Flags & FSF_Uses_Extensions )
			{
				if ( pDirectory->Files[ FileID ].Flags & FF_Directory )
				{
					if ( ! ( Flags & FSF_NoDir_Extensions ) )
					{
						pDirectory->Files[ FileID ].Flags |= FF_Extension;
						pDirectory->Files[ FileID ].Extension = BYTEString( NewExt );
					}
				}
				else
				{
					pDirectory->Files[ FileID ].Flags |= FF_Extension;
					pDirectory->Files[ FileID ].Extension = BYTEString( NewExt );
				}
			}
		}
	}

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	FOPData fop;

	BYTE SUSPData[ 256 ];

	fop.DataType  = FOP_DATATYPE_CDISO;
	fop.Direction = FOP_WriteEntry;
	fop.lXAttr    = 256;
	fop.pXAttr    = SUSPData;
	fop.pFile     = (void *) pFile;
	fop.pFS       = (void *) this;
	
	// This is slightly different to WriteFile/CreateDirectory as the aux data is ours, so we must
	// free it first.
	if ( pFile->pAuxData != nullptr )
	{
		delete pFile->pAuxData;

		pFile->pAuxData = nullptr;
		pFile->lAuxData = 0;
	}

	if ( !ProcessFOP( &fop ) )
	{
		if ( FSID != FSID_ISOHS )
		{
			// Generate Rock Ridge SUSP
			MakeRockRidge( pFile );
		}
		else
		{
			pFile->pAuxData = nullptr;
			pFile->lAuxData = 0;
		}
	}
	else
	{
		if ( fop.lXAttr != 0 )
		{
			pFile->pAuxData = new BYTE[ fop.lXAttr ];
			pFile->lAuxData = fop.lXAttr;

			memcpy( pFile->pAuxData, fop.pXAttr, fop.lXAttr );
		}
	}

	DirectoryShrink();

	if ( DirectoryResize() != NUTS_SUCCESS )
	{
		// Read the Directory back to resync it
		(void) pDirectory->ReadDirectory();

		return -1;
	}

	int r = pDirectory->WriteDirectory();

	if ( RemoveSectors() != NUTS_SUCCESS )
	{
		// Read the Directory back to resync it
		(void) pDirectory->ReadDirectory();

		return -1;
	}

	if ( r == NUTS_SUCCESS )
	{
		r = pDirectory->ReadDirectory();
	}

	return r;
}

BYTE *ISO9660FileSystem::GetTitleString( NativeFile *pFile, DWORD Flags ) {
	static BYTE ISOPath[512];

	rstrncpy( ISOPath, (BYTE *) "CDROM:", 511 );

	if ( UsingJoliet )
	{
		rstrncat( ISOPath, JolietDesc.VolID, 511 );
	}
	else
	{
		rstrncat( ISOPath, PriVolDesc.VolID, 511 );
	}

	rstrncat( ISOPath, (BYTE *) ":", 511 );

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

	WORD Offset = 0;

	if ( HasJoliet )
	{
		if ( UsingJoliet )
		{
			rsprintf( status, "[Joliet]  " );
		}
		else
		{
			rsprintf( status, "[ISO9660] " );
		}

		Offset = 10;
	}

	if ( SelectedItems == 0 )
	{
		rsprintf( &status[ Offset ], "%d File System Objects", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( &status[ Offset ], "%d Items Selected", SelectedItems );
	}
	else if ( pISODir->FileFOPData.find( FileIndex ) != pISODir->FileFOPData.end() )
	{
		rstrncpy( &status[ Offset ], pISODir->FileFOPData[ FileIndex ].StatusString, 128 );
	}
	else
	{
		NativeFile *pFile = &pDirectory->Files[FileIndex];

		if ( pFile->Flags & FF_Directory )
		{
			rsprintf( &status[ Offset ], "%s, Directory", (BYTE *) pFile->Filename	);
		}
		else
		{
			if ( pFile->Flags & FF_Extension )
			{
				rsprintf( &status[ Offset ], "%s.%s, %d bytes",
					(BYTE *) pFile->Filename, (BYTE *) pFile->Extension, (DWORD) pFile->Length
				);
			}
			else
			{
				rsprintf( &status[ Offset ], "%s, %d bytes",
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

	/* Timestamp. Enabled */
	Attr.Index = 20;
	Attr.Type  = AttrVisible | AttrEnabled | AttrTime | AttrFile | AttrDir;
	Attr.Name  = L"Time Stamp";
	Attrs.push_back( Attr );

	return Attrs;
}

LocalCommands ISO9660FileSystem::GetLocalCommands( void )
{
	LocalCommand c1 = { LC_Always, L"Browse Joliet Tree" }; 
	LocalCommand s  = { LC_IsSeparator, L"" };
	LocalCommand c2 = { LC_Always, L"Add Joliet Volume Descriptor" }; 
	LocalCommand c3 = { LC_Always, L"Edit Volume Descriptors" };
	// --
	LocalCommand c4 = { LC_Always, L"Set Maximum Capacity" };
	LocalCommand c5 = { LC_Always, L"Enable Writing" };

	if ( HasJoliet )
	{
		c2.Flags = LC_Always | LC_Disabled;
	
		if ( UsingJoliet )
		{
			c1.Name = L"Browse ISO Tree";
		}
	}
	else
	{
		c1.Flags = LC_Always | LC_Disabled;
	}

	if ( !Writable )
	{
		c4.Flags = LC_Always | LC_Disabled;
	}

	if ( pSource->Flags & DS_ReadOnly )
	{
		c3.Flags = LC_Always | LC_Disabled;
		c4.Flags = LC_Always | LC_Disabled;
		c5.Flags = LC_Always | LC_Disabled;
	}

	if ( Writable )
	{
		c5.Flags |= LC_Disabled;
	}

	LocalCommands cmds;

	cmds.HasCommandSet = true;

	if ( FSID == FSID_ISO9660 )
	{
		cmds.Root = L"ISO-9660 CD-ROM";
	}

	if ( FSID == FSID_ISOHS )
	{
		cmds.Root = L"High Sierra CD-ROM";
	}

	cmds.CommandSet.push_back( c1 );
	cmds.CommandSet.push_back( s );
	cmds.CommandSet.push_back( c2 );
	cmds.CommandSet.push_back( c3 );
	cmds.CommandSet.push_back( s );
	cmds.CommandSet.push_back( c4 );
	cmds.CommandSet.push_back( c5 );

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

	DWORD UsedBlocks = PriVolDesc.VolSize;
	DWORD NumBlocks  = UsedBlocks + FreeSectors;

	Map.Capacity  = (QWORD) NumBlocks * (QWORD) PriVolDesc.SectorSize;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = (QWORD) UsedBlocks * (QWORD) PriVolDesc.SectorSize;

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

	for ( DWORD FreeBlk=UsedBlocks; FreeBlk != NumBlocks; FreeBlk++ )
	{
		BlkNum = (DWORD) ( (double) FreeBlk / BlockRatio );
	
		pBlockMap[ BlkNum ] = (BYTE) BlockFree;
	}

	SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

	return 0;
}

int ISO9660FileSystem::ExecLocalCommand( DWORD CmdIndex, std::vector<NativeFile> &Selection, HWND hParentWnd )
{
	if ( CmdIndex == 0 )
	{
		UsingJoliet = !UsingJoliet;

		pISODir->UsingJoliet = UsingJoliet;

		if ( UsingJoliet )
		{
			DWORD JRootLBA = * (DWORD *) &JolietDesc.DirectoryRecord[  2 ];
			DWORD JRootSz  = * (DWORD *) &JolietDesc.DirectoryRecord[ 10 ];

			pISODir->JDirSector = JRootLBA;
			pISODir->JDirLength = JRootSz;
		}
		else
		{
			DWORD RootLBA = * (DWORD *) &PriVolDesc.DirectoryRecord[  2 ];
			DWORD RootSz  = * (DWORD *) &PriVolDesc.DirectoryRecord[ 10 ];

			pISODir->DirSector = RootLBA;
			pISODir->DirLength = RootSz;
		}

		return CMDOP_REFRESH;
	}

	if ( CmdIndex == 2 ) // Add/remove Joliet
	{
		EnableWriting();

		if ( HasJoliet )
		{
			RemoveJoliet();
		}
		else
		{
			AddJoliet();
		}

		Writable = false;

		Init();
	}

	if ( CmdIndex == 3 )
	{
		EditVolumeDescriptors( hParentWnd );

		return CMDOP_REFRESH;
	}

	if ( CmdIndex == 5 )
	{
		if ( !Writable )
		{
			(void) WritableTest();
		}
		else
		{
			SetMaxCapacity();
		}
	}

	if ( CmdIndex == 6 )
	{
		(void) WritableTest();

		if ( UsingJoliet )
		{
			UsingJoliet = false;

			pISODir->UsingJoliet = false;

			DWORD RootLBA = * (DWORD *) &PriVolDesc.DirectoryRecord[  2 ];
			DWORD RootSz  = * (DWORD *) &PriVolDesc.DirectoryRecord[ 10 ];

			pISODir->DirSector = RootLBA;
			pISODir->DirLength = RootSz;
		}

		pDirectory->ReadDirectory();

		return CMDOP_REFRESH;
	}

	return 0;
}

void JolietScanDirs( FileSystem *pFS, ISOSectorList &sectors, ISOVolDesc &JolietDesc )
{
	for ( DWORD i = 0; i < pFS->pDirectory->Files.size(); i++ )
	{
		// DANGER WILL ROBINSON!
		// References to directories will /always/ point to some Joliet-only blob. But a reference to a /file/
		// might point to something pointed to by something in the /ISO/ structure. We must leave it alone,
		// and hope that it doesn't get orphaned as a result. If it does, the user can use the "remove unused sectors"
		// tool to clear it.

		NativeFile *iFile = &pFS->pDirectory->Files[ i ];

		if ( iFile->Flags & FF_Directory )
		{
			ISOSector sector;

			DWORD DirSize = iFile->Length;
			DWORD DirLoc  = iFile->Attributes[ ISOATTR_START_EXTENT ];

			DWORD DirSects = ( DirSize + ( JolietDesc.SectorSize - 1 ) ) / JolietDesc.SectorSize;

			for ( DWORD i=0; i<DirSects; i++ )
			{
				sector.ID = DirLoc + i; sectors.push_back( sector );
			}

			pFS->ChangeDirectory( i );

			JolietScanDirs( pFS, sectors, JolietDesc );

			pFS->Parent();
		}
	}
}

void ISO9660FileSystem::RemoveJoliet()
{
	// Oooh this is going to be very messy.
	ISOSectorList sectors;

	ISOSector sector;

	sector.ID = JolietDesc.SourceSector; sectors.push_back( sector );

	// Add the path tables
	DWORD PathTableSects = ( JolietDesc.PathTableSize + ( JolietDesc.SectorSize - 1 ) ) / JolietDesc.SectorSize;

	for ( DWORD i=0; i<PathTableSects; i++ )
	{
		sector.ID = JolietDesc.LTableLoc + i; sectors.push_back( sector );
		sector.ID = JolietDesc.MTableLoc + i; sectors.push_back( sector );

		if ( JolietDesc.LTableLocOpt != 0 )
		{
			sector.ID = JolietDesc.LTableLocOpt + i; sectors.push_back( sector );
		}

		if ( JolietDesc.MTableLocOpt != 0 )
		{
			sector.ID = JolietDesc.MTableLocOpt + i; sectors.push_back( sector );
		}
	}

	// Now the icky part. Traversing the Joliet Structure.
	FileSystem *pFS = Clone();

	if ( !UsingJoliet )
	{
		// Put it in Joliet mode if we weren't.
		std::vector<NativeFile> dummy;

		pFS->ExecLocalCommand( 0, dummy, NULL );
	}

	// Get to root
	while ( !pFS->IsRoot() ) { if ( pFS->Parent() != NUTS_SUCCESS ) { return; } }

	// Add the root directory itself
	DWORD RootSize = LEDWORD( &JolietDesc.DirectoryRecord[ 10 ] );
	DWORD RootLoc  = LEDWORD( &JolietDesc.DirectoryRecord[  2 ] );

	DWORD RootSects = ( RootSize + ( JolietDesc.SectorSize - 1 ) ) / JolietDesc.SectorSize;

	for ( DWORD i=0; i<RootSects; i++ )
	{
		sector.ID = RootLoc + i; sectors.push_back( sector );
	}

	// And now, we traverse.
	JolietScanDirs( pFS, sectors, JolietDesc );

	// Blank out the Joliet descriptor sector, otherwise ISO restructure will try to restructure itself.
	BYTE FakeDescriptor[ 7 ] = { 0x03, 'C', 'D', '0', '0', '1', 0x01 };

	AutoBuffer secbuf( JolietDesc.SectorSize );

	if ( pSource->ReadSectorLBA( JolietDesc.SourceSector, (BYTE *) secbuf, JolietDesc.SectorSize ) != DS_SUCCESS )
	{
		NUTSError::Report( L"Removing Joliet structure", hMainWnd );

		return;
	}

	memcpy( (BYTE *) secbuf, FakeDescriptor, 7 );

	if ( pSource->WriteSectorLBA( JolietDesc.SourceSector, (BYTE *) secbuf, JolietDesc.SectorSize ) != DS_SUCCESS )
	{
		NUTSError::Report( L"Removing Joliet structure", hMainWnd );

		return;
	}

	// Now we have all the sectors. Remove them.
	ISOJob job;

	PrepareISOSectors( sectors );

	job.FSID       = FSID;
	job.IsoOp      = ISO_RESTRUCTURE_REM_SECTORS;
	job.pSource    = pSource;
	job.pVolDesc   = &PriVolDesc; // Because reasons
	job.Sectors    = sectors;
	job.SectorSize = PriVolDesc.SectorSize;

	if ( PerformISOJob( &job ) != NUTS_SUCCESS )
	{
		NUTSError::Report( L"Removing Joliet structure", hMainWnd );

		return;
	}

	Init();
}

bool ISO9660FileSystem::WritableTest()
{
	if ( pSource->Flags & DS_ReadOnly )
	{
		NUTSError err( 0x801, L"The data source is not writable" );
	}
	else if ( !Writable )
	{
		if ( MessageBox( hMainWindow,
			L"The image is current in read-only mode for safety. The ISO9660/High Sierra file system "
			L"was not designed to be used as a general purpose read/write file system, and as such "
			L"enabling writing mode may result in unexpected results if the image uses advanced features "
			L"such as multiple volumes. You should make sure you have a backup before proceeding.\n\n"
			L"If this is a newly created image, then proceeding is safe.\n\n"
			L"Do you wish to enable writing to this image?",
			L"NUTS ISO9660/High Sierra File System",
			MB_ICONEXCLAMATION | MB_YESNO ) == IDYES )
		{
			if ( HasJoliet )
			{
				if ( MessageBox( hMainWindow,
					L"The image has a Joliet volume descriptor and associated tree. It is not possible for "
					L"NUTS to correctly identify objects in the Joliet descriptor as matching objects in "
					L"the ISO descriptor. It is also possible the Joliet structure is completely different.\n\n"
					L"To proceed, NUTS must remove the Joliet structure completely. Depending on how much the "
					L"Joliet structure differs from the ISO structure, this may result in sectors belonging to "
					L"objects only recorded in the Joliet structure becoming orphaned and inaccessible. This "
					L"may also result in the image being unsuable if it is a commerical release or an image "
					L"downloaded from the internet.\n\n"
					L"You can add the Joliet structure back once you are done writing, but the structure may not "
					L"be exactly the same as it is now. Doing so will also disable write mode.\n\n"
					L"Are you sure you wish to proceed enabling writing?",
					L"NUTS ISO9660/High Sierra File System",
					MB_ICONEXCLAMATION | MB_YESNO ) == IDYES )
				{
					RemoveJoliet();
				}
				else
				{
					NUTSError err( 0x803, L"The data source is not writable" );
				}
			}

			if ( !HasJoliet )
			{
				EnableWriting();

				return true;
			}
		}
		else
		{
			NUTSError err( 0x802, L"The data source is not writable" );
		}
	}
	else
	{
		return true;
	}

	return false;
}

int ISO9660FileSystem::FilenameConflictCheck( int ExistsCode, NativeFile *pConflictor )
{
	// Checks to see if the filename at the back of the list is in conflict with anything else.
	// First checks the filename itself, then does an ISO conversion and then recheck.
	
	// ExistsCode is FILEOP_ISFILE or FILEOP_ISDIR, which is used to ensure the right
	// return code is used. If everything passes, this functionr eturns NUTS_SUCCESS;

	if ( pDirectory->Files.size() < 1 )
	{
		// ?
		return NUTS_SUCCESS; // I guess....
	}

	NativeFile *pIntended = &pDirectory->Files.back();

	if ( pConflictor != nullptr )
	{
		pIntended = pConflictor;
	}

	// First a direct check of the filename
	for ( int i=0; i<pDirectory->Files.size() - 1; i++ )
	{
		if ( FilenameCmp( &pDirectory->Files[ i ], pIntended ) )
		{
			// EEP! Clash!
			if ( pDirectory->Files[ i ].Flags & FF_Directory )
			{
				if ( ExistsCode == FILEOP_ISFILE )
				{
					ConflictedID = pDirectory->Files[ i ].fileID;

					return FILEOP_ISFILE;
				}
				else
				{
					ConflictedID = pDirectory->Files[ i ].fileID;

					return FILEOP_EXISTS;
				}
			}
			else
			{
				if ( ExistsCode == FILEOP_ISDIR )
				{
					ConflictedID = pDirectory->Files[ i ].fileID;

					return FILEOP_ISDIR;
				}
				else
				{
					ConflictedID = pDirectory->Files[ i ].fileID;

					return FILEOP_EXISTS;
				}
			}
		}
	}

	BYTE IntendedFilename[ 256 ];
	BYTE ThisFilename[ 256 ];

	MakeISOFilename( pIntended, IntendedFilename );

	// Now ISO-ify the filename, and recheck
	for ( int i=0; i<pDirectory->Files.size() - 1; i++ )
	{
		MakeISOFilename( &pDirectory->Files[ i ], ThisFilename );

		if ( rstricmp( IntendedFilename, ThisFilename ) )
		{
			// EEP! Clash!
			if ( pDirectory->Files[ i ].Flags & FF_Directory )
			{
				if ( ExistsCode == FILEOP_ISFILE )
				{
					ConflictedID = pDirectory->Files[ i ].fileID;

					return FILEOP_ISFILE;
				}
				else
				{
					ConflictedID = pDirectory->Files[ i ].fileID;

					return FILEOP_EXISTS;
				}
			}
			else
			{
				if ( ExistsCode == FILEOP_ISDIR )
				{
					ConflictedID = pDirectory->Files[ i ].fileID;

					return FILEOP_ISDIR;
				}
				else
				{
					ConflictedID = pDirectory->Files[ i ].fileID;

					return FILEOP_EXISTS;
				}
			}
		}
	}

	return NUTS_SUCCESS;
}

#define MAKE_RR_TAG(a,b) ( ( a << 8 ) | b )

void ISO9660FileSystem::MakeRockRidge( NativeFile *pFile )
{
	// We're going to make a new Rock Ridge block from the existing one. We'll stop
	// if we reach a tag that makes no sense as an RR tag, or if we run out of SUSP.
	BYTE *pNewAux = new BYTE[ 256 ];

	DWORD lNewAux = 0;

	DWORD pAuxP = 0;
	DWORD pAuxO = 0;

	// We'll skip NM (alternative name) and fill that in ourselves.
	while ( pAuxP < pFile->lAuxData )
	{
		WORD  tag = BEWORD( &pFile->pAuxData[ pAuxP ] );
		BYTE lTag = pFile->pAuxData[ pAuxP + 2 ];

		if ( ( pAuxP + 1 ) > pFile->lAuxData )
		{
			break;
		}

		if ( ( pFile->pAuxData[ pAuxP ] < 'A' ) || ( pFile->pAuxData[ pAuxP ] > 'Z' ) || ( pFile->pAuxData[ pAuxP + 1 ] < 'A' ) || ( pFile->pAuxData[ pAuxP + 1 ] > 'Z' ) )
		{
			break;
		}

		if ( ( pAuxP + lTag ) > pFile->lAuxData )
		{
			break;
		}

		if ( tag != MAKE_RR_TAG( 'N', 'M' ) )
		{
			memcpy( &pNewAux[ pAuxO ], &pFile->pAuxData[ pAuxP ], lTag );

			pAuxO += lTag;
		}

		pAuxP += lTag;

		if ( pAuxO > 255 )
		{
			break;
		}
	}

	pFile->pAuxData = nullptr;
	pFile->lAuxData = 0;

	// Now we'll tack NM on the end, regardless of what was there before
	if ( pAuxO < 255 )
	{
		DWORD FBytes = (DWORD) pFile->Filename.length();

		if ( pFile->Flags & FF_Extension )
		{
			FBytes++; // '.'

			FBytes += (DWORD) pFile->Extension.length();
		}

		FBytes += 5;

		if ( ( pAuxO + FBytes ) > 250 )
		{
			// Um.. not enough room
			FBytes = 255 - pAuxO;
		}

		pNewAux[ pAuxO + 0 ] = 'N';
		pNewAux[ pAuxO + 1 ] = 'M';
		pNewAux[ pAuxO + 2 ] = (BYTE) FBytes;
		pNewAux[ pAuxO + 3 ] = 1;
		pNewAux[ pAuxO + 4 ] = 0;

		BYTE FName[ 256 ];

		rstrncpy( FName, pFile->Filename, (WORD) FBytes );

		if ( pFile->Flags & FF_Extension )
		{
			rstrncat( FName, (BYTE *) ".", (WORD) FBytes );
			rstrncat( FName, pFile->Extension, (WORD) FBytes );
		}

		rstrncpy( &pNewAux[ pAuxO + 5 ], FName, FBytes );

		pAuxO += rstrnlen( FName, 255 ) + 5;

		pFile->pAuxData = pNewAux;
		pFile->lAuxData = pAuxO;

		if ( pAuxO & 1 )
		{
			pFile->lAuxData++;
		}
	}
	else
	{
		delete pNewAux;
	}
}

void ISO9660FileSystem::EnableWriting()
{
	Writable = true;

	ISOJob job;

	job.FSID     = FSID;
	job.IsoOp    = ISO_JOB_SCAN_SIZE;
	job.pSource  = pSource;
	job.pVolDesc = &PriVolDesc;

	PerformISOJob( &job );

	SetMaxCapacity();
}

void ISO9660FileSystem::SetMaxCapacity()
{
	DWORD TotalSectors = ISOChooseCapacity();

	if ( TotalSectors != 0 )
	{
		if ( TotalSectors > PriVolDesc.VolSize )
		{
			FreeSectors = TotalSectors - PriVolDesc.VolSize;
		}
		else
		{
			FreeSectors = 0;
		}
	}
	else
	{
		FreeSectors = 0;
	}
}

// Resizes the directory on disk if needed and adjusts all the internal structures.
int ISO9660FileSystem::DirectoryResize( ISOSectorList *ExtraSectors )
{
	DWORD DirectoryAlloc   = ( pISODir->DirLength + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;
	DWORD DirectorySectors = pISODir->ProjectedDirectorySize();

	if ( ( DirectoryAlloc < DirectorySectors ) || ( ( ExtraSectors != nullptr )  && ( ExtraSectors->size() > 0 ) ) )
	{
		ISOSectorList NewSectors;

		if ( ExtraSectors != nullptr )
		{
			NewSectors = *ExtraSectors;
		}

		for ( DWORD n=0; n< ( DirectorySectors - DirectoryAlloc ); n++ )
		{
			ISOSector sector;

			sector.ID = pISODir->DirSector + DirectoryAlloc + n;

			NewSectors.push_back( sector );
		}

		// Recheck free space for the extra sectors needed
		if ( FreeSectors < NewSectors.size() )
		{
			return NUTSError( 0x808, L"Insufficient free space in the image. If you have not already done so, please set the anticipated disc capacity using the context menu." );
		}

		PrepareISOSectors( NewSectors );

		ISOJob job;

		job.FSID       = FSID;
		job.IsoOp      = ISO_RESTRUCTURE_ADD_SECTORS;
		job.pSource    = pSource;
		job.Sectors    = NewSectors;
		job.SectorSize = PriVolDesc.SectorSize;

		if ( PerformISOJob( &job ) != NUTS_SUCCESS )
		{
			return -1;
		}

		FreeSectors -= NewSectors.size();

		if ( DirectorySectors != DirectoryAlloc )
		{
			// Adjust the size of this directory in the parent
			if ( IsRoot() )
			{
				// The "parent" is the super root in the PVD.
				ISOSetRootSize( pSource, PriVolDesc.SectorSize, PriVolDesc.SectorSize * DirectorySectors, FSID );
			}
			else
			{
				ISOSetDirSize( pSource, PriVolDesc.SectorSize, PriVolDesc.SectorSize * DirectorySectors, pISODir->ParentSector, pISODir->ParentLength, pISODir->DirSector, FSID );
			}
		}

		// Volume size changed, so read this back in
		ReadVolumeDescriptors();

		// Adjust the directory specs
		for ( NativeFileIterator iRefile = pDirectory->Files.begin(); iRefile != pDirectory->Files.end(); iRefile++ )
		{
			iRefile->Attributes[ ISOATTR_START_EXTENT ] = RemapSector( iRefile->Attributes[ ISOATTR_START_EXTENT ], NewSectors, ISO_RESTRUCTURE_ADD_SECTORS );
			iRefile->Attributes[ ISOATTR_FORK_EXTENT  ] = RemapSector( iRefile->Attributes[ ISOATTR_FORK_EXTENT ],  NewSectors, ISO_RESTRUCTURE_ADD_SECTORS );
		}

		pISODir->DirSector    = RemapSector( pISODir->DirSector, NewSectors, ISO_RESTRUCTURE_ADD_SECTORS );
		pISODir->ParentSector = RemapSector( pISODir->DirSector, NewSectors, ISO_RESTRUCTURE_ADD_SECTORS );

		RemapStack( NewSectors, ISO_RESTRUCTURE_ADD_SECTORS );
	}

	// Adjust the parent references in the sub directories
	if ( DirectorySectors != DirectoryAlloc )
	{
		for ( NativeFileIterator iRefile = pDirectory->Files.begin(); iRefile != pDirectory->Files.end(); iRefile++ )
		{
			ISOSetSubDirParentSize(
				pSource,
				PriVolDesc.SectorSize,
				PriVolDesc.SectorSize * DirectorySectors,
				pISODir->DirSector,
				pISODir->DirLength,
				iRefile->Attributes[ ISOATTR_START_EXTENT ],
				FSID
			);
		}
	}

	pISODir->DirLength = PriVolDesc.SectorSize * DirectorySectors;

	return NUTS_SUCCESS;
}

int ISO9660FileSystem::CreateDirectory( NativeFile *pDir, DWORD CreateFlags )
{
	// Basically does the same as WriteFile, with a few changes:
	// 1) Extra space must be allocated for any path table extensions
	// 2) The path table must be updated
	// 3) The "file" content is a directory structure
	// 4) The CreateFlags must be handled, including the rename strategy

	WRITABLE_TEST();

	// Take a copy of pFile so that we don't upset FileOps.
	NativeFile FileCopy = *pDir;

	pDir = &FileCopy;

	// An empty directory only requires 1 sector
	if ( FreeSectors < 1U )
	{
		return NUTSError( 0x808, L"Insufficient free space in the image. If you have not already done so, please set the anticipated disc capacity using the context menu." );
	}

	FOPData fop;

	BYTE SUSPData[ 256 ];

	fop.DataType  = FOP_DATATYPE_CDISO;
	fop.Direction = FOP_WriteEntry;
	fop.lXAttr    = 256;
	fop.pXAttr    = SUSPData;
	fop.pFile     = (void *) pDir;
	fop.pFS       = (void *) this;
	
	// If there is existing data, we need to import it and process it if it came from an ISO. Otherwise,
	// we must disregard it and create our own if FOP processed the data.
	if ( !ProcessFOP( &fop ) )
	{
		if ( FSID != FSID_ISOHS )
		{
			// Generate Rock Ridge SUSP
			MakeRockRidge( pDir );
		}
		else
		{
			pDir->pAuxData = nullptr;
			pDir->lAuxData = 0;
		}
	}
	else
	{
		// The pointers in the object belong to the source, so don't free them. Make a copy
		if ( fop.lXAttr != 0 )
		{
			pDir->pAuxData = new BYTE[ fop.lXAttr ];
			pDir->lAuxData = fop.lXAttr;

			memcpy( pDir->pAuxData, fop.pXAttr, fop.lXAttr );
		}
	}

	// Fill in other ISO attrs
	pDir->Attributes[ ISOATTR_VOLSEQ ] = 1;
	pDir->Attributes[ ISOATTR_START_EXTENT ] = PriVolDesc.VolSize;

	pDir->Length = PriVolDesc.SectorSize;
		
	if ( pDir->EncodingID != ENCODING_ASCII )
	{
		// Plugins that use FOP to translate the file will set the encoding back to ASCII.
		delete pDir->pAuxData;

		pDir->pAuxData = nullptr;

		return FILEOP_NEEDS_ASCII;
	}

	if ( pDir->FSFileTypeX != FT_ISO )
	{
		pDir->Attributes[ ISOATTR_TIMESTAMP ] = (DWORD) time( NULL );
		pDir->Attributes[ ISOATTR_REVISION  ] = 1;
	}

	// Need to fix up some flags. Obviously this is a directory, but we also need to remove any extension flag,
	// as ISO9660/High Sierra don't support extensions for directory names.
	pDir->Flags |= FF_Directory;
	pDir->Flags &= ~FF_Extension;

	// We'll push this onto the directory pre-emptively, but any early return requires popping it back
	// otherwise the directory is ... weird.
	pDirectory->Files.push_back( *pDir );

	int Conflict = FilenameConflictCheck( FILEOP_ISDIR );
	
	if ( Conflict != NUTS_SUCCESS )
	{
		if ( ( CreateFlags & ( CDF_MERGE_DIR | CDF_RENAME_DIR ) ) == 0 )
		{
			delete pDir->pAuxData;

			pDir->pAuxData = nullptr;

			pDirectory->Files.pop_back();

			return Conflict;
		}
		else if ( CreateFlags & CDF_MERGE_DIR )
		{
			return ChangeDirectory( ConflictedID );
		}
		else if ( CreateFlags & CDF_RENAME_DIR )
		{
			if ( RenameIncomingDirectory( pDir ) != NUTS_SUCCESS )
			{
				return -1;
			}
		}
	}

	// We now need to do some path table calculations.
	BYTE ISOFilename[256];

	MakeISOFilename( pDir, ISOFilename );

	pPathTable->AddDirectory( BYTEString( ISOFilename ), PriVolDesc.VolSize, pISODir->DirSector );

	DWORD NewTableSize  = pPathTable->GetProjectedSize();
	DWORD NewTableSects = ( NewTableSize + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;
	DWORD OldTableSects = ( PriVolDesc.PathTableSize + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;

	ISOSectorList ExtraSects;

	if ( NewTableSects > OldTableSects )
	{
		ISOSector sect;

		if ( PriVolDesc.LTableLoc != 0 )    { sect.ID = PriVolDesc.LTableLoc    + OldTableSects; ExtraSects.push_back( sect ); }
		if ( PriVolDesc.LTableLocOpt != 0 ) { sect.ID = PriVolDesc.LTableLocOpt + OldTableSects; ExtraSects.push_back( sect ); }
		if ( PriVolDesc.MTableLoc != 0 )    { sect.ID = PriVolDesc.MTableLoc    + OldTableSects; ExtraSects.push_back( sect ); }
		if ( PriVolDesc.MTableLocOpt != 0 ) { sect.ID = PriVolDesc.MTableLocOpt + OldTableSects; ExtraSects.push_back( sect ); }

		if ( FSID == FSID_ISOHS )
		{
			if ( PriVolDesc.LTableLocOptA != 0 ) { sect.ID = PriVolDesc.LTableLocOptA + OldTableSects; ExtraSects.push_back( sect ); }
			if ( PriVolDesc.LTableLocOptB != 0 ) { sect.ID = PriVolDesc.LTableLocOptB + OldTableSects; ExtraSects.push_back( sect ); }
			if ( PriVolDesc.MTableLocOptA != 0 ) { sect.ID = PriVolDesc.MTableLocOptA + OldTableSects; ExtraSects.push_back( sect ); }
			if ( PriVolDesc.MTableLocOptB != 0 ) { sect.ID = PriVolDesc.MTableLocOptB + OldTableSects; ExtraSects.push_back( sect ); }
		}
	}

	if ( DirectoryResize( &ExtraSects ) != NUTS_SUCCESS )
	{
		delete pDir->pAuxData;

		pDir->pAuxData = nullptr;

		pDirectory->Files.pop_back();

		return -1;
	}

	DWORD StartSect = pDirectory->Files.back().Attributes[ ISOATTR_START_EXTENT ];

	if ( pDirectory->WriteDirectory() != NUTS_SUCCESS )
	{
		delete pDir->pAuxData;

		pDir->pAuxData = nullptr;

		pDirectory->Files.pop_back();

		return -1;
	}

	AutoBuffer DataSector( PriVolDesc.SectorSize );

	// The PathTable got re-read in DirectoryResize, so do this step again, and this time actually write the tables out.
	// Note that the ISO-restructure will have shifted the path tables correctly, so the points are now correct.
	// We'll re-read the volume descriptors, in case DirectoryResize was 0.
	ReadVolumeDescriptors();

	pPathTable->AddDirectory( pDir->Filename, PriVolDesc.VolSize, pISODir->DirSector );

	if ( PriVolDesc.LTableLoc != 0 )    { pPathTable->WritePathTable( PriVolDesc.LTableLoc,    false, FSID ); }
	if ( PriVolDesc.LTableLocOpt != 0 ) { pPathTable->WritePathTable( PriVolDesc.LTableLocOpt, false, FSID ); }
	if ( PriVolDesc.MTableLoc != 0 )    { pPathTable->WritePathTable( PriVolDesc.MTableLoc,    true,  FSID  ); }
	if ( PriVolDesc.MTableLocOpt != 0 ) { pPathTable->WritePathTable( PriVolDesc.MTableLocOpt, true,  FSID  ); }

	if ( FSID == FSID_ISOHS )
	{
		if ( PriVolDesc.LTableLocOptA != 0 ) { pPathTable->WritePathTable( PriVolDesc.LTableLocOptA, false, FSID ); }
		if ( PriVolDesc.LTableLocOptA != 0 ) { pPathTable->WritePathTable( PriVolDesc.LTableLocOptB, false, FSID ); }
		if ( PriVolDesc.MTableLocOptB != 0 ) { pPathTable->WritePathTable( PriVolDesc.MTableLocOptA, true,  FSID  ); }
		if ( PriVolDesc.MTableLocOptB != 0 ) { pPathTable->WritePathTable( PriVolDesc.MTableLocOptB, true,  FSID  ); }
	}

	// Now we need to create an empty directory.
	AutoBuffer NewDirSector( PriVolDesc.SectorSize );

	BYTE *pDirsector = (BYTE *) NewDirSector;

	ZeroMemory( pDirsector, PriVolDesc.SectorSize );

	// . = this
	pDirsector[ 0 ] = 0x22; // fixed size
	pDirsector[ 1 ] = 0;
	WLEDWORD( &pDirsector[  2 ], PriVolDesc.VolSize );    WBEDWORD( &pDirsector[  6 ], PriVolDesc.VolSize );
	WLEDWORD( &pDirsector[ 10 ], PriVolDesc.SectorSize ); WBEDWORD( &pDirsector[ 14 ], PriVolDesc.SectorSize );
	pISODir->ConvertUnixTime( &pDirsector[ 18 ], time( NULL ) );
	pDirsector[ 25 ] = 0x2; // Directory
	pDirsector[ 26 ] = 0;
	pDirsector[ 27 ] = 0;
	WBEDWORD( &pDirsector[ 28 ], 0x01000001 );
	pDirsector[ 32 ] = 1;
	pDirsector[ 33 ] = 0;
	pDirsector[ 34 ] = 0;

	if ( FSID == FSID_ISOHS ) { pDirsector[ 24 ] = pDirsector[ 25 ]; pDirsector[ 25 ] = 0; }

	pDirsector += 0x22;

	// .. = parent
	pDirsector[ 0 ] = 0x22; // fixed size
	pDirsector[ 1 ] = 0;
	WLEDWORD( &pDirsector[  2 ], pISODir->DirSector ); WBEDWORD( &pDirsector[  6 ], pISODir->DirSector );
	WLEDWORD( &pDirsector[ 10 ], pISODir->DirLength ); WBEDWORD( &pDirsector[ 14 ], pISODir->DirLength );
	pISODir->ConvertUnixTime( &pDirsector[ 18 ], time( NULL ) );
	pDirsector[ 25 ] = 0x2; // Directory
	pDirsector[ 26 ] = 0;
	pDirsector[ 27 ] = 0;
	WBEDWORD( &pDirsector[ 28 ], 0x01000001 );
	pDirsector[ 32 ] = 1;
	pDirsector[ 33 ] = 1;
	pDirsector[ 34 ] = 0;

	if ( FSID == FSID_ISOHS ) { pDirsector[ 24 ] = pDirsector[ 25 ]; pDirsector[ 25 ] = 0; }

	SetISOHints( pSource, true, true );

	(void) pSource->WriteSectorLBA( PriVolDesc.VolSize, (BYTE *) NewDirSector, PriVolDesc.SectorSize );

	// Now make some changes to the descriptor, and read it back in.
	(void) ISOExtendDataArea( pSource, PriVolDesc.SectorSize, 1U, FSID );

	(void) ISOSetPathTableSize( pSource, PriVolDesc.SectorSize, NewTableSize, FSID );

	FreeSectors--;

	ReadVolumeDescriptors();

	if ( CreateFlags & CDF_ENTER_AFTER )
	{
		if ( !IsRoot() )
		{
			CDPath += "/";
		}

		pISODir->Push( StartSect, PriVolDesc.SectorSize );
			
		CDPath += std::string( (char *) (BYTE *) pDir->Filename );
	}

	return pDirectory->ReadDirectory();
}

int ISO9660FileSystem::RenameIncomingDirectory( NativeFile *pDir )
{
	/* The idea here is to try and alter the name of the directory to something unique.

	   We're going to do this by copying the filename, then overwriting the trail with
	   a number starting at 1, and incrementing on each pass. The number is prefixed by _

	   When we find one, we'll return it. If not, we go around again. If we somehow fill
	   up all 10 characters, we'll error here.
	*/

	bool Done = false;

	DWORD MaxClip = min( pDir->Filename.length(), 100 );

	BYTEString CurrentFilename( pDir->Filename, MaxClip + 11 );
	BYTEString ProposedFilename( MaxClip + 11 );

	DWORD ProposedIndex = 1;

	BYTE ProposedSuffix[ 11 ];

	while ( !Done )
	{
		rstrncpy( ProposedFilename, CurrentFilename, MaxClip );

		rsprintf( ProposedSuffix, "_%d", ProposedIndex );

		WORD SuffixLen = rstrnlen( ProposedSuffix, 9 );
		
		rstrncat( (BYTE *) ProposedFilename, ProposedSuffix, MaxClip );

		/* Now see if that's unique */
		NativeFileIterator iFile;

		NativeFile temp;

		temp.Flags = 0;
		temp.Filename = ProposedFilename;

		bool Unique = true;

		BYTE ISOFileName[ 256 ];
		BYTE ProposedISO[ 256 ];

		MakeISOFilename( &temp, ProposedISO );

		for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
		{
			if ( rstricmp( iFile->Filename, ProposedFilename ) )
			{
				Unique = false;

				break;
			}

			MakeISOFilename( &*iFile, ISOFileName );

			if ( rstricmp( ISOFileName, ProposedISO ) )
			{
				Unique = false;

				break;
			}
		}

		Done = Unique;

		ProposedIndex++;

		if ( ProposedIndex == 0 )
		{
			/* EEP - Ran out */
			return NUTSError( 208, L"Unable to find unique name for incoming directory" );
		}
	}

	pDir->Filename = ProposedFilename;

	return 0;
}

void ISO9660FileSystem::RemapStack(  ISOSectorList &Sectors, BYTE IsoOp )
{
	for (int i=0; i<pISODir->PathStackExtent.size(); i++ )
	{
		pISODir->PathStackExtent[ i ] = RemapSector( pISODir->PathStackExtent[ i ], Sectors, IsoOp );
	}
}

int ISO9660FileSystem::DeleteFile( DWORD FileID )
{
	WRITABLE_TEST();

	// This is (compared with WriteFile/CreateDirectory) remarkably simple.
	// We just need to delete the sectors that the object occupies, and then remove it
	// from the directory and write.

	// There are extra steps:
	// 1) If the directory shrinks as a result, we must delete the unneeded sectors.
	// 2) If the object is a directory, then it must be removed from the path table
	// 3) If the path table(s) shrink as a result, we must delete THOSE unneeded sectors.
	// 4) In the interests of efficiency, don't do the sector delete until CommitOps is
	//    called, otherwise we'll be here all night trying to reorganise the ISO. Note
	//    that this only applies if the Op is a DELETE. DeleteFile can be called prior
	//    to WriteFile or CreateDirectory to resolve a name conflict, in which case
	//    that reorganisation should happen immediately.

	NativeFile *pObj = &pDirectory->Files[ FileID ];

	ISOSector sector;

	// Step 1) The sectors occupied by the object.
	DWORD OccupiedSectors = ( pObj->Length + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;

	for ( DWORD i=0; i<OccupiedSectors; i++ )
	{
		sector.ID = pObj->Attributes[ ISOATTR_START_EXTENT ] + i;

		DeleteSectors.push_back( sector );
	}

	if ( pObj->ExtraForks > 0 )
	{
		OccupiedSectors = ( pObj->Attributes[ ISOATTR_FORK_LENGTH ] + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;

		for ( DWORD i=0; i<OccupiedSectors; i++ )
		{
			sector.ID = pObj->Attributes[ ISOATTR_FORK_EXTENT ] + i;

			DeleteSectors.push_back( sector );
		}
	}

	// Step 2) Path Tables
	if ( pObj->Flags & FF_Directory )
	{
		DWORD OldTableSects = ( pPathTable->GetProjectedSize() + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;
		
		pPathTable->RemoveDirectory( pObj->Attributes[ ISOATTR_START_EXTENT ] );

		DWORD NewTableSects = ( pPathTable->GetProjectedSize() + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;

		if ( OldTableSects < NewTableSects )
		{
			// Only one sector will go, so we can simply this.
			if ( PriVolDesc.LTableLoc    != 0 ) { sector.ID = PriVolDesc.LTableLoc    + OldTableSects; DeleteSectors.push_back( sector ); pPathTable->WritePathTable( PriVolDesc.LTableLoc,    false, FSID ); }
			if ( PriVolDesc.MTableLoc    != 0 ) { sector.ID = PriVolDesc.MTableLoc    + OldTableSects; DeleteSectors.push_back( sector ); pPathTable->WritePathTable( PriVolDesc.LTableLocOpt, false, FSID );  }
			if ( PriVolDesc.LTableLocOpt != 0 ) { sector.ID = PriVolDesc.LTableLocOpt + OldTableSects; DeleteSectors.push_back( sector ); pPathTable->WritePathTable( PriVolDesc.MTableLoc,    true,  FSID );  }
			if ( PriVolDesc.MTableLocOpt != 0 ) { sector.ID = PriVolDesc.MTableLocOpt + OldTableSects; DeleteSectors.push_back( sector ); pPathTable->WritePathTable( PriVolDesc.MTableLocOpt, true,  FSID );  }

			if ( FSID == FSID_ISOHS )
			{
				if ( PriVolDesc.LTableLocOptA != 0 ) { sector.ID = PriVolDesc.LTableLocOptA + OldTableSects; DeleteSectors.push_back( sector ); pPathTable->WritePathTable( PriVolDesc.LTableLocOptA, false, FSID );  }
				if ( PriVolDesc.MTableLocOptA != 0 ) { sector.ID = PriVolDesc.MTableLocOptA + OldTableSects; DeleteSectors.push_back( sector ); pPathTable->WritePathTable( PriVolDesc.MTableLocOptA, true,  FSID );  }
				if ( PriVolDesc.LTableLocOptB != 0 ) { sector.ID = PriVolDesc.LTableLocOptB + OldTableSects; DeleteSectors.push_back( sector ); pPathTable->WritePathTable( PriVolDesc.LTableLocOptB, false, FSID );  }
				if ( PriVolDesc.MTableLocOptB != 0 ) { sector.ID = PriVolDesc.MTableLocOptB + OldTableSects; DeleteSectors.push_back( sector ); pPathTable->WritePathTable( PriVolDesc.MTableLocOptB, true,  FSID );  }
			}
		}
	}

	// Post-script: If this is the root directory, and the filename was referenced by the descriptor, we must update the descriptor.
	if ( IsRoot() )
	{
		BYTE ISOFilename[ 256 ];

		bool WriteBack = false;

		MakeISOFilename( &pDirectory->Files[ FileID ], ISOFilename, false );

		if ( rstrncmp( PriVolDesc.CopyrightFilename, ISOFilename, 17 ) )
		{
			rstrncpy( PriVolDesc.CopyrightFilename, (BYTE *) "                 ", 17 );

			WriteBack = true;
		}

		if ( rstrncmp( PriVolDesc.AbstractFilename, ISOFilename, 17 ) )
		{
			rstrncpy( PriVolDesc.AbstractFilename, (BYTE *) "                 ", 17 );

			WriteBack = true;
		}

		if ( rstrncmp( PriVolDesc.BibliographicFilename, ISOFilename, 17 ) )
		{
			rstrncpy( PriVolDesc.BibliographicFilename, (BYTE *) "                 ", 17 );

			WriteBack = true;
		}

		if ( WriteBack )
		{
			WriteVolumeDescriptor( PriVolDesc, PriVolDesc.SourceSector, FSID, false );
		}
	}

	// Step 3) Directory shrinkage
	(void) pDirectory->Files.erase( pDirectory->Files.begin() + FileID );

	DirectoryShrink();

	if ( pDirectory->WriteDirectory() != NUTS_SUCCESS )
	{
		DeleteSectors.clear();

		return -1;
	}

	if ( FileOpsAction != AA_DELETE )
	{
		if ( RemoveSectors() != NUTS_SUCCESS )
		{
			return -1;
		}

		DeleteSectors.clear();
	}

	return pDirectory->ReadDirectory();
}

int ISO9660FileSystem::DeleteFile( NativeFile *pFile )
{
	if ( FilenameConflictCheck( FILEOP_EXISTS ) != NUTS_SUCCESS )
	{
		return DeleteFile( ConflictedID );
	}

	return 0;
}

void ISO9660FileSystem::CommitOps( void )
{
	if ( FileOpsAction == AA_DELETE )
	{
		if ( DeleteSectors.size() > 0 )
		{
			PrepareISOSectors( DeleteSectors );

			ISOJob job;

			job.FSID       = FSID;
			job.IsoOp      = ISO_RESTRUCTURE_REM_SECTORS;
			job.pSource    = pSource;
			job.pVolDesc   = &PriVolDesc;
			job.Sectors    = DeleteSectors;
			job.SectorSize = PriVolDesc.SectorSize;

			(void) PerformISOJob( &job );

			// Need to remap a few things here.
			pISODir->ParentSector = RemapSector( pISODir->ParentSector, DeleteSectors, ISO_RESTRUCTURE_REM_SECTORS );
			pISODir->DirSector    = RemapSector( pISODir->DirSector,    DeleteSectors, ISO_RESTRUCTURE_REM_SECTORS );

			RemapStack( DeleteSectors, ISO_RESTRUCTURE_REM_SECTORS );

			FreeSectors += DeleteSectors.size();
		}
	}

	DeleteSectors.clear();

	FileOpsAction = AA_NONE;
}


int ISO9660FileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	WRITABLE_TEST();

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	for ( BYTE i=16; i<32; i++ )
	{
		pFile->Attributes[ i ] = Changes->Attributes[ i ];
	}
	
	FOPData fop;

	// First do an AttrChanges pass. This gives plugins the chance to make any further changes
	// to the attributes in response. The plugin will set the attributes accordingly.

	fop.DataType  = FOP_DATATYPE_CDISO;
	fop.Direction = FOP_AttrChanges;
	fop.pFile     = (void *) pFile;
	fop.pFS       = (void *) this;
	fop.pXAttr    = (BYTE *) Changes;

	(void) ProcessFOP( &fop );

	BYTE SUSPData[ 256 ];

	fop.DataType  = FOP_DATATYPE_CDISO;
	fop.Direction = FOP_WriteEntry;
	fop.lXAttr    = 256;
	fop.pXAttr    = SUSPData;
	fop.pFile     = (void *) pFile;
	fop.pFS       = (void *) this;
	
	// This is slightly different to WriteFile/CreateDirectory as the aux data is ours, so we must
	// free it first.
	if ( pFile->pAuxData != nullptr )
	{
		delete pFile->pAuxData;

		pFile->pAuxData = nullptr;
		pFile->lAuxData = 0;
	}

	if ( !ProcessFOP( &fop ) )
	{
		if ( FSID != FSID_ISOHS )
		{
			// Generate Rock Ridge SUSP
			MakeRockRidge( pFile );
		}
		else
		{
			pFile->pAuxData = nullptr;
			pFile->lAuxData = 0;
		}
	}
	else
	{
		if ( fop.lXAttr != 0 )
		{
			pFile->pAuxData = new BYTE[ fop.lXAttr ];
			pFile->lAuxData = fop.lXAttr;

			memcpy( pFile->pAuxData, fop.pXAttr, fop.lXAttr );
		}
	}

	DirectoryShrink();

	if ( DirectoryResize() != NUTS_SUCCESS )
	{
		// Read the Directory back to resync it
		(void) pDirectory->ReadDirectory();

		return -1;
	}

	int r = pDirectory->WriteDirectory();

	if ( RemoveSectors() != NUTS_SUCCESS )
	{
		// Read the Directory back to resync it
		(void) pDirectory->ReadDirectory();

		return -1;
	}

	if ( r == NUTS_SUCCESS )
	{
		r = pDirectory->ReadDirectory();
	}

	return r;
}

void ISO9660FileSystem::DirectoryShrink( )
{
	DWORD DirectoryAlloc = ( pISODir->DirLength + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;
	DWORD DirectorySize  = pISODir->ProjectedDirectorySize();

	ISOSector sector;

	if ( DirectorySize < DirectoryAlloc )
	{
		sector.ID = pISODir->DirSector + DirectorySize;

		DeleteSectors.push_back( sector );
	}
}

int ISO9660FileSystem::RemoveSectors()
{
	if ( DeleteSectors.size() == 0U )
	{
		return 0;
	}

	PrepareISOSectors( DeleteSectors );

	ISOJob job;

	job.FSID       = FSID;
	job.IsoOp      = ISO_RESTRUCTURE_REM_SECTORS;
	job.pSource    = pSource;
	job.pVolDesc   = &PriVolDesc;
	job.Sectors    = DeleteSectors;
	job.SectorSize = PriVolDesc.SectorSize;

	if ( PerformISOJob( &job ) != NUTS_SUCCESS )
	{
		return -1;
	}

	// Need to remap a few things here.
	pISODir->ParentSector = RemapSector( pISODir->ParentSector, DeleteSectors, ISO_RESTRUCTURE_REM_SECTORS );
	pISODir->DirSector    = RemapSector( pISODir->DirSector,    DeleteSectors, ISO_RESTRUCTURE_REM_SECTORS );

	RemapStack( DeleteSectors, ISO_RESTRUCTURE_REM_SECTORS );

	FreeSectors += DeleteSectors.size();

	return 0;
}

int	ISO9660FileSystem::ReplaceFile(NativeFile *pFile, CTempFile &store)
{
	WRITABLE_TEST();

	// This is not that hard in fact. We already have the structures in place, so
	// we just need to expand or contract the data space as required, then update
	// the directory with the new length.

	DWORD AllocSectors = ( pFile->Length + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;
	DWORD NeedSectors = ( store.Ext() + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;

	if ( AllocSectors != NeedSectors )
	{
		// Oh dear. Size change.
		ISOSectorList sectors;
		ISOSector sector;
		BYTE IsoOp;

		if ( NeedSectors < AllocSectors )
		{
			// Shrunk!
			for ( DWORD i=NeedSectors; i<AllocSectors; i++ )
			{
				sector.ID = pFile->Attributes[ ISOATTR_START_EXTENT ] + i;

				sectors.push_back( sector );
			}

			IsoOp = ISO_RESTRUCTURE_REM_SECTORS;
		}
		else
		{
			// Grew!
			for ( DWORD i=AllocSectors; i<NeedSectors; i++ )
			{
				sector.ID = pFile->Attributes[ ISOATTR_START_EXTENT ] + i;

				sectors.push_back( sector );
			}

			IsoOp = ISO_RESTRUCTURE_ADD_SECTORS;
		}

		PrepareISOSectors( sectors );

		ISOJob job;

		job.FSID       = FSID;
		job.IsoOp      = IsoOp;
		job.pSource    = pSource;
		job.pVolDesc   = &PriVolDesc;
		job.Sectors    = sectors;
		job.SectorSize = PriVolDesc.SectorSize;

		if ( PerformISOJob( &job ) != NUTS_SUCCESS )
		{
			return -1;
		}

		// Update directory structures, and write back
		for ( NativeFileIterator iRefile = pDirectory->Files.begin(); iRefile != pDirectory->Files.end(); iRefile++ )
		{
			iRefile->Attributes[ ISOATTR_START_EXTENT ] = RemapSector( iRefile->Attributes[ ISOATTR_START_EXTENT ], sectors, IsoOp );

			if ( iRefile->ExtraForks > 0 )
			{
				iRefile->Attributes[ ISOATTR_FORK_EXTENT ] = RemapSector( iRefile->Attributes[ ISOATTR_FORK_EXTENT ], sectors, IsoOp );
			}
		}

		pISODir->DirSector    = RemapSector( pISODir->DirSector,    sectors, IsoOp );
		pISODir->ParentSector = RemapSector( pISODir->ParentSector, sectors, IsoOp );

		RemapStack( sectors, IsoOp );

		FreeSectors += AllocSectors;
		FreeSectors -= NeedSectors;
	}

	// Now that's done, write the data out
	DWORD BytesToGo = store.Ext();
	DWORD SectorNum = pFile->Attributes[ ISOATTR_START_EXTENT ];

	store.Seek( 0 );

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( PriVolDesc.SectorSize, BytesToGo );

		AutoBuffer Sector( PriVolDesc.SectorSize );

		ZeroMemory( (BYTE *) Sector, PriVolDesc.SectorSize );

		store.Read( (BYTE *) Sector, BytesRead );

		SetISOHints( pSource, ( BytesToGo <= PriVolDesc.SectorSize ), ( BytesToGo <= PriVolDesc.SectorSize ) );

		if ( pSource->WriteSectorLBA( SectorNum, (BYTE *) Sector, PriVolDesc.SectorSize ) != DS_SUCCESS )
		{
			return -1;
		}

		BytesToGo -= BytesRead;

		SectorNum++;
	}

	// Update the directory.
	pDirectory->Files[ pFile->fileID ].Length = store.Ext();

	return 0;
}

static ISOSectorType FormatSectorType;
static DWORD FormatSectorSize;
static BYTE  FormatNumLTables;
static BYTE  FormatNumMTables;

INT_PTR CALLBACK FormatDialog(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if ( uMsg == WM_INITDIALOG )
	{
		CheckRadioButton( hwndDlg, IDC_MODE1PURE, IDC_XAM2F2, IDC_MODE1PURE );
		CheckRadioButton( hwndDlg, IDC_1LTABLE, IDC_4LTABLE, IDC_1LTABLE );
		CheckRadioButton( hwndDlg, IDC_1MTABLE, IDC_4MTABLE, IDC_1MTABLE );
	}

	if ( uMsg == WM_COMMAND )
	{
		if ( ( wParam == IDOK ) || ( wParam == IDCANCEL ) )
		{
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_MODE1PURE ) ) ) { FormatSectorType = ISOSECTOR_PURE1;   FormatSectorSize = 0x800; }
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_MODE1RAW ) ) )  { FormatSectorType = ISOSECTOR_MODE1;   FormatSectorSize = 0x800; }
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_MODE2RAW ) ) )  { FormatSectorType = ISOSECTOR_MODE2;   FormatSectorSize = 0x920; }
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_XAM2F1 ) ) )    { FormatSectorType = ISOSECTOR_XA_M2F1; FormatSectorSize = 0x800; }
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_XAM2F2 ) ) )    { FormatSectorType = ISOSECTOR_XA_M2F1; FormatSectorSize = 0x914; }

			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_1LTABLE ) ) )   { FormatNumLTables = 1; }
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_2LTABLE ) ) )   { FormatNumLTables = 2; }
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_3LTABLE ) ) )   { FormatNumLTables = 3; }
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_4LTABLE ) ) )   { FormatNumLTables = 4; }

			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_1MTABLE ) ) )   { FormatNumMTables = 1; }
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_2MTABLE ) ) )   { FormatNumMTables = 2; }
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_3MTABLE ) ) )   { FormatNumMTables = 3; }
			if ( Button_GetCheck( GetDlgItem( hwndDlg, IDC_4MTABLE ) ) )   { FormatNumMTables = 4; }

			EndDialog( hwndDlg, wParam );

			return TRUE;
		}
	}

	return FALSE;
}

int ISO9660FileSystem::Format_PreCheck( int FormatType, HWND hWnd )
{
	FormatSectorSize = 0x800;
	FormatNumLTables = 1;
	FormatNumMTables = 1;
	FormatSectorType = ISOSECTOR_PURE1;

	if ( DialogBoxParam( hInst, MAKEINTRESOURCE( IDD_ISO_FORMAT ), hWnd, FormatDialog, NULL ) == IDCANCEL )
	{
		return -1;
	}

	return 0;
}

int ISO9660FileSystem::Format_Process( DWORD FT, HWND hWnd )
{
	// if this is a raw sector type, we need to wrap the DataSource;
	if ( FormatSectorType != ISOSECTOR_PURE1 )
	{
		ISORawSectorSource *pLowerSource = new ISORawSectorSource( pSource );

		pLowerSource->SetSectorType( FormatSectorType );
		
		// Now that ISO Raw Sector source has DS_RETAIN'd the source, we need to release it,
		// as we now only contact the source through pLowerSource

		DS_RELEASE( pSource );

		pSource = pLowerSource;
	}

	// "format" the first 16 sectors. In a pure mode this just writes loads of zeros.
	// But for the raw modes, this will fill in sector headers, etc.
	AutoBuffer BlankSect( FormatSectorSize );
	ZeroMemory( (BYTE *) BlankSect, FormatSectorSize );

	for ( DWORD s=0; s<15; s++ )
	{
		SetISOHints( pSource, false, false );

		pSource->WriteSectorLBA( s, (BYTE *)  BlankSect, FormatSectorSize );
	}

	ISOVolDesc FormatDesc;

	ZeroMemory( &FormatDesc, sizeof( ISOVolDesc ) );

	FormatDesc.SectorSize    = FormatSectorSize;
	FormatDesc.SeqNum        = 1;
	FormatDesc.VolSetSize    = 1;

	DWORD RootExtent = 18 + FormatNumLTables + FormatNumMTables;

	FormatDesc.VolSize       = RootExtent;

	ISOPathTable FormatTable( pSource, FormatSectorSize );

	FormatTable.BlankTable( RootExtent );

	FormatDesc.PathTableSize = FormatTable.GetProjectedSize();

	DWORD PathTableBase = 18;

	// Yes these two switches are meant to be missing breaks.
	switch ( FormatNumLTables )
	{
	case 4:
		FormatTable.WritePathTable( PathTableBase + 3, false, FSID );
		FormatDesc.LTableLocOptB = PathTableBase + 3;
	case 3:
		FormatTable.WritePathTable( PathTableBase + 2, false, FSID );
		FormatDesc.LTableLocOptA = PathTableBase + 2;
	case 2:
		FormatTable.WritePathTable( PathTableBase + 1, false, FSID );
		FormatDesc.LTableLocOpt = PathTableBase + 1;
	case 1:
		FormatTable.WritePathTable( PathTableBase + 0, false, FSID );
		FormatDesc.LTableLoc = PathTableBase + 0;
	}

	PathTableBase += FormatNumLTables;

	switch ( FormatNumMTables )
	{
	case 4:
		FormatTable.WritePathTable( PathTableBase + 3, true, FSID );
		FormatDesc.MTableLocOptB = PathTableBase + 3;
	case 3:
		FormatTable.WritePathTable( PathTableBase + 2, true, FSID );
		FormatDesc.MTableLocOptA = PathTableBase + 2;
	case 2:
		FormatTable.WritePathTable( PathTableBase + 1, true, FSID );
		FormatDesc.MTableLocOpt = PathTableBase + 1;
	case 1:
		FormatTable.WritePathTable( PathTableBase + 0, true, FSID );
		FormatDesc.MTableLoc = PathTableBase + 0;
	}

	// Fill in other blocks
	FormatDesc.VolumeLBN = 1;
	
	rstrncpy( FormatDesc.VolID, (BYTE *) "CDROM VOLUME", 32 );

	{
		time_t t = time(NULL);
		struct tm *pT = localtime( &t );

		static const char * const days[8] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };

		BYTE Title[ 20 ];

		rsprintf( FormatDesc.VolSetID, "VOLUME AT %s %02d %02d %04d %02d:%02d",
			days[ pT->tm_wday ],
			pT->tm_mday, pT->tm_mon + 1, pT->tm_year + 1900,
			pT->tm_hour, pT->tm_min
		);

		// File in dates here
		rsprintf( FormatDesc.CreationTime, "%04d%02d%02d%02d%02d%02d00", // Note terminating zero becomes "minutes from GMT".
			pT->tm_year + 1900, pT->tm_mon+ 1, pT->tm_mday,
			pT->tm_hour, pT->tm_min, pT->tm_sec
			);

		rsprintf( FormatDesc.ModTime, "%04d%02d%02d%02d%02d%02d00", // Note terminating zero becomes "minutes from GMT".
			pT->tm_year + 1900, pT->tm_mon+ 1, pT->tm_mday,
			pT->tm_hour, pT->tm_min, pT->tm_sec
			);

		rstrncpy( FormatDesc.EffectiveTime, (BYTE *) "0000000000000000", 16 ); // Terminating zero becomes "minutes from GMT".
		rstrncpy( FormatDesc.ExpireTime, (BYTE *) "0000000000000000", 16 ); // Terminating zero becomes "minutes from GMT".
	}

	rstrncpy( FormatDesc.Publisher, (BYTE *) "NUTS USER", 128 );
	rstrncpy( FormatDesc.Preparer, (BYTE *) "NUTS", 128 );
	rstrncpy( FormatDesc.AppID, (BYTE *) "GENERAL USE", 128 );
	
	// We won't write the following fields here, as they have no purpose on a blank image:
	// copyright_file_id, abstract_file_id, application_data
	// file_strcuture_version is always 1, and will be written by WriteVolumeDescriptor();

	// Make a directory record
	
	// We must fill in . and .., because reasons.
	AutoBuffer DirRec( 0x44 );

	BYTE *pEnt = (BYTE *) DirRec;

	// . = this
	pEnt[ 0 ] = 0x22; // fixed size
	pEnt[ 1 ] = 0;
	WLEDWORD( &pEnt[  2 ], RootExtent ); WBEDWORD( &pEnt[  6 ], RootExtent );
	WLEDWORD( &pEnt[ 10 ], FormatSectorSize ); WBEDWORD( &pEnt[ 14 ], FormatSectorSize );
	pISODir->ConvertUnixTime( &pEnt[ 18 ], time( NULL ) );
	pEnt[ 25 ] = 0x2; // Directory
	pEnt[ 26 ] = 0;
	pEnt[ 27 ] = 0;
	WBEDWORD( &pEnt[ 28 ], 0x01000001 );
	pEnt[ 32 ] = 1;
	pEnt[ 33 ] = 0;

	if ( FSID == FSID_ISOHS ) { pEnt[ 24 ] = pEnt[ 25 ]; pEnt[ 25 ] = 0; }

	pEnt += 0x22;

	// .. = parent
	pEnt[ 0 ] = 0x22; // fixed size
	pEnt[ 1 ] = 0;
	WLEDWORD( &pEnt[  2 ], RootExtent ); WBEDWORD( &pEnt[  6 ], RootExtent );
	WLEDWORD( &pEnt[ 10 ], FormatSectorSize ); WBEDWORD( &pEnt[ 14 ], FormatSectorSize );
	pISODir->ConvertUnixTime( &pEnt[ 18 ], time( NULL ) );
	pEnt[ 25 ] = 0x2; // Directory
	pEnt[ 26 ] = 0;
	pEnt[ 27 ] = 0;
	WBEDWORD( &pEnt[ 28 ], 0x01000001 );
	pEnt[ 32 ] = 1;
	pEnt[ 33 ] = 1;

	if ( FSID == FSID_ISOHS ) { pEnt[ 24 ] = pEnt[ 25 ]; pEnt[ 25 ] = 0; }

	// take a copy in the PVD - . only!
	memcpy( &FormatDesc.DirectoryRecord, (BYTE *) DirRec, 34 );

	WriteVolumeDescriptor( FormatDesc, 16, FSID, false );

	AutoBuffer SetTerm( FormatDesc.SectorSize );

	BYTE *SetTerminator = (BYTE *) SetTerm;
	BYTE TerminatorRecord[ 16 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 'C', 'D', 'R', 'O', 'M', 1 };

	ZeroMemory( SetTerminator, 2048 );

	if ( FSID == FSID_ISO9660 )
	{
		memcpy( TerminatorRecord, &TerminatorRecord[ 8 ], 8 );
		memset( &TerminatorRecord[ 8 ], 0, 8 );

		TerminatorRecord[ 3 ] = '0';
		TerminatorRecord[ 4 ] = '0';
		TerminatorRecord[ 5 ] = '1';
	}

	memcpy( SetTerminator, TerminatorRecord, 16 );

	SetISOHints( pSource, true, true );

	if ( pSource->WriteSectorLBA( 17, SetTerminator, FormatDesc.SectorSize ) != DS_SUCCESS )
	{
		return -1;
	}

	AutoBuffer DirSector( FormatSectorSize );

	ZeroMemory( (BYTE *) DirSector, 2048 );

	memcpy( (BYTE *) DirSector, (BYTE *) DirRec, 0x44 );

	// Write our root directory out.
	SetISOHints( pSource, true, true );
	
	if ( pSource->WriteSectorLBA( RootExtent, (BYTE *) DirSector, FormatSectorSize )  != DS_SUCCESS )
	{
		return -1;
	}

	return 0;
}

void ISO9660FileSystem::WriteVolumeDescriptor( ISOVolDesc &VolDesc, DWORD Sector, DWORD FSID, bool Joliet )
{
	AutoBuffer SectorBuf( VolDesc.SectorSize );

	BYTE *Buffer = (BYTE *) SectorBuf;

	ZeroMemory( Buffer, VolDesc.SectorSize );

	const BYTE CDID1[] = { 1, 'C', 'D', 'R', 'O', 'M', 1 };
	const BYTE CDID2[] = { 1, 'C', 'D', '0', '0', '1', 1 };

	if ( FSID == FSID_ISOHS )
	{
		memcpy( &Buffer[ 8 ], CDID1, 7 );

		WLEDWORD( &Buffer[ 0 ], VolDesc.VolumeLBN );
		WBEDWORD( &Buffer[ 4 ], VolDesc.VolumeLBN );
	}
	else
	{
		memcpy( &Buffer[ 0 ], CDID2, 7 );
	}

	if ( Joliet )
	{
		Buffer[ 0x58 ] = 0x25;
		Buffer[ 0x59 ] = 0x2f;
		Buffer[ 0x5a ] = 0x45;
		Buffer[ 0x00 ] = 0x02;
	}

	if ( FSID == FSID_ISO9660 )
	{
		Buffer[ 7 ] = 1; // structure version

		if ( Joliet )
		{
			ISOJolietStore( &Buffer[  8 ], VolDesc.SysID, 32 );
			ISOJolietStore( &Buffer[ 40 ], VolDesc.VolID, 32 );
		}
		else
		{
			ISOStrStore( &Buffer[  8 ], VolDesc.SysID, 32 );
			ISOStrStore( &Buffer[ 40 ], VolDesc.VolID, 32 );
		}

		WLEDWORD( &Buffer[  80 ], VolDesc.VolSize );
		WBEDWORD( &Buffer[  84 ], VolDesc.VolSize );
		WLEWORD(  &Buffer[ 120 ], VolDesc.VolSetSize );
		WBEWORD(  &Buffer[ 122 ], VolDesc.VolSetSize );
		WLEWORD(  &Buffer[ 124 ], VolDesc.SeqNum );
		WBEWORD(  &Buffer[ 126 ], VolDesc.SeqNum );
		WLEWORD(  &Buffer[ 128 ], VolDesc.SectorSize );
		WBEWORD(  &Buffer[ 130 ], VolDesc.SectorSize );

		WLEDWORD( &Buffer[ 132 ], VolDesc.PathTableSize );
		WBEDWORD( &Buffer[ 136 ], VolDesc.PathTableSize );
		WLEDWORD( &Buffer[ 140 ], VolDesc.LTableLoc );
		WLEDWORD( &Buffer[ 144 ], VolDesc.LTableLocOpt );
		WBEDWORD( &Buffer[ 148 ], VolDesc.MTableLoc );
		WBEDWORD( &Buffer[ 152 ], VolDesc.MTableLocOpt );

		memcpy( &Buffer[ 156 ], &VolDesc.DirectoryRecord, 34 );

		if ( Joliet )
		{
			ISOJolietStore( &Buffer[ 190 ], VolDesc.VolSetID, 128 );
			ISOJolietStore( &Buffer[ 318 ], VolDesc.Publisher, 128 );
			ISOJolietStore( &Buffer[ 446 ], VolDesc.Preparer, 128 );
			ISOJolietStore( &Buffer[ 574 ], VolDesc.AppID, 128 );

			ISOJolietStore( &Buffer[ 702 ], VolDesc.CopyrightFilename, 37 );
			ISOJolietStore( &Buffer[ 739 ], VolDesc.AbstractFilename, 37 );
			ISOJolietStore( &Buffer[ 776 ], VolDesc.BibliographicFilename, 37 );
		}
		else
		{
			ISOStrStore( &Buffer[ 190 ], VolDesc.VolSetID, 128 );
			ISOStrStore( &Buffer[ 318 ], VolDesc.Publisher, 128 );
			ISOStrStore( &Buffer[ 446 ], VolDesc.Preparer, 128 );
			ISOStrStore( &Buffer[ 574 ], VolDesc.AppID, 128 );

			ISOStrStore( &Buffer[ 702 ], VolDesc.CopyrightFilename, 37 );
			ISOStrStore( &Buffer[ 739 ], VolDesc.AbstractFilename, 37 );
			ISOStrStore( &Buffer[ 776 ], VolDesc.BibliographicFilename, 37 );
		}

		memcpy( &Buffer[ 813 ], &VolDesc.CreationTime, 17 );
		memcpy( &Buffer[ 830 ], &VolDesc.ModTime, 17 );
		memcpy( &Buffer[ 847 ], &VolDesc.ExpireTime, 17 );
		memcpy( &Buffer[ 864 ], &VolDesc.EffectiveTime, 17 );

		memcpy( &Buffer[ 883 ],  &VolDesc.ApplicationBytes, 512 );

		Buffer[ 881 ] = 1; // File structure version
	}

	if ( FSID == FSID_ISOHS )
	{
		Buffer[ 14 ] = 1; // structure version

		ISOStrStore( &Buffer[ 16 ], VolDesc.SysID, 32 );
		ISOStrStore( &Buffer[ 48 ], VolDesc.VolID, 32 );

		WLEDWORD( &Buffer[  88 ], VolDesc.VolSize );
		WBEDWORD( &Buffer[  92 ], VolDesc.VolSize );
		WLEWORD(  &Buffer[ 128 ], VolDesc.VolSetSize );
		WBEWORD(  &Buffer[ 130 ], VolDesc.VolSetSize );
		WLEWORD(  &Buffer[ 132 ], VolDesc.SeqNum );
		WBEWORD(  &Buffer[ 134 ], VolDesc.SeqNum );
		WLEWORD(  &Buffer[ 136 ], VolDesc.SectorSize );
		WBEWORD(  &Buffer[ 138 ], VolDesc.SectorSize );

		WLEDWORD( &Buffer[ 140 ], VolDesc.PathTableSize );
		WBEDWORD( &Buffer[ 144 ], VolDesc.PathTableSize );
		WLEDWORD( &Buffer[ 148 ], VolDesc.LTableLoc );
		WLEDWORD( &Buffer[ 152 ], VolDesc.LTableLocOpt );
		WLEDWORD( &Buffer[ 156 ], VolDesc.LTableLocOptA );
		WLEDWORD( &Buffer[ 160 ], VolDesc.LTableLocOptB );
		WBEDWORD( &Buffer[ 164 ], VolDesc.MTableLoc );
		WBEDWORD( &Buffer[ 168 ], VolDesc.MTableLocOpt );
		WBEDWORD( &Buffer[ 172 ], VolDesc.MTableLocOptA );
		WBEDWORD( &Buffer[ 176 ], VolDesc.MTableLocOptB );

		memcpy( &Buffer[ 180 ], &VolDesc.DirectoryRecord, 34 );

		ISOStrStore( &Buffer[ 214 ], VolDesc.VolSetID, 128 );
		ISOStrStore( &Buffer[ 342 ], VolDesc.Publisher, 128 );
		ISOStrStore( &Buffer[ 470 ], VolDesc.Preparer, 128 );
		ISOStrStore( &Buffer[ 598 ], VolDesc.AppID, 128 );

		ISOStrStore( &Buffer[ 726 ], VolDesc.CopyrightFilename, 37 );
		ISOStrStore( &Buffer[ 758 ], VolDesc.AbstractFilename, 37 );

		memcpy( &Buffer[ 790 ], &VolDesc.CreationTime, 17 );
		memcpy( &Buffer[ 806 ], &VolDesc.ModTime, 17 );
		memcpy( &Buffer[ 822 ], &VolDesc.ExpireTime, 17 );
		memcpy( &Buffer[ 838 ], &VolDesc.EffectiveTime, 17 );

		memcpy( &Buffer[ 856 ],  &VolDesc.ApplicationBytes, 512 );

		Buffer[ 854 ] = 1; // File structure version
	}

	SetISOHints( pSource, true, false );

	if ( pSource->WriteSectorLBA( Sector, Buffer, PriVolDesc.SectorSize ) != DS_SUCCESS )
	{
		return;
	}
}

FSToolList ISO9660FileSystem::GetToolsList( void )
{
	FSToolList list;

	if ( pSource->Flags & DS_ReadOnly )
	{
		return list;
	}

	/* Repair Icon, Large Android Icons, Aha-Soft, CC Attribution 3.0 US */
	HBITMAP hRepair = LoadBitmap( hInst, MAKEINTRESOURCE( IDB_REPAIR ) );

	FSTool PathTool = { hRepair, L"Rebuild Path Tables", L"Rebuild the path tables from the directory structure." };
	FSTool DirTool  = { hRepair, L"Rebuild Directory Tree", L"Rebuild the directory structure from the path table. This tool may result in an inconsistent usage of disk sectors." };
	FSTool UseTool  = { hRepair, L"Remove Unused Sectors", L"Remove sectors not used by any file system object." };

	list.push_back( PathTool );
	list.push_back( DirTool );
	list.push_back( UseTool );

	return list;
}

int ISO9660FileSystem::GatherDirs( FileSystem *pCloneFS, ISOPathTable *pScanTable, DWORD ThisExtent )
{
	// We must use an index rather than an interator, as calls to ChangeDirectory() and Parent()
	// will destroy and recreate the structure.

	DWORD FileID;

	BYTE ISOFilename[ 256 ];

	for ( FileID = 0; FileID < pCloneFS->pDirectory->Files.size(); FileID++ )
	{
		if ( WaitForSingleObject( hToolEvent, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		if ( pCloneFS->pDirectory->Files[ FileID ].Flags & FF_Directory )
		{
			MakeISOFilename( &pCloneFS->pDirectory->Files[ FileID ], ISOFilename );

			DWORD NextExtent = pCloneFS->pDirectory->Files[ FileID ].Attributes[ ISOATTR_START_EXTENT ];

			pScanTable->AddDirectory( BYTEString( ISOFilename ), NextExtent, ThisExtent );

			DoneEntries++;

			::SendMessage( ScanWnd, WM_FSTOOL_PROGRESS, Percent( 0, 1, DoneEntries, ScanEntries, false), 0 );

			(void) pCloneFS->ChangeDirectory( FileID );

			GatherDirs( pCloneFS, pScanTable, NextExtent );

			(void) pCloneFS->Parent();
		}
	}

	return 0;
}

int ISO9660FileSystem::CorrectDirs( FileSystem *pCloneFS, DWORD CurrentParent )
{
	// We must use an index rather than an interator, as calls to ChangeDirectory() and Parent()
	// will destroy and recreate the structure.

	DWORD FileID;

	BYTE ISOFilename[ 256 ];

	for ( FileID = 0; FileID < pCloneFS->pDirectory->Files.size(); FileID++ )
	{
		if ( WaitForSingleObject( hToolEvent, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		if ( pCloneFS->pDirectory->Files[ FileID ].Flags & FF_Directory )
		{
			MakeISOFilename( &pCloneFS->pDirectory->Files[ FileID ], ISOFilename );

			pCloneFS->pDirectory->Files[ FileID ].Attributes[ ISOATTR_START_EXTENT ] = pPathTable->ExtentForDir( BYTEString( ISOFilename) , CurrentParent );
		}
	}

	if ( pCloneFS->pDirectory->WriteDirectory() != NUTS_SUCCESS )
	{
		return -1;
	}

	for ( FileID = 0; FileID < pCloneFS->pDirectory->Files.size(); FileID++ )
	{
		if ( WaitForSingleObject( hToolEvent, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		if ( pCloneFS->pDirectory->Files[ FileID ].Flags & FF_Directory )
		{
			DWORD ParentExt = pCloneFS->pDirectory->Files[ FileID ].Attributes[ ISOATTR_START_EXTENT ];

			if ( ParentExt != CurrentParent )
			{
				// If we corrupted the image even further (!) and created a loop, don't try to recurse it.
				pCloneFS->ChangeDirectory( FileID );

				CorrectDirs( pCloneFS, ParentExt );

				pCloneFS->Parent();
			}
		}

		DoneEntries++;

		::SendMessage( ScanWnd, WM_FSTOOL_PROGRESS, Percent( 0, 1, DoneEntries, ScanEntries, false), 0 );
	}

	return 0;
}

int ISO9660FileSystem::RecordDirectorySectors( FileSystem *pCloneFS )
{
	DWORD FileID;

	for ( FileID = 0; FileID < pCloneFS->pDirectory->Files.size(); FileID++ )
	{
		if ( WaitForSingleObject( hToolEvent, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		DWORD s = pCloneFS->pDirectory->Files[ FileID ].Attributes[ ISOATTR_START_EXTENT ];
		DWORD l = ( (DWORD) pCloneFS->pDirectory->Files[ FileID ].Length + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;

		for ( DWORD i=0; i<l; i++ )
		{
			if ( ( s + i ) < PriVolDesc.VolSize ) 
			{
				pSectorsInUse[ s + i ] = true;
			}
		}

		if ( pCloneFS->pDirectory->Files[ FileID ].ExtraForks > 0 )
		{
			s = pCloneFS->pDirectory->Files[ FileID ].Attributes[ ISOATTR_FORK_EXTENT ];
			l = ( pCloneFS->pDirectory->Files[ FileID ].Attributes[ ISOATTR_FORK_LENGTH ] + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;

			for ( DWORD i=0; i<l; i++ )
			{
				if ( ( s + i ) < PriVolDesc.VolSize ) 
				{
					pSectorsInUse[ s + i ] = 0xFF;
				}
			}
		}

		if ( pCloneFS->pDirectory->Files[ FileID ].Flags & FF_Directory )
		{
			pCloneFS->ChangeDirectory( FileID );

			RecordDirectorySectors( pCloneFS );

			pCloneFS->Parent();
		}

	}

	DoneEntries++;

	::SendMessage( ScanWnd, WM_FSTOOL_PROGRESS, Percent( 0, 2, DoneEntries, ScanEntries, false), 0 );

	return 0;
}

int ISO9660FileSystem::RunTool( BYTE ToolNum, HWND ProgressWnd )
{
	ResetEvent( hToolEvent );

	// Tool 0: Rebuild path table
	// Tool 1: Rebuild directory tree
	// Tool 2: Remove unused sectors

	if ( ToolNum == 0 )
	{
		::SendMessage( ProgressWnd, WM_FSTOOL_SETDESC, 0, (LPARAM) L"This tool will examine directory structure, and construct a new path table from it." );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGLIMIT, 0, 100 );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 0, 1, 0, 0, false), 0 );

		// There are 3 steps:
		// 1. Recurse the directory structure and get the directory extents, adding them to a new path table object.
		// 2. Get the projected size of the path table and compare it with the current values. Expand or contract the path table sectors as required. (PerformISOJob)
		// 3. Write the path tables to the extents given in the volume descriptor

		ISOPathTable *pScanTable = new ISOPathTable( pSource, PriVolDesc.SectorSize );

		// Get a clone FS. We can use this to traverse, since we won't make changes here.
		FileSystem *pFS = Clone();

		// Get it to the root.
		while ( !pFS->IsRoot() ) { pFS->Parent(); }

		DWORD RootExtent = LEDWORD( &PriVolDesc.DirectoryRecord[ 2 ] );

		pScanTable->BlankTable( RootExtent );

		ScanEntries = pScanTable->GetNumEntries();
		DoneEntries = 0;
		ScanWnd     = ProgressWnd;

		if ( WaitForSingleObject( hToolEvent, 0 ) != WAIT_OBJECT_0 )
		{
			(void) GatherDirs( pFS, pScanTable, RootExtent );
		}

		delete pFS;

		if ( WaitForSingleObject( hToolEvent, 0 ) != WAIT_OBJECT_0 )
		{
			return 0;
		}

		DWORD NewTableSize = pScanTable->GetProjectedSize();

		DWORD NewTableSects = ( NewTableSize + ( PriVolDesc.SectorSize -1 ) ) / PriVolDesc.SectorSize;

		DWORD OldTableSize = pPathTable->GetProjectedSize();

		DWORD OldTableSects = ( OldTableSize + ( PriVolDesc.SectorSize -1 ) ) / PriVolDesc.SectorSize;

		if ( OldTableSects != NewTableSects )
		{
			// Table size changed. Let's work out what we need to do.
			if ( NewTableSects > OldTableSects )
			{
				DWORD NumSects = NewTableSects - OldTableSects;

				ISOSectorList sectors;

				for ( DWORD i=0; i<NumSects; i++ )
				{
					ISOSector sector;

					if ( PriVolDesc.LTableLoc != 0 )    { sector.ID = PriVolDesc.LTableLoc + OldTableSects +i; sectors.push_back( sector ); }
					if ( PriVolDesc.LTableLocOpt != 0 ) { sector.ID = PriVolDesc.LTableLocOpt + OldTableSects +i; sectors.push_back( sector ); }
					if ( PriVolDesc.MTableLoc != 0 )    { sector.ID = PriVolDesc.MTableLoc + OldTableSects +i; sectors.push_back( sector ); }
					if ( PriVolDesc.MTableLocOpt != 0 ) { sector.ID = PriVolDesc.MTableLocOpt + OldTableSects +i; sectors.push_back( sector ); }

					if ( FSID == FSID_ISOHS )
					{
						if ( PriVolDesc.LTableLocOptA != 0 ) { sector.ID = PriVolDesc.LTableLocOptA + OldTableSects +i; sectors.push_back( sector ); }
						if ( PriVolDesc.LTableLocOptB != 0 ) { sector.ID = PriVolDesc.LTableLocOptB + OldTableSects +i; sectors.push_back( sector ); }
						if ( PriVolDesc.MTableLocOptA != 0 ) { sector.ID = PriVolDesc.MTableLocOptA + OldTableSects +i; sectors.push_back( sector ); }
						if ( PriVolDesc.MTableLocOptB != 0 ) { sector.ID = PriVolDesc.MTableLocOptB + OldTableSects +i; sectors.push_back( sector ); }
					}
				}

				PrepareISOSectors( sectors );

				ISOJob job;

				job.FSID     = FSID;
				job.IsoOp    = ISO_RESTRUCTURE_ADD_SECTORS;
				job.pSource  = pSource;
				job.pVolDesc = &PriVolDesc;
				job.Sectors  = sectors;

				PerformISOJob( &job );
			}

			if ( NewTableSects < OldTableSects )
			{
				DWORD NumSects = OldTableSects - NewTableSects;

				ISOSectorList sectors;

				for ( DWORD i=0; i<NumSects; i++ )
				{
					ISOSector sector;

					if ( PriVolDesc.LTableLoc != 0 )    { sector.ID = PriVolDesc.LTableLoc + NewTableSects +i; sectors.push_back( sector ); }
					if ( PriVolDesc.LTableLocOpt != 0 ) { sector.ID = PriVolDesc.LTableLocOpt + NewTableSects +i; sectors.push_back( sector ); }
					if ( PriVolDesc.MTableLoc != 0 )    { sector.ID = PriVolDesc.MTableLoc + NewTableSects +i; sectors.push_back( sector ); }
					if ( PriVolDesc.MTableLocOpt != 0 ) { sector.ID = PriVolDesc.MTableLocOpt + NewTableSects +i; sectors.push_back( sector ); }

					if ( FSID == FSID_ISOHS )
					{
						if ( PriVolDesc.LTableLocOptA != 0 ) { sector.ID = PriVolDesc.LTableLocOptA + NewTableSects +i; sectors.push_back( sector ); }
						if ( PriVolDesc.LTableLocOptB != 0 ) { sector.ID = PriVolDesc.LTableLocOptB + NewTableSects +i; sectors.push_back( sector ); }
						if ( PriVolDesc.MTableLocOptA != 0 ) { sector.ID = PriVolDesc.MTableLocOptA + NewTableSects +i; sectors.push_back( sector ); }
						if ( PriVolDesc.MTableLocOptB != 0 ) { sector.ID = PriVolDesc.MTableLocOptB + NewTableSects +i; sectors.push_back( sector ); }
					}
				}

				PrepareISOSectors( sectors );

				ISOJob job;

				job.FSID     = FSID;
				job.IsoOp    = ISO_RESTRUCTURE_REM_SECTORS;
				job.pSource  = pSource;
				job.pVolDesc = &PriVolDesc;
				job.Sectors  = sectors;

				PerformISOJob( &job );
			}
		}

		// Now write the new path tables out.
		if ( PriVolDesc.LTableLoc != 0 )    { pScanTable->WritePathTable( PriVolDesc.LTableLoc,    false, FSID ); }
		if ( PriVolDesc.LTableLocOpt != 0 ) { pScanTable->WritePathTable( PriVolDesc.LTableLocOpt, false, FSID ); }
		if ( PriVolDesc.MTableLoc != 0 )    { pScanTable->WritePathTable( PriVolDesc.MTableLoc,    true,  FSID ); }
		if ( PriVolDesc.MTableLocOpt != 0 ) { pScanTable->WritePathTable( PriVolDesc.MTableLocOpt, true,  FSID ); }

		if ( FSID == FSID_ISOHS )
		{
			if ( PriVolDesc.LTableLocOptA != 0 ) { pScanTable->WritePathTable( PriVolDesc.LTableLocOptA, false, FSID ); }
			if ( PriVolDesc.LTableLocOptB != 0 ) { pScanTable->WritePathTable( PriVolDesc.LTableLocOptB, false, FSID ); }
			if ( PriVolDesc.MTableLocOptA != 0 ) { pScanTable->WritePathTable( PriVolDesc.MTableLocOptA, true,  FSID ); }
			if ( PriVolDesc.MTableLocOptB != 0 ) { pScanTable->WritePathTable( PriVolDesc.MTableLocOptB, true,  FSID ); }
		}

		delete pScanTable;

		::SendMessage( ScanWnd, WM_FSTOOL_PROGRESS, 100, 0 );

		return Init();;
	}

	if ( ToolNum == 1 )
	{
		::SendMessage( ProgressWnd, WM_FSTOOL_SETDESC, 0, (LPARAM) L"This tool will examine directory structure, and adjust the directory extents from the values in the path table. Note this may produce an inconsistent sector usage state." );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGLIMIT, 0, 100 );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 0, 3, 0, 0, false), 0 );

		if ( MessageBox( ProgressWnd,
			L"This tool will attempt to repair a defective ISO structure by rewriting directory locations according to the accompanying "
			L"locations in the path table. On a damaged ISO structure, this could result in data sectors becoming orphaned from any file "
			L"or directory objects, and thus inaccessible from most tools. If you use the 'Remove Unused Sectors' tool it will remove these "
			L"orphaned data blocks from the structure, which is an irreversible process.\r\n\r\n"
			L"Are you sure you want to proceed?",
			L"NUTS ISO Tool", MB_ICONEXCLAMATION | MB_YESNO ) == IDNO )
		{
			return 0;
		}

		// Go through the directory structure and get the extent for any entry that is a directory.
		// Then go back through and recurse any directory.

		// Get a clone FS. We can use this to traverse, since we won't make changes here.
		FileSystem *pFS = Clone();

		// Get it to the root.
		while ( !pFS->IsRoot() ) { pFS->Parent(); }

		DWORD RootExtent = LEDWORD( &PriVolDesc.DirectoryRecord[ 2 ] );

		ScanEntries = pPathTable->GetNumEntries();
		DoneEntries = 0;
		ScanWnd     = ProgressWnd;

		if ( WaitForSingleObject( hToolEvent, 0 ) != WAIT_OBJECT_0 )
		{
			(void) CorrectDirs( pFS, RootExtent );
		}

		delete pFS;

		(void) pDirectory->ReadDirectory();

		return 0;
	}

	if ( ToolNum == 2 )
	{
		::SendMessage( ProgressWnd, WM_FSTOOL_SETDESC, 0, (LPARAM) L"This tool will examine the ISO structure, and remove any sectors not in use. Note this may remove hidden data." );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGLIMIT, 0, 100 );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 0, 3, 0, 0, false), 0 );

		if ( MessageBox( ProgressWnd,
			L"This tool will attempt to remove unused sectors in order to reduce the size of the overall image. If you have used the "
			L"'Rebuild Directory Tree' tool this may have orphaned sectors containing data from the structure, in which case this tool "
			L"will remove them without option for recovery.\r\n\r\n"
			L"Additionally, be careful of using this tool on commercial images that may have hidden sectors as a copy protection mechanism, "
			L"as they may also be removed resulting in an unusable disc.\r\n\r\n"
			L"Are you sure you wish to proceed?",
			L"NUTS ISO Tool", MB_ICONEXCLAMATION | MB_YESNO ) == IDNO )
		{
			return 0;
		}

		SectorsInUse = new AutoBuffer( PriVolDesc.VolSize );

		pSectorsInUse = (BYTE *) (*SectorsInUse);

		ZeroMemory( pSectorsInUse, PriVolDesc.VolSize );

		// Add the first 16.
		for ( DWORD i=0; i<16; i++ ) { if ( i < PriVolDesc.VolSize ) { pSectorsInUse[ i ] = 0xFF; } }

		// Add the VD set
		AutoBuffer sec( PriVolDesc.SectorSize );

		DWORD s = 0;

		BYTE *pSec = (BYTE *) sec;

		while ( s < PriVolDesc.VolSize ) 
		{
			if ( pSource->ReadSectorLBA( s, pSec, PriVolDesc.SectorSize ) != DS_SUCCESS )
			{
				return -1;
			}

			if ( s < PriVolDesc.VolSize ) { pSectorsInUse[ s ] = 0xFF; }

			if ( FSID == FSID_ISOHS )
			{
				if ( pSec[ 8 ] == 0xFF )
				{
					break;
				}
			}
			else
			{
				if ( pSec[ 0 ] == 0xFF )
				{
					break;
				}
			}

			s++;
		}

		// Add the path tables
		DWORD PathTableSects = ( PriVolDesc.PathTableSize + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;

		if ( PriVolDesc.LTableLoc != 0 )    { for ( DWORD i = 0; i<PathTableSects; i++ ) { if ( i < PriVolDesc.VolSize ) { pSectorsInUse[ PriVolDesc.LTableLoc    + i ] = 0xFF; } } }
		if ( PriVolDesc.LTableLocOpt != 0 ) { for ( DWORD i = 0; i<PathTableSects; i++ ) { if ( i < PriVolDesc.VolSize ) { pSectorsInUse[ PriVolDesc.LTableLocOpt + i ] = 0xFF; } } }
		if ( PriVolDesc.MTableLoc != 0 )    { for ( DWORD i = 0; i<PathTableSects; i++ ) { if ( i < PriVolDesc.VolSize ) { pSectorsInUse[ PriVolDesc.MTableLoc    + i ] = 0xFF; } } }
		if ( PriVolDesc.MTableLocOpt != 0 ) { for ( DWORD i = 0; i<PathTableSects; i++ ) { if ( i < PriVolDesc.VolSize ) { pSectorsInUse[ PriVolDesc.MTableLocOpt + i ] = 0xFF; } } }

		if ( FSID == FSID_ISOHS )
		{
			if ( PriVolDesc.LTableLocOptA != 0 ) { for ( DWORD i = 0; i<PathTableSects; i++ ) { if ( i < PriVolDesc.VolSize ) { pSectorsInUse[ PriVolDesc.LTableLocOptA + i ] = 0xFF; } } }
			if ( PriVolDesc.LTableLocOptB != 0 ) { for ( DWORD i = 0; i<PathTableSects; i++ ) { if ( i < PriVolDesc.VolSize ) { pSectorsInUse[ PriVolDesc.LTableLocOptB + i ] = 0xFF; } } }
			if ( PriVolDesc.MTableLocOptA != 0 ) { for ( DWORD i = 0; i<PathTableSects; i++ ) { if ( i < PriVolDesc.VolSize ) { pSectorsInUse[ PriVolDesc.MTableLocOptA + i ] = 0xFF; } } }
			if ( PriVolDesc.MTableLocOptB != 0 ) { for ( DWORD i = 0; i<PathTableSects; i++ ) { if ( i < PriVolDesc.VolSize ) { pSectorsInUse[ PriVolDesc.MTableLocOptB + i ] = 0xFF; } } }
		}

		// Add directories
		// Get a clone FS. We can use this to traverse, since we won't make changes here.
		FileSystem *pFS = Clone();

		// Get it to the root.
		while ( !pFS->IsRoot() ) { pFS->Parent(); }

		DWORD RootExtent = LEDWORD( &PriVolDesc.DirectoryRecord[  2 ] );
		DWORD RootSize   = LEDWORD( &PriVolDesc.DirectoryRecord[ 10 ] );

		RootSize = ( RootSize + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;

		for ( DWORD i=0; i<RootSize; i++ ) { pSectorsInUse[ RootExtent + i ] = true; }

		ScanEntries = pPathTable->GetNumEntries() + 1;
		DoneEntries = 0;
		ScanWnd     = ProgressWnd;

		if ( WaitForSingleObject( hToolEvent, 0 ) != WAIT_OBJECT_0 )
		{
			(void) RecordDirectorySectors( pFS );
		}

		delete pFS;

		::SendMessage( ScanWnd, WM_FSTOOL_PROGRESS, Percent( 1, 2, 0, PriVolDesc.VolSize, false), 0 );

		// Now prepare some sectors for deletion
		ISOSectorList sectors;

		for ( DWORD i=0; i<PriVolDesc.VolSize; i++ )
		{
			if ( WaitForSingleObject( hToolEvent, 0 ) == WAIT_OBJECT_0 )
			{
				return 0;
			}

			if ( ( i < PriVolDesc.VolSize ) && ( pSectorsInUse[ i ] == 0x00 ) )
			{
				ISOSector sector;

				sector.ID = i;

				sectors.push_back( sector );
			}

			if ( ( i % 10 ) == 0 )
			{
				::SendMessage( ScanWnd, WM_FSTOOL_PROGRESS, Percent( 1, 2, i, PriVolDesc.VolSize, false), 0 );
			}
		}

		if ( sectors.size() > 0 )
		{
			PrepareISOSectors( sectors );

			ISOJob job;

			job.FSID       = FSID;
			job.IsoOp      = ISO_RESTRUCTURE_REM_SECTORS;
			job.pSource    = pSource;
			job.pVolDesc   = &PriVolDesc;
			job.Sectors    = sectors;
			job.SectorSize = PriVolDesc.SectorSize;

			PerformISOJob( &job );
		}

		std::wstring Msg = std::to_wstring( (QWORD) sectors.size() ) + L" unneeded sector(s) were deleted.";

		(void) MessageBox( ProgressWnd, Msg.c_str(), L"NUTS ISO Tool", MB_ICONINFORMATION | MB_OK );

		::SendMessage( ScanWnd, WM_FSTOOL_PROGRESS, 100, 0 );

		delete SectorsInUse;

		return Init();
	}

	return -1;
}

static DataSource *pEditSource;
static HWND        pCurrentTab = nullptr;
static DWORD       VDEFSID;
static ISOVolDesc  *pVD; // Used for any required disc params.
static ISOVolDesc  *pIVD;
static ISOVolDesc  *pJVD;
static FileSystem  *pEditFS; // Used for locating files

INT_PTR CALLBACK SysAreaFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if ( message == WM_INITDIALOG )
	{
		return (INT_PTR) TRUE;
	}

	if ( message == WM_COMMAND )
	{
		if ( LOWORD( wParam ) == IDC_ISO_NEWSYSAREA )
		{
			std::wstring Filename;

			if ( OpenAnyFileDialog( hDlg, Filename, L"Select System Use Area Data File" ) )
			{
				CTempFile SysArea( Filename );

				SysArea.Keep();

				if ( SysArea.Ext() > ( 16 * pVD->SectorSize ) )
				{
					std::wstring bytes = std::to_wstring( (QWORD) 16 * pVD->SectorSize );

					std::wstring msg = std::wstring(
						L"The selected file is too large to be installed to the System Use Area. The maximum size allowed "
						L"on this disc is " + bytes + L" bytes"
					);

					MessageBox( hDlg, msg.c_str(), L"NUTS ISO Volume Descriptor Editor", MB_ICONEXCLAMATION | MB_OK );
				}
				else
				{
					AutoBuffer buf( pVD->SectorSize );

					DWORD BytesToGo = (DWORD) SysArea.Ext();

					DWORD Sect = 0;

					SysArea.Seek( 0 );

					while ( BytesToGo > 0 )
					{
						DWORD BytesToRead = min( BytesToGo, pVD->SectorSize );

						ZeroMemory( (BYTE *) buf, pVD->SectorSize );

						SysArea.Read( (BYTE *) buf, BytesToRead );

						if ( pEditSource->WriteSectorLBA( Sect, (BYTE *) buf, pVD->SectorSize ) != DS_SUCCESS )
						{
							NUTSError::Report( L"Installing System Use Area", hDlg );

							break;
						}

						Sect++;

						BytesToGo -= BytesToRead;
					}

					MessageBox( hDlg, L"System Use Area Installed", L"NUTS ISO Volume Descriptor Editor", MB_ICONINFORMATION | MB_OK );
				}
			}
		}

		if ( LOWORD( wParam ) == IDC_ISO_SAVESYSAREA )
		{
			std::wstring Filename;

			if ( SaveFileDialog( hDlg, Filename, L"Binary File", L"bin", L"Save ISO System Use Area" ) )
			{
				CTempFile SysArea;

				AutoBuffer buf( pVD->SectorSize );

				// We don't know how much of the Sys Area is actually used, so just grab the lot
				DWORD BytesToGo = 16 * pVD->SectorSize;

				DWORD Sect = 0;

				SysArea.Seek( 0 );

				while ( BytesToGo > 0 )
				{
					DWORD BytesToRead = min( BytesToGo, pVD->SectorSize );

					ZeroMemory( (BYTE *) buf, pVD->SectorSize );

					if ( pEditSource->ReadSectorLBA( Sect, (BYTE *) buf, pVD->SectorSize ) != DS_SUCCESS )
					{
						NUTSError::Report( L"Saving System Use Area", hDlg );

						break;
					}

					SysArea.Write( (BYTE *) buf, BytesToRead );

					Sect++;

					BytesToGo -= BytesToRead;
				}

				SysArea.SetExt( 16 * pVD->SectorSize );
				SysArea.KeepAs( Filename );

				MessageBox( hDlg, L"System Use Area Saved", L"NUTS ISO Volume Descriptor Editor", MB_ICONINFORMATION | MB_OK );
			}
		}

		return (INT_PTR) TRUE;
	}

	return FALSE;
}

DWORD FindDescriptor( bool CreateIfNone, BYTE DescType )
{
	DWORD Sect = 16;

	BYTE buf[ 2048 ];

	while ( Sect < 32 ) // Let's be realistic
	{
		pEditSource->ReadSectorLBA( Sect, buf, 2048 );

		BYTE DType = buf[ 0 ];

		if ( VDEFSID == FSID_ISOHS )
		{
			DType = buf[ 8 ];
		}

		if ( DType == DescType )
		{
			// Aha!
			return Sect;
		}

		if ( DType == 0xFF )
		{
			break;
		}

		Sect++;
	}

	if ( !CreateIfNone )
	{
		// Not found -\:)/-
		return 0;
	}

	// Insert one then.
	ISOSectorList sectors;
	ISOSector     sector;
	ISOJob        job;

	sector.ID = 16;

	sectors.push_back( sector );
	
	PrepareISOSectors( sectors );

	job.FSID       = VDEFSID;
	job.IsoOp      = ISO_RESTRUCTURE_ADD_SECTORS;
	job.pSource    = pEditSource;
	job.pVolDesc   = pVD;
	job.Sectors    = sectors;
	job.SectorSize = pVD->SectorSize;

	(void) PerformISOJob( &job );

	return 16;
}

INT_PTR CALLBACK BootDescFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if ( message == WM_NOTIFY )
	{
		return TRUE;
	}

	if ( message == WM_COMMAND )
	{
		if ( LOWORD( wParam ) == IDC_ISO_NEWBOOTCODE )
		{
			std::wstring Filename;

			if ( OpenAnyFileDialog( hDlg, Filename, L"Select Boot Descriptor Data File" ) )
			{
				CTempFile BootFile( Filename );

				BootFile.Keep();

				// "Where does 2032 come from?"
				// Well, a BVD still needs 8 bytes for the VD header. On High Sierra that's reduced a
				// further 8 bytes for the Volume LBN. The sector can't be bigger than the smallest sector
				// size (2048 bytes) because the sector size is not known at use time (only that it is
				// at least 2048 bytes). 2048 - ( 8 + 8 ) = 2032.
				// Technically, ISO9660 could reclaim 8 bytes for the BVD. But we'll just impose a limit
				// here for simplicity.

				if ( BootFile.Ext() > 2032 )
				{
					std::wstring msg = std::wstring(
						L"The selected file is too large to be installed to a Boot Descriptor. The maximum size allowed "
						L"is 2,032 bytes."
					);

					MessageBox( hDlg, msg.c_str(), L"NUTS ISO Volume Descriptor Editor", MB_ICONEXCLAMATION | MB_OK );
				}
				else
				{
					BYTE buf[ 2048 ];

					BootFile.Seek( 0 );

					ZeroMemory( (BYTE *) buf, 2048 );

					BootFile.Read( &buf[ 7 ], min( BootFile.Ext(), 2032 ) );

					DWORD Sect = FindDescriptor( true, 0x00 );

					if ( pEditSource->WriteSectorLBA( Sect, (BYTE *) buf, 2048 ) != DS_SUCCESS )
					{
						NUTSError::Report( L"Installing Boot Descriptor", hDlg );
					}
					else
					{
						MessageBox( hDlg, L"Boot Descriptor Installed", L"NUTS ISO Volume Descriptor Editor", MB_ICONINFORMATION | MB_OK );
					}
				}
			}
		}

		if ( LOWORD( wParam ) == IDC_ISO_SAVEBOOTCODE )
		{
			std::wstring Filename;

			DWORD Sect = FindDescriptor( false, 0x00 );

			if ( Sect != 0 )
			{
				if ( SaveFileDialog( hDlg, Filename, L"Binary File", L"bin", L"Save ISO Boot Descriptor" ) )
				{
					CTempFile BootFile;

					BYTE buf[ 2048 ];

					BootFile.Seek( 0 );

					ZeroMemory( (BYTE *) buf, 2048 );

					if ( pEditSource->ReadSectorLBA( Sect, (BYTE *) buf, 2048 ) != DS_SUCCESS )
					{
						NUTSError::Report( L"Saving Boot Descriptor hDlg", hDlg );
					}
					else
					{
						BootFile.Write( &buf[ 7 ], 2032 );

						BootFile.KeepAs( Filename );

						MessageBox( hDlg, L"Boot Descriptor Saved", L"NUTS ISO Volume Descriptor Editor", MB_ICONINFORMATION | MB_OK );
					}
				}
			}
			else
			{
				MessageBox( hDlg, L"No Boot Descriptor Found", L"NUTS ISO Volume Descriptor Editor", MB_ICONWARNING | MB_OK );
			}
		}

		return (INT_PTR) TRUE;
	}

	return FALSE;
}

void ISODTToSystemTimeID( HWND hDlg, BYTE *pTime, DWORD id )
{
	SYSTEMTIME sTime;

	int y,mo,d,h,m,s,ms;

	int n = sscanf( (char *) pTime, "%04d%02d%02d%02d%02d%02d%02d",
		&y, &mo, &d, &h, &m, &s, &ms
	);

	if ( y == 0 ) { n = 0; }

	if ( n == 7 )
	{
		sTime.wYear         = y;
		sTime.wMonth        = mo;
		sTime.wDay          = d;
		sTime.wHour         = h;
		sTime.wMinute       = m;
		sTime.wSecond       = s;
		sTime.wMilliseconds = ms * 10;

		::SendMessage( GetDlgItem( hDlg, (int) id ), DTM_SETSYSTEMTIME, (WPARAM) GDT_VALID, (LPARAM) &sTime );
	}
	else
	{
		::SendMessage( GetDlgItem( hDlg, (int) id ), DTM_SETSYSTEMTIME, (WPARAM) GDT_NONE, (LPARAM) &sTime );
	}

	::SendMessage( GetDlgItem( hDlg, (int) id ), DTM_SETFORMAT, 0, (LPARAM) L"dd/MM/yy hh:mm" );
}

static EncodingEdit *pSysID;
static EncodingEdit *pVolID;
static EncodingEdit *pVolSetID;
static EncodingEdit *pPublisher;
static EncodingEdit *pPreprarer;
static EncodingEdit *pApplication;

bool DChars( BYTE c )
{
	if ( ( c >= 'A' ) && ( c <= 'Z' ) ) { return true; }
	if ( ( c >= '0' ) && ( c <= '9' ) ) { return true; }
	if ( c == '_' ) { return true; }

	return false;
}

bool AChars( BYTE c )
{
	if ( DChars( c ) ) { return true; }

	const BYTE AX[ 20 ] = { '!', '"', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '<', '=', '>', '?', ' ' };

	for ( BYTE i=0; i<20; i++ )
	{
		if ( c == AX[ i ] ) { return true; }
	}

	return false;
}


EncodingEdit *CreateEditForISO( HWND hDlg, BYTE *pString, int x, int y, fnCharValidator pValidatorFunc )
{
	EncodingEdit *pEdit = new EncodingEdit( hDlg, x, y, 132, false );

	pEdit->SetText( pString );

	pEdit->Encoding     = ENCODING_ASCII;
	pEdit->AllowedChars = EncodingEdit::textInputAny;
	pEdit->MaxLen       = 37;
	pEdit->pValidator   = pValidatorFunc;

	return pEdit;
}

INT_PTR CALLBACK NullFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	return (INT_PTR) FALSE;
}

INT_PTR CALLBACK VDFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
		{
			std::wstring AppDataPath;

			if ( LOWORD( wParam ) == IDC_ISO_SAVEAPPDATA )
			{
				if ( SaveFileDialog( hDlg, AppDataPath, L"Binary Data", L"bin", L"Save Descriptor Applicatioin Data" ) )
				{
					CTempFile AppData;

					AppData.Seek( 0 );

					AppData.Write( pVD->ApplicationBytes, 512 );

					AppData.KeepAs( AppDataPath );
				}
			}

			if ( LOWORD( wParam ) == IDC_ISO_INSTALLAPPDATA )
			{
				if ( OpenAnyFileDialog( hDlg, AppDataPath, L"Select Descriptor Application Data File" ) )
				{
					CTempFile AppData( AppDataPath );

					AppData.Keep();

					AppData.Seek( 0 );

					AppData.Read( pVD->ApplicationBytes, 512 );
				}
			}
		}
		break;

	case WM_INITDIALOG:
		{
			DWORD FC = 1;

			for ( int i=0; i < pEditFS->pDirectory->Files.size(); i++ )
			{
				BYTE ISOFilename[ 256 ];

				std::string sfile;

				if ( i == 0 )
				{
					sfile = "[None]";

					::SendMessageA( GetDlgItem( hDlg, IDC_ISO_COPYRIGHTFILE ),     CB_ADDSTRING, 0, (LPARAM) sfile.c_str() );
					::SendMessageA( GetDlgItem( hDlg, IDC_ISO_ABSTRACTFILE ),      CB_ADDSTRING, 0, (LPARAM) sfile.c_str() );
					::SendMessageA( GetDlgItem( hDlg, IDC_ISO_BIBLIOGRAPHICFILE ), CB_ADDSTRING, 0, (LPARAM) sfile.c_str() );

					::SendMessageA( GetDlgItem( hDlg, IDC_ISO_COPYRIGHTFILE ),     CB_SETCURSEL, 0, 0 );
					::SendMessageA( GetDlgItem( hDlg, IDC_ISO_ABSTRACTFILE ),      CB_SETCURSEL, 0, 0 );
					::SendMessageA( GetDlgItem( hDlg, IDC_ISO_BIBLIOGRAPHICFILE ), CB_SETCURSEL, 0, 0 );

					::SetWindowPos( GetDlgItem( hDlg, IDC_ISO_BIBLIOGRAPHICFILE ), NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
					::SetWindowPos( GetDlgItem( hDlg, IDC_ISO_ABSTRACTFILE ),      NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
					::SetWindowPos( GetDlgItem( hDlg, IDC_ISO_COPYRIGHTFILE ),     NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
				}
				else
				{
					if ( pEditFS->pDirectory->Files[ i ].Flags & FF_Directory )
					{
						continue;
					}

					MakeISOFilename( &pEditFS->pDirectory->Files[ i ], ISOFilename, false );

					sfile = std::string( (char *) ISOFilename );

					::SendMessageA( GetDlgItem( hDlg, IDC_ISO_COPYRIGHTFILE ), CB_ADDSTRING, 0, (LPARAM) sfile.c_str() );

					if ( rstrcmp( ISOFilename, pVD->CopyrightFilename ) )
					{
						::SendMessageA( GetDlgItem( hDlg, IDC_ISO_COPYRIGHTFILE ), CB_SETCURSEL, FC, 0 );
					}

					::SendMessageA( GetDlgItem( hDlg, IDC_ISO_ABSTRACTFILE ), CB_ADDSTRING, 0, (LPARAM) sfile.c_str() );

					if ( rstrcmp( ISOFilename, pVD->AbstractFilename ) )
					{
						::SendMessageA( GetDlgItem( hDlg, IDC_ISO_ABSTRACTFILE ), CB_SETCURSEL, FC, 0 );
					}

					if ( VDEFSID == FSID_ISO9660 )
					{
						::SendMessageA( GetDlgItem( hDlg, IDC_ISO_BIBLIOGRAPHICFILE ), CB_ADDSTRING, 0, (LPARAM) sfile.c_str() );

						if ( rstrcmp( ISOFilename, pVD->BibliographicFilename ) )
						{
							::SendMessageA( GetDlgItem( hDlg, IDC_ISO_BIBLIOGRAPHICFILE ), CB_SETCURSEL, FC, 0 );
						}
					}

					FC++;
				}
			}

			ISODTToSystemTimeID( hDlg, pVD->CreationTime,  IDC_ISO_CREATION );
			ISODTToSystemTimeID( hDlg, pVD->ModTime,       IDC_ISO_MODIFICATION );
			ISODTToSystemTimeID( hDlg, pVD->ExpireTime,    IDC_ISO_EXPIRATION );
			ISODTToSystemTimeID( hDlg, pVD->EffectiveTime, IDC_ISO_EFFECTIVE );

			if ( VDEFSID == FSID_ISOHS )
			{
				::ShowWindow( GetDlgItem( hDlg, IDC_ISO_BIBPROMPT ), SW_HIDE );
				::ShowWindow( GetDlgItem( hDlg, IDC_ISO_BIBLIOGRAPHICFILE ), SW_HIDE );
			}

			pSysID       = CreateEditForISO( hDlg, pVD->SysID,     100,  8, AChars );
			pVolID       = CreateEditForISO( hDlg, pVD->VolID,     320,  8, nullptr ); // TECHNICALLY DChars, but many ISOs violate this
			pVolSetID    = CreateEditForISO( hDlg, pVD->VolSetID,  100, 34, DChars );
			pPublisher   = CreateEditForISO( hDlg, pVD->Publisher, 320, 34, AChars );
			pPreprarer   = CreateEditForISO( hDlg, pVD->Preparer,  100, 60, AChars );
			pApplication = CreateEditForISO( hDlg, pVD->AppID,     320, 60, AChars );

			::SetWindowPos( pApplication->hWnd, NULL, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE );
			::SetWindowPos( pPreprarer->hWnd,   NULL, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE );
			::SetWindowPos( pPublisher->hWnd,   NULL, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE );
			::SetWindowPos( pVolSetID->hWnd,    NULL, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE );
			::SetWindowPos( pVolID->hWnd,       NULL, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE );
			::SetWindowPos( pSysID->hWnd,       NULL, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE );

			pSysID->MaxLen = 32;
			pVolID->MaxLen = 32;

			// Lets do the Path Tables then
			CheckRadioButton( hDlg, IDC_LTABLE1, IDC_LTABLE1, IDC_LTABLE1 );
			if ( pVD->LTableLocOpt != 0 ) { CheckRadioButton( hDlg, IDC_LTABLE1, IDC_LTABLE2, IDC_LTABLE1 ); }
			if ( ( VDEFSID == FSID_ISOHS ) && ( pVD->LTableLocOptA != 0 ) ) { CheckRadioButton( hDlg, IDC_LTABLE1, IDC_LTABLE3, IDC_LTABLE1 ); }
			if ( ( VDEFSID == FSID_ISOHS ) && ( pVD->LTableLocOptB != 0 ) ) { CheckRadioButton( hDlg, IDC_LTABLE1, IDC_LTABLE4, IDC_LTABLE1 ); }

			CheckRadioButton( hDlg, IDC_MTABLE1, IDC_MTABLE1, IDC_MTABLE1 );
			if ( pVD->MTableLocOpt != 0 ) { CheckRadioButton( hDlg, IDC_MTABLE1, IDC_MTABLE2, IDC_MTABLE1 ); }
			if ( ( VDEFSID == FSID_ISOHS ) && ( pVD->MTableLocOptA != 0 ) ) { CheckRadioButton( hDlg, IDC_MTABLE1, IDC_MTABLE3, IDC_MTABLE1 ); }
			if ( ( VDEFSID == FSID_ISOHS ) && ( pVD->MTableLocOptB != 0 ) ) { CheckRadioButton( hDlg, IDC_MTABLE1, IDC_MTABLE4, IDC_MTABLE1 ); }

			if ( VDEFSID == FSID_ISO9660 )
			{
				ShowWindow( GetDlgItem( hDlg, IDC_LTABLE3 ), SW_HIDE );
				ShowWindow( GetDlgItem( hDlg, IDC_LTABLE4 ), SW_HIDE );
				ShowWindow( GetDlgItem( hDlg, IDC_MTABLE3 ), SW_HIDE );
				ShowWindow( GetDlgItem( hDlg, IDC_MTABLE4 ), SW_HIDE );
			}
		}
	}

	return FALSE;
}

void SystemTimeToISOTimeID( HWND hDlg, BYTE *TimeSpec, int DID )
{
	SYSTEMTIME SysTime;

	ZeroMemory( &SysTime, sizeof( SYSTEMTIME ) );

	if ( ::SendMessage( GetDlgItem( hDlg, DID ), DTM_GETSYSTEMTIME, 0, (LPARAM) &SysTime ) == GDT_VALID )
	{
		rsprintf( TimeSpec, "%04d%02d%02d%02d%02d%02d%02d",
			SysTime.wYear, SysTime.wMonth, SysTime.wDay,
			SysTime.wHour, SysTime.wMinute, SysTime.wSecond,
			SysTime.wMilliseconds / 10
		);
	}
	else
	{
		rstrncpy( TimeSpec, (BYTE *) "0000000000000000", 17 );
	}
}

void DeleteExtraEditBoxes()
{
	if ( pSysID != nullptr )       { delete pSysID;       pSysID       = nullptr; }
	if ( pVolID != nullptr )       { delete pVolID;       pVolID       = nullptr; }
	if ( pVolSetID != nullptr )    { delete pVolSetID;    pVolSetID    = nullptr; }
	if ( pPublisher != nullptr )   { delete pPublisher;   pPublisher   = nullptr; }
	if ( pPreprarer != nullptr )   { delete pPreprarer;   pPreprarer   = nullptr; }
	if ( pApplication != nullptr ) { delete pApplication; pApplication = nullptr; }
}

void RecoverEditedVD( HWND hDlg )
{
	if ( pVD == nullptr )
	{
		return;
	}

	rstrncpy( pVD->SysID,     pSysID->GetText(),       32 );
	rstrncpy( pVD->VolID,     pVolID->GetText(),       32 );
	rstrncpy( pVD->VolSetID,  pVolSetID->GetText(),    37 );
	rstrncpy( pVD->Publisher, pPublisher->GetText(),   37 );
	rstrncpy( pVD->Preparer,  pPreprarer->GetText(),   37 );
	rstrncpy( pVD->AppID,     pApplication->GetText(), 37 );

	DeleteExtraEditBoxes();

	SystemTimeToISOTimeID( hDlg, pVD->CreationTime,  IDC_ISO_CREATION );
	SystemTimeToISOTimeID( hDlg, pVD->ModTime,       IDC_ISO_MODIFICATION );
	SystemTimeToISOTimeID( hDlg, pVD->ExpireTime,    IDC_ISO_EXPIRATION );
	SystemTimeToISOTimeID( hDlg, pVD->EffectiveTime, IDC_ISO_EFFECTIVE );

	// Go through the files in the FS and match it up with the indices in the drop downs
	DWORD CopyrightFile     = ::SendMessage( GetDlgItem( hDlg, IDC_ISO_COPYRIGHTFILE ),     CB_GETCURSEL, 0, 0 );
	DWORD AbstractFile      = ::SendMessage( GetDlgItem( hDlg, IDC_ISO_ABSTRACTFILE ),      CB_GETCURSEL, 0, 0 );
	DWORD BibliographicFile = ::SendMessage( GetDlgItem( hDlg, IDC_ISO_BIBLIOGRAPHICFILE ), CB_GETCURSEL, 0, 0 );

	if ( CopyrightFile == 0 )     { rstrncpy( pVD->CopyrightFilename,     (BYTE *) "                 ", 17 ); }
	if ( AbstractFile == 0 )      { rstrncpy( pVD->AbstractFilename,      (BYTE *) "                 ", 17 ); }
	if ( BibliographicFile == 0 ) { rstrncpy( pVD->BibliographicFilename, (BYTE *) "                 ", 17 ); }

	DWORD FC = 1;

	for ( DWORD i=0; i<pEditFS->pDirectory->Files.size(); i++ )
	{
		if ( pEditFS->pDirectory->Files[ i ].Flags & FF_Directory )
		{
			continue;
		}

		if ( FC == CopyrightFile )
		{
			rstrncpy( pVD->CopyrightFilename, (BYTE *) pEditFS->pDirectory->Files[ i ].Filename, min( pEditFS->pDirectory->Files[ i ].Filename.length(), 17 ) );
		}

		if ( FC == AbstractFile )
		{
			rstrncpy( pVD->AbstractFilename, (BYTE *) pEditFS->pDirectory->Files[ i ].Filename, min( pEditFS->pDirectory->Files[ i ].Filename.length(), 17 ) );
		}

		if ( FC == BibliographicFile )
		{
			rstrncpy( pVD->BibliographicFilename, (BYTE *) pEditFS->pDirectory->Files[ i ].Filename, min( pEditFS->pDirectory->Files[ i ].Filename.length(), 17 ) );
		}

		FC++;
	}

	// Path table counts
	DWORD LTables = 1;
	DWORD MTables = 1;

	if ( Button_GetState( GetDlgItem( hDlg, IDC_LTABLE2 ) ) == BST_CHECKED ) { LTables = 2 ; }
	if ( Button_GetState( GetDlgItem( hDlg, IDC_LTABLE3 ) ) == BST_CHECKED ) { LTables = 3 ; }
	if ( Button_GetState( GetDlgItem( hDlg, IDC_LTABLE4 ) ) == BST_CHECKED ) { LTables = 4 ; }

	if ( Button_GetState( GetDlgItem( hDlg, IDC_MTABLE2 ) ) == BST_CHECKED ) { MTables = 2 ; }
	if ( Button_GetState( GetDlgItem( hDlg, IDC_MTABLE3 ) ) == BST_CHECKED ) { MTables = 3 ; }
	if ( Button_GetState( GetDlgItem( hDlg, IDC_MTABLE4 ) ) == BST_CHECKED ) { MTables = 4 ; }

	// Cap for ISO9660
	if ( VDEFSID == FSID_ISO9660 ) { LTables = min( 2, LTables ); MTables = min( 2, MTables ); }

	// Now make the decisions
	if ( ( pVD->LTableLocOpt == 0 ) && ( LTables > 1 ) ) { pVD->LTableLocOpt = 0xFFFFFFFF; }
	if ( ( pVD->MTableLocOpt == 0 ) && ( MTables > 1 ) ) { pVD->MTableLocOpt = 0xFFFFFFFF; }

	if ( ( pVD->LTableLocOpt != 0 ) && ( LTables < 2 ) ) { pVD->LTableLocOpt = 0x00000000; }
	if ( ( pVD->MTableLocOpt != 0 ) && ( MTables < 2 ) ) { pVD->MTableLocOpt = 0x00000000; }

	if ( VDEFSID == FSID_ISOHS )
	{
		if ( ( pVD->LTableLocOptA == 0 ) && ( LTables > 2 ) ) { pVD->LTableLocOptA = 0xFFFFFFFF; }
		if ( ( pVD->LTableLocOptB == 0 ) && ( LTables > 3 ) ) { pVD->LTableLocOptB = 0xFFFFFFFF; }
		if ( ( pVD->MTableLocOptA == 0 ) && ( MTables > 2 ) ) { pVD->MTableLocOptA = 0xFFFFFFFF; }
		if ( ( pVD->MTableLocOptB == 0 ) && ( MTables > 3 ) ) { pVD->MTableLocOptB = 0xFFFFFFFF; }

		if ( ( pVD->LTableLocOptA != 0 ) && ( LTables < 3 ) ) { pVD->LTableLocOptA = 0x00000000; }
		if ( ( pVD->LTableLocOptB != 0 ) && ( LTables < 4 ) ) { pVD->LTableLocOptB = 0x00000000; }
		if ( ( pVD->MTableLocOptA != 0 ) && ( MTables < 3 ) ) { pVD->MTableLocOptA = 0x00000000; }
		if ( ( pVD->MTableLocOptB != 0 ) && ( MTables < 4 ) ) { pVD->MTableLocOptB = 0x00000000; }
	}
}

INT_PTR CALLBACK VDEditBox(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			if ( pVD != nullptr )
			{
				RecoverEditedVD( pCurrentTab );
			}

			EndDialog(hDlg, LOWORD(wParam));

			return (INT_PTR)TRUE;
		}
	}
	break;

	case WM_DESTROY:
		{
			DeleteExtraEditBoxes();
		}
		break;

	case WM_INITDIALOG:
		{
			TCITEM tie;

			tie.mask = TCIF_TEXT;

			tie.pszText = L"System Use Area";
			TabCtrl_InsertItem( GetDlgItem( hDlg, IDC_VD_TABS ), 1, &tie );

			tie.pszText = L"Boot";
			TabCtrl_InsertItem( GetDlgItem( hDlg, IDC_VD_TABS ), 2, &tie );

			if ( VDEFSID == FSID_ISO9660 )
			{
				tie.pszText = L"ISO Primary";
				TabCtrl_InsertItem( GetDlgItem( hDlg, IDC_VD_TABS ), 3, &tie );

				tie.pszText = L"Joliet Secondary";
				TabCtrl_InsertItem( GetDlgItem( hDlg, IDC_VD_TABS ), 4, &tie );
			}
			else
			{
				tie.pszText = L"High Sierra Primary";
				TabCtrl_InsertItem( GetDlgItem( hDlg, IDC_VD_TABS ), 3, &tie );
			}

			NMHDR nm;

			TabCtrl_SetCurSel( GetDlgItem( hDlg, IDC_VD_TABS ), 0 );

			nm.code = TCN_SELCHANGE;

			::SendMessage( hDlg, WM_NOTIFY, 0, (LPARAM) &nm );

			return (INT_PTR) TRUE;
		}

	case WM_NOTIFY:
		{
			NMHDR *pnm = (NMHDR *) lParam;

			if ( pnm->code == TCN_SELCHANGING )
			{
				RecoverEditedVD( pCurrentTab );
			}

			if ( pnm->code == TCN_SELCHANGE )
			{
				/* New tab please */
				if ( pCurrentTab != nullptr )
				{
					DestroyWindow( pCurrentTab );
				}

				int iSel = TabCtrl_GetCurSel( GetDlgItem( hDlg, IDC_VD_TABS ) );

				if ( iSel == 0 )
				{
					pVD = nullptr;

					HRSRC hRes = FindResource( hInst, MAKEINTRESOURCE( IDD_ISO_SYSUSEAREA ), RT_DIALOG );
					HGLOBAL hG = LoadResource( hInst, hRes );

					LPDLGTEMPLATEW dlg = (LPDLGTEMPLATEW) LockResource( hG );

					pCurrentTab = CreateDialogIndirect( hInst, dlg, hDlg, SysAreaFunc );
				}
				
				if ( iSel == 1 )
				{
					pVD = nullptr;

					HRSRC hRes = FindResource( hInst, MAKEINTRESOURCE( IDD_ISO_BOOTDESC ), RT_DIALOG );
					HGLOBAL hG = LoadResource( hInst, hRes );

					LPDLGTEMPLATEW dlg = (LPDLGTEMPLATEW) LockResource( hG );

					pCurrentTab = CreateDialogIndirect( hInst, dlg, hDlg, BootDescFunc );
				}
				
				if ( iSel == 2 )
				{
					pVD = pIVD;

					HRSRC hRes = FindResource( hInst, MAKEINTRESOURCE( IDD_ISO_VOLDESC ), RT_DIALOG );
					HGLOBAL hG = LoadResource( hInst, hRes );

					LPDLGTEMPLATEW dlg = (LPDLGTEMPLATEW) LockResource( hG );

					pCurrentTab = CreateDialogIndirect( hInst, dlg, hDlg, VDFunc );
				}
				
				if ( iSel == 3 )
				{
					pVD = pJVD;

					HRSRC hRes;
					
					if ( pJVD != nullptr )
					{
						hRes = FindResource( hInst, MAKEINTRESOURCE( IDD_ISO_VOLDESC ), RT_DIALOG );
					}
					else
					{
						hRes = FindResource( hInst, MAKEINTRESOURCE( IDD_ISO_NOJOLIET ), RT_DIALOG );
					}

					HGLOBAL hG = LoadResource( hInst, hRes );

					LPDLGTEMPLATEW dlg = (LPDLGTEMPLATEW) LockResource( hG );

					if ( pJVD != nullptr )
					{
						pCurrentTab = CreateDialogIndirect( hInst, dlg, hDlg, VDFunc );
					}
					else
					{
						pCurrentTab = CreateDialogIndirect( hInst, dlg, hDlg, NullFunc );
					}
				}

				// NONE OF THIS MAKES ANY SENSE WHATSOEVER. MICROSOFT YOU ARE ON ==DRUGS==
				DWORD dwDlgBase = GetDialogBaseUnits();
				int cxMargin = LOWORD(dwDlgBase) / 4; 
				int cyMargin = HIWORD(dwDlgBase) / 8;

				RECT r, tr;

				GetWindowRect( GetDlgItem( hDlg, IDC_VD_TABS ), &tr );

				MapWindowPoints( GetDesktopWindow(), hDlg, (LPPOINT) &tr, 2 );

				r = tr;

				TabCtrl_AdjustRect( GetDlgItem( hDlg, IDC_VD_TABS ), TRUE, &r );

				OffsetRect(&r, cxMargin - r.left, cyMargin - r.top); 

				TabCtrl_AdjustRect( GetDlgItem( hDlg, IDC_VD_TABS ), FALSE, &r );

				tr.left -= cxMargin; tr.top -= cyMargin;
				r.left  -= cxMargin; r.top  -= cyMargin;
				r.left += tr.left; r.right += tr.left;
				r.top  += tr.top; r.bottom += tr.top;

				TabCtrl_GetItemRect( GetDlgItem( hDlg, IDC_VD_TABS ), iSel, &tr );

				r.bottom -= ( tr.bottom - tr.top ) + ( cyMargin * 2 ) + 2;
				r.right  -= ( cxMargin * 2 ) + 4;
				r.top    += 2;

				SetWindowPos( pCurrentTab, NULL, r.left, r.top, (r.right - r.left), (r.bottom - r.top), NULL );

				ShowWindow( pCurrentTab, SW_SHOW );
			}

			return (INT_PTR) FALSE;
		}
	}

	return (INT_PTR)FALSE;
}

void ISO9660FileSystem::EditVolumeDescriptors( HWND hParentWnd )
{
	pEditSource = pSource;
	VDEFSID     = FSID;
	pEditFS     = Clone();

	ISOVolDesc TempISODesc = PriVolDesc; pIVD = &TempISODesc;
	ISOVolDesc TempJOLDesc = JolietDesc; pJVD = &TempJOLDesc;

	// Get to root
	while ( !pEditFS->IsRoot() ) { pEditFS->Parent(); }

	pCurrentTab = nullptr;

	if ( !HasJoliet )
	{
		pJVD = nullptr;
	}

	if ( DialogBox( hInst, MAKEINTRESOURCE( IDD_VD_EDIT ), hParentWnd, VDEditBox ) == IDOK )
	{
		bool WasUnwritable = false;

		if ( ( !Writable ) && ( ( pSource->Flags & DS_ReadOnly ) == 0 ) )
		{
			WasUnwritable = true;

			EnableWriting(); // Temporarily!
		}

		// We need to do some cleaning up of path tables here.
		// First look for path tables to remove:
		ISOSectorList sectors;

		DWORD PathTableSects = ( PriVolDesc.PathTableSize + ( PriVolDesc.SectorSize -1 ) ) / PriVolDesc.SectorSize;

		for ( DWORD i=0; i<PathTableSects; i++ )
		{
			ISOSector sector;

			// Note: There's always one Path Table.
			if ( ( PriVolDesc.LTableLocOpt != 0 ) && ( TempISODesc.LTableLocOpt == 0 ) )
			{
				sector.ID = PriVolDesc.LTableLocOpt + i;
				sectors.push_back( sector );
			}

			// Note: No Joliet with High Sierra

			if ( HasJoliet )
			{
				if ( ( JolietDesc.LTableLocOpt != 0 ) && ( TempJOLDesc.LTableLocOpt == 0 ) )
				{
					sector.ID = JolietDesc.LTableLocOpt + i;
					sectors.push_back( sector );
				}
			}

			if ( FSID == FSID_ISOHS )
			{
				if ( ( PriVolDesc.LTableLocOptA != 0 ) && ( TempISODesc.LTableLocOptA == 0 ) )
				{
					sector.ID = PriVolDesc.LTableLocOpt + i;
					sectors.push_back( sector );
				}

				if ( ( PriVolDesc.LTableLocOptB != 0 ) && ( TempISODesc.LTableLocOptB == 0 ) )
				{
					sector.ID = PriVolDesc.LTableLocOptB + i;
					sectors.push_back( sector );
				}
			}

			// Note: There's always one Path Table.
			if ( ( PriVolDesc.MTableLocOpt != 0 ) && ( TempISODesc.MTableLocOpt == 0 ) )
			{
				sector.ID = PriVolDesc.LTableLocOpt + i;
				sectors.push_back( sector );
			}

			// Note: No Joliet with High Sierra

			if ( HasJoliet )
			{
				if ( ( JolietDesc.LTableLocOpt != 0 ) && ( TempJOLDesc.LTableLocOpt == 0 ) )
				{
					sector.ID = JolietDesc.LTableLocOpt + i;
					sectors.push_back( sector );
				}
			}

			if ( FSID == FSID_ISOHS )
			{
				if ( ( PriVolDesc.LTableLocOptA != 0 ) && ( TempISODesc.LTableLocOptA == 0 ) )
				{
					sector.ID = PriVolDesc.LTableLocOpt + i;
					sectors.push_back( sector );
				}

				if ( ( PriVolDesc.LTableLocOptB != 0 ) && ( TempISODesc.LTableLocOptB == 0 ) )
				{
					sector.ID = PriVolDesc.LTableLocOptB + i;
					sectors.push_back( sector );
				}
			}
		}

		if ( sectors.size() > 0 )
		{
			PrepareISOSectors( sectors );

			ISOJob job;

			job.FSID       = FSID;
			job.IsoOp      = ISO_RESTRUCTURE_REM_SECTORS;
			job.pSource    = pSource;
			job.pVolDesc   = &PriVolDesc;
			job.Sectors    = sectors;
			job.SectorSize = PriVolDesc.SectorSize;

			PerformISOJob( &job );
		}

		// Now look for new path table entries.
		sectors.clear();

		// We'll need the insertion point (we could just shunt the sectors on the end, but that's just not cricket).
		DWORD StartTable = PriVolDesc.LTableLoc + PathTableSects;

		if ( ( PriVolDesc.LTableLocOpt != 0 ) && ( ( PriVolDesc.LTableLocOpt + PathTableSects ) > StartTable ) ) { StartTable = PriVolDesc.LTableLocOpt + PathTableSects; }
		if ( ( PriVolDesc.MTableLocOpt != 0 ) && ( ( PriVolDesc.MTableLocOpt + PathTableSects ) > StartTable ) ) { StartTable = PriVolDesc.MTableLocOpt + PathTableSects; }
		if ( ( PriVolDesc.MTableLoc    != 0 ) && ( ( PriVolDesc.MTableLoc    + PathTableSects ) > StartTable ) ) { StartTable = PriVolDesc.MTableLoc    + PathTableSects; }

		if ( FSID == FSID_ISOHS )
		{
			if ( ( PriVolDesc.LTableLocOptA != 0 ) && ( ( PriVolDesc.LTableLocOptA + PathTableSects ) > StartTable ) ) { StartTable = PriVolDesc.LTableLocOptA + PathTableSects; }
			if ( ( PriVolDesc.LTableLocOptB != 0 ) && ( ( PriVolDesc.LTableLocOptB + PathTableSects ) > StartTable ) ) { StartTable = PriVolDesc.LTableLocOptB + PathTableSects; }
			if ( ( PriVolDesc.MTableLocOptA != 0 ) && ( ( PriVolDesc.MTableLocOptA + PathTableSects ) > StartTable ) ) { StartTable = PriVolDesc.MTableLocOptA + PathTableSects; }
			if ( ( PriVolDesc.MTableLocOptB != 0 ) && ( ( PriVolDesc.MTableLocOptB + PathTableSects ) > StartTable ) ) { StartTable = PriVolDesc.MTableLocOptB + PathTableSects; }
		}

		for ( DWORD i=0; i<PathTableSects; i++ )
		{
			ISOSector sector;

			sector.ID = StartTable;
		
			// Always one path table!
		
			if ( ( TempISODesc.LTableLocOpt != 0 ) && ( PriVolDesc.LTableLocOpt == 0 ) )
			{
				sectors.push_back( sector );
			}

			if ( ( TempISODesc.MTableLocOpt != 0 ) && ( PriVolDesc.MTableLocOpt == 0 ) )
			{
				sectors.push_back( sector );
			}

			if ( HasJoliet )
			{
				if ( ( TempJOLDesc.LTableLocOpt != 0 ) && ( JolietDesc.LTableLocOpt == 0 ) )
				{
					sectors.push_back( sector );
				}

				if ( ( TempJOLDesc.MTableLocOpt != 0 ) && ( JolietDesc.MTableLocOpt == 0 ) )
				{
					sectors.push_back( sector );
				}
			}

			if ( FSID == FSID_ISOHS )
			{
				if ( ( TempISODesc.LTableLocOptA != 0 ) && ( PriVolDesc.LTableLocOptA == 0 ) )
				{
					sectors.push_back( sector );
				}

				if ( ( TempISODesc.MTableLocOptB != 0 ) && ( PriVolDesc.MTableLocOptB == 0 ) )
				{
					sectors.push_back( sector );
				}
			}
		}

		if ( sectors.size() > 0 )
		{
			PrepareISOSectors( sectors );

			ISOJob job;

			job.FSID       = FSID;
			job.IsoOp      = ISO_RESTRUCTURE_ADD_SECTORS;
			job.pSource    = pSource;
			job.pVolDesc   = &PriVolDesc;
			job.Sectors    = sectors;
			job.SectorSize = PriVolDesc.SectorSize;

			PerformISOJob( &job );
		}

		// Re-read the volume descriptors as they may have changed
		ReadVolumeDescriptors();

		// Copy the locations across
		if ( ( TempISODesc.LTableLoc     != 0 ) && ( TempISODesc.LTableLoc     != 0xFFFFFFFF ) ) { TempISODesc.LTableLoc     = PriVolDesc.LTableLoc;     }
		if ( ( TempISODesc.LTableLocOpt  != 0 ) && ( TempISODesc.LTableLocOpt  != 0xFFFFFFFF ) ) { TempISODesc.LTableLocOpt  = PriVolDesc.LTableLocOpt;  }
		if ( ( TempISODesc.LTableLocOptA != 0 ) && ( TempISODesc.LTableLocOptA != 0xFFFFFFFF ) ) { TempISODesc.LTableLocOptA = PriVolDesc.LTableLocOptA; }
		if ( ( TempISODesc.LTableLocOptB != 0 ) && ( TempISODesc.LTableLocOptB != 0xFFFFFFFF ) ) { TempISODesc.LTableLocOptB = PriVolDesc.LTableLocOptB; }
		if ( ( TempJOLDesc.LTableLoc     != 0 ) && ( TempJOLDesc.LTableLoc     != 0xFFFFFFFF ) ) { TempJOLDesc.LTableLoc     = JolietDesc.LTableLoc;     }
		if ( ( TempJOLDesc.LTableLocOpt  != 0 ) && ( TempJOLDesc.LTableLocOpt  != 0xFFFFFFFF ) ) { TempJOLDesc.LTableLocOpt  = JolietDesc.LTableLocOpt;  }

		if ( ( TempISODesc.MTableLoc     != 0 ) && ( TempISODesc.MTableLoc     != 0xFFFFFFFF ) ) { TempISODesc.LTableLoc     = PriVolDesc.LTableLoc;     }
		if ( ( TempISODesc.MTableLocOpt  != 0 ) && ( TempISODesc.MTableLocOpt  != 0xFFFFFFFF ) ) { TempISODesc.LTableLocOpt  = PriVolDesc.LTableLocOpt;  }
		if ( ( TempISODesc.MTableLocOptA != 0 ) && ( TempISODesc.MTableLocOptA != 0xFFFFFFFF ) ) { TempISODesc.LTableLocOptA = PriVolDesc.LTableLocOptA; }
		if ( ( TempISODesc.MTableLocOptB != 0 ) && ( TempISODesc.MTableLocOptB != 0xFFFFFFFF ) ) { TempISODesc.LTableLocOptB = PriVolDesc.LTableLocOptB; }
		if ( ( TempJOLDesc.MTableLoc     != 0 ) && ( TempJOLDesc.MTableLoc     != 0xFFFFFFFF ) ) { TempJOLDesc.LTableLoc     = JolietDesc.LTableLoc;     }
		if ( ( TempJOLDesc.MTableLocOpt  != 0 ) && ( TempJOLDesc.MTableLocOpt  != 0xFFFFFFFF ) ) { TempJOLDesc.LTableLocOpt  = JolietDesc.LTableLocOpt;  }

		memcpy( TempISODesc.DirectoryRecord, PriVolDesc.DirectoryRecord, 34 );
		memcpy( TempJOLDesc.DirectoryRecord, JolietDesc.DirectoryRecord, 34 );

		// Now write out any path tables.
		ISOPathTable *TempTable = new ISOPathTable( pSource, PriVolDesc.SectorSize );

		TempTable->ReadPathTable( JolietDesc.LTableLoc, JolietDesc.SectorSize, FSID, true );

		if ( TempISODesc.LTableLocOpt == 0xFFFFFFFF ) { TempTable->WritePathTable( StartTable, false, FSID, false ); TempISODesc.LTableLocOpt = StartTable; StartTable += PathTableSects; }
	
		if ( FSID == FSID_ISOHS )
		{
			if ( TempISODesc.LTableLocOptA == 0xFFFFFFFF  ) { TempTable->WritePathTable( StartTable, false, FSID, false ); TempISODesc.LTableLocOptA = StartTable; StartTable += PathTableSects; }
			if ( TempISODesc.LTableLocOptB == 0xFFFFFFFF  ) { TempTable->WritePathTable( StartTable, false, FSID, false ); TempISODesc.LTableLocOptB = StartTable; StartTable += PathTableSects; }
		}

		if ( TempISODesc.MTableLocOpt == 0xFFFFFFFF  ) { TempTable->WritePathTable( StartTable, true, FSID, false ); TempISODesc.MTableLocOpt = StartTable; StartTable += PathTableSects; }
	
		if ( FSID == FSID_ISOHS )
		{
			if ( TempISODesc.LTableLocOptA == 0xFFFFFFFF  ) { TempTable->WritePathTable( StartTable, true, FSID, false ); TempISODesc.LTableLocOptA = StartTable; StartTable += PathTableSects; }
			if ( TempISODesc.LTableLocOptB == 0xFFFFFFFF  ) { TempTable->WritePathTable( StartTable, true, FSID, false ); TempISODesc.LTableLocOptB = StartTable; StartTable += PathTableSects; }
		}

		delete TempTable;

		if ( HasJoliet )
		{
			ISOPathTable *TempTable = new ISOPathTable( pSource, PriVolDesc.SectorSize );

			TempTable->ReadPathTable( JolietDesc.LTableLoc, PriVolDesc.SectorSize, FSID );

			if ( TempJOLDesc.LTableLocOpt ) { TempTable->WritePathTable( StartTable, false, FSID, true ); TempJOLDesc.LTableLocOpt = StartTable; StartTable += PathTableSects; }
	
			if ( TempISODesc.MTableLocOpt ) { TempTable->WritePathTable( StartTable, true, FSID, true ); TempJOLDesc.MTableLocOpt = StartTable; StartTable += PathTableSects; }
	
			delete TempTable;
		}

		// Put this back now, otherwise we might make a disc writable with a Joliet structure on it.
		if ( WasUnwritable )
		{
			Writable = false;
		}

		WriteVolumeDescriptor( *pIVD, pIVD->SourceSector, FSID, false );

		if ( HasJoliet )
		{
			WriteVolumeDescriptor( *pJVD, pJVD->SourceSector, FSID, true );
		}
	}

	Init();

	delete pEditFS;
}

static DWORD RunningSector;
static DWORD DirDepth;

void ISO9660FileSystem::JolietProcessDirectory( DWORD DirSector, DWORD DirSize, DWORD ParentSector, DWORD ParentSize, ISOPathTable *pJolietTable )
{
	if ( DirDepth >= 8 ) { return; }

	DWORD fc = pDirectory->Files.size();

	std::vector<std::pair<DWORD,DWORD>> DirEntries;

	DWORD ThisParent = RunningSector;

	if ( IsRoot() )
	{
		RunningSector += DirSize;
	}

	for ( DWORD i=0; i<fc; i++ ) 
	{
		NativeFile *pFile = &pDirectory->Files[ i ];

		if ( pFile->Flags & FF_Directory )
		{
			DirDepth++;

			DWORD Extent = RunningSector;

			pJolietTable->AddDirectory( pFile->Filename, Extent, DirSector );

			ChangeDirectory( i );

			DWORD Size   = pISODir->ProjectedDirectorySize( true );

			DirEntries.push_back( std::make_pair<DWORD,DWORD>( Extent, Size ) );

			RunningSector += Size;

			JolietProcessDirectory( Extent, Size, DirSector, DirSize, pJolietTable );

			Parent();

			DirDepth--;
		}
		else
		{
			DirEntries.push_back( std::make_pair<DWORD,DWORD>( 0, 0 ) );
		}
	}

	for ( DWORD i=0; i<fc; i++ ) 
	{
		NativeFile *pFile = &pDirectory->Files[ i ];

		if ( pFile->Flags & FF_Directory )
		{
			// Make a new location for this.
			pFile->Attributes[ ISOATTR_START_EXTENT ] = DirEntries[ i ].first;
			pFile->Length                             = DirEntries[ i ].second * JolietDesc.SectorSize;
		}
	}

	// Write out the directory
	pISODir->WriteJDirectory( DirSector, ParentSector, ParentSize * JolietDesc.SectorSize );
}

void ISO9660FileSystem::AddJoliet()
{
	// We can take the existing path table and just write out a couple of new ones, along with 
	// a new Joliet descriptor.

	ISOSectorList sectors;

	ISOSector sector;

	sector.ID = PriVolDesc.SourceSector + 1;

	sectors.push_back( sector );

	DWORD TableSize = pPathTable->GetProjectedSize( true );

	DWORD TableSects = ( TableSize + ( PriVolDesc.SectorSize - 1 ) ) / PriVolDesc.SectorSize;

	DWORD TotalSects = TableSects * 2; // At least one L-table and one M-Table.

	if ( PriVolDesc.LTableLocOpt != 0 ) { TotalSects += TableSects; }
	if ( PriVolDesc.MTableLocOpt != 0 ) { TotalSects += TableSects; }

	DWORD TableSect = PriVolDesc.MTableLoc + TableSects;

	for ( DWORD i = 0; i < TotalSects; i ++ )
	{
		sector.ID = TableSect; sectors.push_back( sector );
	}

	// Make the space
	ISOJob job;

	PrepareISOSectors( sectors );

	job.FSID       = FSID;
	job.IsoOp      = ISO_RESTRUCTURE_ADD_SECTORS;
	job.pSource    = pSource;
	job.pVolDesc   = &PriVolDesc;
	job.Sectors    = sectors;
	job.SectorSize = PriVolDesc.SectorSize;

	PerformISOJob( &job );

	Init();

	// Recalculate this - it's probably moved
	TableSect = PriVolDesc.MTableLoc + TableSects;

	JolietDesc = PriVolDesc;

	ISOPathTable JolietTable( pSource, JolietDesc.SectorSize );

	JolietTable.BlankTable( PriVolDesc.VolSize );

	WLEDWORD( &JolietDesc.DirectoryRecord[ 2 ], PriVolDesc.VolSize );
	WBEDWORD( &JolietDesc.DirectoryRecord[ 6 ], PriVolDesc.VolSize );

	JolietDesc.SourceSector = PriVolDesc.SourceSector + 1;

	WriteVolumeDescriptor( JolietDesc, PriVolDesc.SourceSector + 1, FSID, true );

	// Now we just need to collate the directories
	DirDepth = 0;
	RunningSector = PriVolDesc.VolSize;

	JolietProcessDirectory(
		RunningSector,
		pISODir->ProjectedDirectorySize() * JolietDesc.SectorSize,
		RunningSector,
		pISODir->ProjectedDirectorySize() * JolietDesc.SectorSize,
		&JolietTable
	);

	PriVolDesc.VolSize = RunningSector;
	JolietDesc.VolSize = RunningSector;

	WriteVolumeDescriptor( PriVolDesc, PriVolDesc.SourceSector, FSID, false );

	JolietDesc.LTableLoc = PriVolDesc.MTableLoc + TableSects;
	JolietDesc.MTableLoc = JolietDesc.LTableLoc + TableSects;

	if ( PriVolDesc.LTableLocOpt != 0 ) { JolietDesc.LTableLocOpt = JolietDesc.LTableLoc    + TableSects; }
	if ( PriVolDesc.MTableLocOpt != 0 ) { JolietDesc.MTableLocOpt = JolietDesc.LTableLocOpt + TableSects; }

	JolietTable.WritePathTable( JolietDesc.LTableLoc, false, FSID, true );
	JolietTable.WritePathTable( JolietDesc.MTableLoc, true,  FSID, true );

	if ( JolietDesc.LTableLocOpt != 0 ) { JolietTable.WritePathTable( JolietDesc.LTableLocOpt, false, FSID, true ); }
	if ( JolietDesc.MTableLocOpt != 0 ) { JolietTable.WritePathTable( JolietDesc.MTableLocOpt, true,  FSID, true ); }

	WriteVolumeDescriptor( JolietDesc, JolietDesc.SourceSector, FSID, true );

	Writable = false;

	// Done
	Init();
}
