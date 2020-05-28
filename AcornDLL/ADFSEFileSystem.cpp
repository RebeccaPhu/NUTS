#include "StdAfx.h"
#include "ADFSEFileSystem.h"
#include "RISCOSIcons.h"
#include "../nuts/libfuncs.h"
#include "../NUTS/NestedImageSource.h"
#include "SpriteFile.h"
#include "Sprite.h"
#include "../NUTS/OffsetDataSource.h"

#include "AcornDLL.h"
#include "resource.h"

#include <time.h>
#include <CommCtrl.h>

#include <assert.h>
#include <sstream>
#include <iterator>

int ADFSEFileSystem::Init(void) {
	pEDirectory	= new ADFSEDirectory( pSource );
	pDirectory = (Directory *) pEDirectory;

	pFSMap  = new NewFSMap( pSource );

	pEDirectory->pMap = pFSMap;

	pFSMap->ReadFSMap();

	pEDirectory->DirSector = pFSMap->RootLoc;
	pEDirectory->SecSize   = pFSMap->SecSize;

	CommonUseResolvedIcons = UseResolvedIcons;

	pDirectory->ReadDirectory();

	ResolveAppIcons<ADFSEFileSystem>( this );

	return 0;
}

FSHint ADFSEFileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FSID;

	BYTE SectorBuf[4096];

	BYTE CheckByte;

	if ( ( FSID == FSID_ADFS_E ) || ( FSID == FSID_ADFS_EP ) )
	{
		/* Use ReadRaw because the sector size isn't known */
		pSource->ReadRaw( 0, 1024, SectorBuf );

		CheckByte = pFSMap->ZoneCheck( SectorBuf, 1024 );

		if ( CheckByte == SectorBuf[ 0 ] )
		{
			pSource->ReadRaw( 0x800, 256, SectorBuf );

			if ( ( rstrncmp( &SectorBuf[ 1 ], (BYTE *) "Nick", 4 ) ) && ( FSID == FSID_ADFS_E ) )
			{
				hint.Confidence = 25;

				return hint;
			}

			if ( ( rstrncmp( &SectorBuf[ 4 ], (BYTE *) "SBPr", 4 ) ) && ( FSID == FSID_ADFS_EP ) )
			{
				hint.Confidence = 25;

				return hint;
			}
		}

		// Sector 0 can be damaged, because there's a backup on Sector 1.
	
		pSource->ReadRaw( 1024, 1024, SectorBuf );

		CheckByte = pFSMap->ZoneCheck( SectorBuf, 1024 );

		if ( CheckByte == SectorBuf[ 0 ] )
		{
			pSource->ReadRaw( 0x800, 256, SectorBuf );

			if ( ( rstrncmp( &SectorBuf[ 1 ], (BYTE *) "Nick", 4 ) ) && ( FSID == FSID_ADFS_E ) )
			{
				hint.Confidence = 25;

				return hint;
			}

			if ( ( rstrncmp( &SectorBuf[ 4 ], (BYTE *) "SBPr", 4 ) ) && ( FSID == FSID_ADFS_EP ) )
			{
				hint.Confidence = 25;

				return hint;
			}
		}
	}
	
	/* Going to need to read the Boot Block and locate the map base. */
	pSource->ReadRaw( 0x0C00, 1024, SectorBuf );

	BYTE *pRecord   = &SectorBuf[ 0x1C0 ]; // Bit o' boot block.
	DWORD BPMB      = 1 << pRecord[ 0x05 ];
	WORD  ZoneSpare = * (WORD *) &pRecord[ 0x0A ];
	WORD  Zones     = pRecord[ 0x09 ] + ( pRecord[ 0x2A ] * 256 );

	BYTE  TypeByte  = pRecord[ 0x03 ];

	DWORD SecSize   = 1 << pRecord[ 0x00 ];
	DWORD SecsTrack = pRecord[ 0x01 ];

	if ( SecSize < 256 )
	{
		/* No ADFS disc is this small */
		hint.FSID       = FSID;
		hint.Confidence = 0;

		return hint;
	}

	if ( Zones < 2 )
	{
		/* We're beyond E format and into the multi-zone F/F+/G/G+ territory now,
		   so if Zones is less than 2, this ain't one o' them. */
		return hint;
	}

	BYTE BootCheck = pFSMap->BootBlockCheck( SectorBuf );

	if ( BootCheck != SectorBuf[ 0x1FF ] )
	{
		/* Boot block makes no sense - is this correct? */
		hint.FSID       = FSID;
		hint.Confidence = 0;

		return hint;
	}

	/* Calculate location of root zone */
	DWORD DR_Size = 60;
	DWORD ZZ      = DR_Size * 8;

	DWORD MapAddr = ((Zones / 2) * (8*SecSize-ZoneSpare)-ZZ)*BPMB;
	DWORD Bits    = 0;

	pSource->ReadRaw( MapAddr, SecSize, SectorBuf );

	pFSMap = new NewFSMap( pSource );

	pFSMap->ReadDiscRecord( &SectorBuf[ 4 ] );

	CheckByte = pFSMap->ZoneCheck( SectorBuf, SecSize );

	if ( CheckByte == SectorBuf[ 0 ] )
	{
		hint.Confidence = 25;

		/* So it looks like new-map, but what kind? Could be F,E+,F+,G or a hard disc */
		switch ( FSID )
		{
		case FSID_ADFS_HN:
		case FSID_ADFS_HP:
			if ( TypeByte == 0 ) { hint.Confidence += 5; }
			break;

		case FSID_ADFS_F:
		case FSID_ADFS_FP:
			if ( SecsTrack == 10 ) { hint.Confidence += 5; }
			break;

		case FSID_ADFS_G:
			if ( SecsTrack == 20 ) { hint.Confidence += 5; }
			break;
		}

		if ( ( FSID == FSID_ADFS_FP ) || ( FSID == FSID_ADFS_HP ) || ( FSID == FSID_ADFS_G ) )
		{
			if ( pFSMap->FormatVersion == 1 )
			{
				hint.Confidence += 5;
			}
			else
			{
				hint.Confidence = 5;
			}
		}
	}

	delete pFSMap;

	pFSMap = nullptr;
	
	return hint;
}


int ADFSEFileSystem::ChangeDirectory( DWORD FileID )
{
	if ( FileID > pDirectory->Files.size() )
	{
		return -1;
	}

	NativeFile *file = &pDirectory->Files[ FileID ];

	pEDirectory->DirSector = file->SSector;

	rstrncat(path, (BYTE *) ".",   512 );
	rstrncat(path, file->Filename, 512 );

	FreeAppIcons( pDirectory );

	pDirectory->ReadDirectory();

	ResolveAppIcons<ADFSEFileSystem>( this );

	return 0;
}


int	ADFSEFileSystem::Parent() {	
	if ( IsRoot() )
	{
		return -1;
	}

	pEDirectory->DirSector = pEDirectory->ParentSector;

	FreeAppIcons( pDirectory );

	pDirectory->ReadDirectory();

	ResolveAppIcons<ADFSEFileSystem>( this );

	BYTE *p = rstrrchr( path, (BYTE) '.', 512 );

	if (p)
		*p	= 0;

	return 0;
}

int	ADFSEFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	BYTE Sector[ 4096 ];

	DWORD fileSize = (DWORD) pFile->Length;

	FileFragments Frags = pFSMap->GetFileFragments( pFile->SSector );

	FileFragment_iter iFrag = Frags.begin();

	DWORD FragSectors = 0;
	DWORD FragSector  = 0;

	while ( iFrag != Frags.end() ) {
		FragSectors = iFrag->Length / pFSMap->SecSize;

		for ( FragSector = 0; FragSector < FragSectors; FragSector++ )
		{
			if( fileSize == 0 )
			{
				return 0;
			}

			pSource->ReadSector( iFrag->Sector + FragSector, Sector, pFSMap->SecSize );

			if ( fileSize > pFSMap->SecSize )
			{
				store.Write( Sector, pFSMap->SecSize );

				fileSize -= pFSMap->SecSize;
			}
			else
			{
				store.Write( Sector, fileSize );

				fileSize = 0;
			}
		}

		iFrag++;

		if ( fileSize == 0 )
		{
			return 0;
		}
	}

	return 0;
}

int ADFSEFileSystem::ReplaceFile(NativeFile *pFile, CTempFile &store)
{
	NativeFileIterator iFile;

	/* Delete the original file */
	NativeFile theFile = pDirectory->Files[ pFile->fileID ];

	DeleteFile( &theFile, FILEOP_DELETE_FILE );

	/* Write the new data */
	pFile->Length = store.Ext();

	WriteFile( pFile, store );

	/* Update the reference */
	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( pFile, &*iFile ) )
		{
			*pFile = *iFile;

			break;
		}
	}

	return 0;
}

int	ADFSEFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	if ( Override )
	{
		pFile = &OverrideFile;

		Override = false;
	}

	if ( ( pFile->EncodingID != ENCODING_ASCII ) && ( pFile->EncodingID != ENCODING_ACORN ) )
	{
		return FILEOP_NEEDS_ASCII;
	}

	if ( pDirectory->Files.size() >= 77 )
	{
		return NUTSError( 0x80, L"Directory Full" );
	}

	/* Check it doesn't already exist */
	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile ++ )
	{
		if ( rstrnicmp( pFile->Filename, iFile->Filename, 10 ) )
		{
			return FILEOP_EXISTS;
		}
	}

	DWORD SectorsRequired = pFile->Length / pFSMap->SecSize;

	if ( ( pFile->Length % pFSMap->SecSize ) != 0 )
		SectorsRequired++;

	TargetedFileFragments StorageFrags = FindSpace( pFile->Length, false );

	if ( StorageFrags.Frags.size() == 0 )
	{
		/* No space on disk */
		return NUTSError( 0x3A, L"Disc full" );
	}

	store.Seek( 0 );

	BYTE *SectorBuf = (BYTE *) malloc( pFSMap->SecSize );

	FileFragment_iter iFrag;

	QWORD FileToGo = store.Ext();

	for ( iFrag = StorageFrags.Frags.begin(); iFrag != StorageFrags.Frags.end(); )
	{
		DWORD SecNum    = iFrag->Sector;
		DWORD BytesToGo = iFrag->Length;

		if ( FileToGo == 0 )
		{
			iFrag = StorageFrags.Frags.end();

			continue;
		}

		while ( ( BytesToGo != 0 ) && ( FileToGo != 0 ) )
		{
			store.Read( SectorBuf, pFSMap->SecSize );

			DWORD BytesToWrite = pFSMap->SecSize;

			if ( BytesToWrite > FileToGo )
			{
				BytesToWrite = FileToGo;
			}

			pSource->WriteSector( SecNum, SectorBuf, pFSMap->SecSize );

			SecNum++;

			BytesToGo -= pFSMap->SecSize;

			FileToGo  -= BytesToWrite;
		}

		if ( FileToGo != 0 )
		{
			iFrag++;
		}
	}

	/* We're on */
	NativeFile	DestFile	= *pFile;

	DestFile.SeqNum   = 0;
	DestFile.SSector  = StorageFrags.FragID << 8;
	DestFile.SSector |= StorageFrags.SectorOffset & 0xFF;

	if ( pFile->FSFileType == FT_ACORN )
	{
		/* Preset the read and write attribtues, as DFS doesn't have them */
		DestFile.AttrRead  = 0xFFFFFFFF;
		DestFile.AttrWrite = 0xFFFFFFFF;
	}
	else if ( pFile->FSFileType != FT_ACORNX )
	{
		/* Preset EVERYTHING! */
		DestFile.AttrRead   = 0xFFFFFFFF;
		DestFile.AttrWrite  = 0xFFFFFFFF;
		DestFile.AttrLocked = 0x00000000;

		InterpretImportedType( &DestFile );
	}

	SetTimeStamp( &DestFile );

	DestFile.Flags &= ( 0xFFFFFFFF ^ FF_Extension );

	pDirectory->Files.push_back(DestFile);

	pDirectory->WriteDirectory();

	FreeAppIcons( pDirectory );

	int r = pDirectory->ReadDirectory();

	ResolveAppIcons<ADFSEFileSystem>( this );

	return r;
}

BYTE *ADFSEFileSystem::DescribeFile(DWORD FileIndex) {
	static BYTE status[ 128 ];

	NativeFile	*pFile	= &pDirectory->Files[FileIndex];

	DWORD Type = ( pFile->LoadAddr & 0x000FFF00 ) >> 8;

	std::string FileTypeName = RISCOSIcons::GetNameForType( Type );

	const char *pTypeName = FileTypeName.c_str();

	if ( pFile->Flags & FF_Directory )
	{
		rsprintf( status, "[%s%s%s%s] - Directory",
			(pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-"
		);
	}
	else
	{
		sprintf_s( (char *) status, 128, "[%s%s%s%s] %08X bytes, %s/%03X",
			(pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-",
			(DWORD) pFile->Length, pTypeName, Type
		);
	}
		
	return status;
}

BYTE *ADFSEFileSystem::GetStatusString( int FileIndex, int SelectedItems ) {
	static BYTE status[128];

	if ( SelectedItems == 0 )
	{
		rsprintf( status, "%d File System Objects", pDirectory->Files.size() );
	}
	else if ( SelectedItems > 1 )
	{
		rsprintf( status, "%d Items Selected", SelectedItems );
	}
	else 
	{
		NativeFile	*pFile	= &pDirectory->Files[FileIndex];

		DWORD Type = ( pFile->LoadAddr & 0x000FFF00 ) >> 8;

		std::string FileTypeName = RISCOSIcons::GetNameForType( Type );

		const char *pTypeName = FileTypeName.c_str();

		if ( pFile->Flags & FF_Directory )
		{
			rsprintf( status, "%s | [%s%s%s%s] - Directory",
				pFile->Filename, (pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-"
			);
		}
		else
		{
			rsprintf( status, "%s | [%s%s%s%s] - %0X bytes - %s/%03X",
				pFile->Filename, (pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-",
				(DWORD) pFile->Length, (char *) pTypeName, Type
			);
		}
	}
		
	return status;
}

BYTE *ADFSEFileSystem::GetTitleString( NativeFile *pFile )
{
	static BYTE ADFSPath[512];

	std::string sPath = "ADFS::" + std::string( (char *) pFSMap->DiscName ) + "." + std::string( (char *) path );

	if ( pFile != nullptr )
	{
		sPath += "." + std::string( (char *) pFile->Filename ) + " > ";
	}

	rstrncpy( ADFSPath, (BYTE *) sPath.c_str(), 511 );

	return ADFSPath;
}


int ADFSEFileSystem::ResolveAuxFileType( NativeFile *pSprite, NativeFile *pFile, SpriteFile &spriteFile )
{
	DWORD AuxTypeID  = 0x0000;
	DWORD MaskColour = GetSysColor( COLOR_WINDOW );

	for (BYTE i=0; i<3; i++)
	{
		AuxTypeID <<= 4;

		BYTE *pC = &pSprite->Filename[ 5 + i ];

		if ( ( *pC >= '0' ) && ( *pC <= '9' ) )
		{
			AuxTypeID |= *pC - '0';
		}
		else if ( ( *pC >= 'A' ) && ( *pC <= 'F' ) )
		{
			AuxTypeID |= ( *pC - 'A' ) + 10;
		}
		else if ( ( *pC >= 'a' ) && ( *pC <= 'f' ) )
		{
			AuxTypeID |= ( *pC - 'a' ) + 10;
		}
		else
		{
			return -1;
		}
	}

	CTempFile FileObj;

	spriteFile.ReadFile( pSprite->fileID, FileObj );

	Sprite sprite( FileObj );

	IconDef icon;

	sprite.GetNaturalBitmap( &icon.bmi, &icon.pImage, MaskColour );

	HBITMAP pAuxBitmap = CreateBitmap( icon.bmi.biWidth, icon.bmi.biHeight, icon.bmi.biPlanes, icon.bmi.biBitCount, icon.pImage );

	RISCOSIcons::AddIconMaps( 0x0, AuxTypeID, FT_Arbitrary, std::string( (char *) pSprite->Filename ), pAuxBitmap );

	return 0;
}

FileSystem *ADFSEFileSystem::FileFilesystem( DWORD FileID )
{
	if ( pDirectory->Files[ FileID ].RISCTYPE == 0xFF9 )
	{
		/* Sprite file. These are quite difficult to detect, so we'll offer a ready made FS here. */
		DataSource *pSource = FileDataSource( FileID );

		FileSystem *pSpriteFS = new SpriteFile( pSource );

		return pSpriteFS;
	}

	return nullptr;
}

int ADFSEFileSystem::Refresh( void )
{
	CommonUseResolvedIcons = UseResolvedIcons;

	FreeAppIcons( pDirectory );

	pDirectory->ReadDirectory();

	ResolveAppIcons<ADFSEFileSystem>( this );

	return 0;
}


WCHAR *ADFSEFileSystem::Identify( DWORD FileID )
{
	static WCHAR *BASICFile  = L"BASIC Program";
	static WCHAR *DrawFile   = L"Draw File";
	static WCHAR *ExecFile   = L"Exec (Spool)";
	static WCHAR *FontFile   = L"Font";
	static WCHAR *JPEGFile   = L"JPEG Image";
	static WCHAR *ModFile    = L"Module";
	static WCHAR *ObeyFile   = L"Obey (Script)";
	static WCHAR *SpriteFile = L"Sprite File";
	static WCHAR *UtilFile   = L"Utility";
	static WCHAR *AppFile    = L"Application";
	static WCHAR *DataFile   = L"Data File";
	static WCHAR *TextFile   = L"Text File";

	switch ( pDirectory->Files[ FileID ].RISCTYPE )
	{
	case 0xFFB:
		return BASICFile;
	case 0xAFF:
		return DrawFile;
	case 0xFFE:
		return ExecFile;
	case 0xFF7:
		return FontFile;
	case 0xC85:
		return JPEGFile;
	case 0xFFA:
		return ModFile;
	case 0xFEB:
		return ObeyFile;
	case 0xFF9:
		return SpriteFile;
	case 0xFFC:
		return UtilFile;
	case 0xA01:
		return AppFile;
	case 0xFFD:
		return DataFile;
	case 0xFFF:
		return TextFile;
	default:
		return FileSystem::Identify( FileID );
	}

	return FileSystem::Identify( FileID );
}


int ADFSEFileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	ResetEvent( hCancelFree );

	static FSSpace Map;

	pFSMap->ReadFSMap();

	Fragment_iter iFragment;
	DWORD         NumBlocks = 0;

	for ( iFragment = pFSMap->Fragments.begin(); iFragment != pFSMap->Fragments.end(); iFragment++ )
	{
		NumBlocks += iFragment->Length;
	}

	NumBlocks &= 0xFFFFFFFF;

	Map.Capacity  = NumBlocks;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = 0;

	NumBlocks /= pFSMap->SecSize;

	double BlockRatio = ( (double) NumBlocks / (double) TotalBlocks );

	if ( BlockRatio < 1.0 ) { BlockRatio = 1.0 ; }
	
	memset( pBlockMap, BlockAbsent, TotalBlocks );
	memset( pBlockMap, BlockFree, (DWORD) ( NumBlocks / BlockRatio ) );

	DWORD BlkNum;

	if ( pFSMap->Zones > 1 )
	{
		/* This has a Boot Block at sector 3 */
		BlkNum = (DWORD) ( (double) 3 / BlockRatio );
	
		pBlockMap[ BlkNum ] = (BYTE) BlockFixed;
	}

	DWORD LastUsed  = 0;
	DWORD Increment = Map.Capacity / 1000;

	if ( Increment < ( 200 * 1024 ) )
	{
		Increment = 200 * 1024;
	}

	SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

	for ( iFragment = pFSMap->Fragments.begin(); iFragment != pFSMap->Fragments.end(); iFragment++ )
	{
		if ( WaitForSingleObject( hCancelFree, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		if ( iFragment->FragID != 0U )
		{
			Map.UsedBytes += iFragment->Length;

			if ( WaitForSingleObject( hCancelFree, 0 ) == WAIT_OBJECT_0 )
			{
				return 0;
			}

			DWORD StartBlk = ( iFragment->FragOffset * pFSMap->BPMB ) / pFSMap->SecSize;
			DWORD NumBlks  = iFragment->Length / pFSMap->SecSize;

			for ( DWORD Blk = StartBlk; Blk != StartBlk + NumBlks; Blk++ )
			{
				BlkNum = (DWORD) ( (double) Blk / BlockRatio );

				assert( BlkNum < TotalBlocks );
					
				if ( ( pBlockMap[ BlkNum ] == BlockFree ) || ( pBlockMap[ BlkNum ] == BlockUsed ) )
				{
					switch ( iFragment->FragID )
					{
						case 0: /* Free space - ignore*/
							break;

						case 1: /* Hard error - unmovable */
						case 2: /* Root Directory */
							pBlockMap[ BlkNum ] = BlockFixed;
							break;

						default:
							if ( pBlockMap[ BlkNum ] == BlockFree )
							{
								pBlockMap[ BlkNum ] = BlockUsed;
							}
							break;
					}
				}
			}

			if ( ( Map.UsedBytes - LastUsed ) > Increment )
			{
				SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

				LastUsed = Map.UsedBytes;
			}
		}
	}

	SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );

	return 0;
}

AttrDescriptors ADFSEFileSystem::GetFSAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;
	
	Attrs.clear();

	AttrDesc Attr;

	/* Disc Title */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrString | AttrEnabled;
	Attr.Name  = L"Disc Title";

	Attr.MaxStringLength = 10;
	Attr.pStringVal      = rstrndup( pFSMap->DiscName, 10 );

	Attrs.push_back( Attr );

	/* Boot Option */
	/* File type */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect;
	Attr.Name  = L"Boot Option";

	Attr.StartingValue  = pFSMap->BootOption;

	AttrOption opt;

	opt.Name            = L"No Boot";
	opt.EquivalentValue = 0x00000000;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Load";
	opt.EquivalentValue = 0x00000001;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Run";
	opt.EquivalentValue = 0x00000002;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	opt.Name            = L"Exec";
	opt.EquivalentValue = 0x00000003;
	opt.Dangerous       = false;

	Attr.Options.push_back( opt );

	Attrs.push_back( Attr );

	return Attrs;
}

int ADFSEFileSystem::SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal )
{
	if ( PropID == 0 )
	{
		BBCStringCopy( (char *) pFSMap->DiscName, (char *) pNewVal, 10 );
	}

	if ( PropID == 1 )
	{
		pFSMap->BootOption = NewVal;
	}

	return pFSMap->WriteFSMap();
}

AttrDescriptors ADFSEFileSystem::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Indirect disc address. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile | AttrDir;
	Attr.Name  = L"Indirect address";
	Attrs.push_back( Attr );

	/* Locked */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"Locked";
	Attrs.push_back( Attr );

	/* Read */
	Attr.Index = 2;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"Read";
	Attrs.push_back( Attr );

	/* Write */
	Attr.Index = 3;
	Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile | AttrDir;
	Attr.Name  = L"Write";
	Attrs.push_back( Attr );

	/* Load address. Hex. */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Load address";
	Attrs.push_back( Attr );

	/* Exec address. Hex. */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;
	Attr.Name  = L"Execute address";
	Attrs.push_back( Attr );

	/* File Type. Hex. */
	Attr.Index = 7;
	Attr.Type  = AttrVisible | AttrEnabled | AttrCombo | AttrHex | AttrFile;
	Attr.Name  = L"File Type";

	static DWORD    FileTypes[ 13 ] = {
		0xFFB, 0xFFD, 0xAFF, 0xFFE, 0xFF7, 0xC85, 0xFFA, 0xFEB, 0xFF9, 0xFFF, 0xFFC
	};

	static std::wstring Names[ 13 ] = {
		L"BASIC Program", L"Data", L"Draw File", L"Exec (Spool)", L"Font", L"JPEG Image", L"Module", L"Obey (Script)", L"Sprite", L"Text File", L"Utility"
	};

	for ( BYTE i=0; i<13; i++ )
	{
		AttrOption opt;

		opt.Name            = Names[ i ];
		opt.EquivalentValue = FileTypes[ i ];
		opt.Dangerous       = false;

		Attr.Options.push_back( opt );
	}

	Attrs.push_back( Attr );

	/* Time Stamp */
	Attr.Index = 8;
	Attr.Type  = AttrVisible | AttrEnabled | AttrTime | AttrFile | AttrDir;
	Attr.Name  = L"Time Stamp";
	Attrs.push_back( Attr );

	return Attrs;
}

TargetedFileFragments ADFSEFileSystem::FindSpace( DWORD Length, bool ForDir )
{
	/* OK, big long explanation time.

	   New map formats (E/E+/F/F+/G) support fragmentation through a system similar to what Microsoft
	   termed "Allocation Units" (and previously referred to as "Clusters"). The problem with clusters
	   (or "Fragments" as Acorn calls them) is that they are wasteful with small files. An 800K E disc
	   for example uses 128-byte clusters, with a minimum size of 16 clusters, for a minimum allocation
	   of 2K. That means 16 files taking up 64 bytes each should take only 1K
	   of space, but with 2K effective clusters would take up 32K!

	   Acorn developed a clever, if complicated solution to this. The disc address used in each file
	   is actually in two parts - a fragment ID (bits 8 upwards), and a sector number (bits 0-7).
	   Any fragment used by at least one file in the current directory in which the entire fragment
	   is unused is game to have the remainder used by one or more files that will fit the remainder
	   of the fragment (that is to say, a file that requires multiple fragments may not start part
	   way through a fragment).

	   This even allows storage of small files in the fragment allocated to the directory itself, as
	   the fragment is very often bigger than the directory required to contain it.

	   This means that we have to use two allocation strategies:

	   1) Scan the current directory and build up a sector-in-fragment allocation map for every
	      fragment in use by all files (and the directory itself). Then scan these allocation maps
	      for a set of sectors that are free for use. The Free Space Map doesn't need modifying for
	      this, as the map only records whole fragments in use. Note that any file that occupies more
	      than one fragment is disqualified from this (or rather, it's fragment is) as it otherwise
	      invalidates the reading algorithm for reading sub-fragment files.

	   2) If this fails, we have to look to the space map. As stated before, a file that can't fit
	      using strategy 1 may not start midway through a fragment. So Find the first fragment in
	      the map that has an ID of 0 and is big enough to contain the file in terms of sectors
	      as a subset of fragment length in sectors. If there are no whole fragments big enough,
	      then the file can be stored in multiple fragments, starting with the largest (for read
	      efficiency). Here the fragments need their IDs changing to match each other. This fragment
	      ID needs to come from the NewFSMap class itself. If the map cannot provide then the result
	      is "Disc Full".

	   There are two notes on this whole process:

	   Fragment ID 2 is special, and must be handled with care, as the beginning of fragment 2 is the
	   free space map itself (or one of them on multizone discs). Fragment ID 1 refers to a hard error,
	   and absolutely must not be touched or relocated, or even contemplated.

	   Additional additional: Directories must always occupy a fragment at the beginning (except the root).
	   This is because otherwise the sub-fragment system can't determine what other things are in the
	   sub-fragment occupied by the newly created directory, and thus will probably overwrite something.

	   Additional additional additional: Big directories have their own issues. They are exstensible, so
	   we can't store files in the spare space. They are thus excluded from the calculations for sub-fragment
	   allocations. Note that extending the directory is taken care of in ADFSEDirectory::WritePDirectory()
	   which precalculates the data space required, then determines if the current fragment set is big enough,
	   asking NewFSMap for more sectors if required. This extensibility explains why Big root directories
	   are in 0x00000301 rather than the usual 0x0002xx of the New types.
	*/

	/* Step one: Build up maps of the directory and it's files in terms of sectors used in fragments */
	std::map< DWORD, BYTE *> FragMaps;
	std::map< DWORD, DWORD > FragSizes;

	DWORD SecLength = Length / pFSMap->SecSize;

	if ( Length % pFSMap->SecSize ) { SecLength++; }

	DWORD DirFragID = pEDirectory->DirSector >> 8;
	DWORD DirSector = pEDirectory->DirSector & 0xFF;

	DWORD NumSectors;

	/* Step 1a: The directory itself, but not for Big Directories! */
	if ( pFSMap->FormatVersion != 0x00000001 )
	{
		DWORD DirFrags = pFSMap->GetMatchingFragmentCount( DirFragID );
		DWORD DirSize  = pFSMap->GetSingleFragmentSize( DirFragID );

		if ( DirSector != 0 ) { DirSector--; }

		NumSectors = 0x800 / pFSMap->SecSize;

		if ( DirFrags == 1 )
		{
			FragMaps[ DirFragID ] = (BYTE *) malloc( DirSize );

			ZeroMemory( FragMaps[ DirFragID ], DirSize );

			for ( DWORD i=0; i<(DirSector + NumSectors ); i++ )
			{
				FragMaps[ DirFragID ][ i ] = 0xFF;
			}

			FragSizes[ DirFragID ] = DirSize;
		}
	}

	/* Step 1b: Files within the directory */
	NativeFileIterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		DWORD FileFragID = iFile->SSector >> 8;
		DWORD FileSector = iFile->SSector & 0xFF;

		DWORD FileFrags = pFSMap->GetMatchingFragmentCount( FileFragID );
		DWORD FileSize  = pFSMap->GetSingleFragmentSize( FileFragID );

		if ( FileSector != 0 ) { FileSector--; }

		NumSectors = iFile->Length / pFSMap->SecSize;

		if ( ( iFile->Length % pFSMap->SecSize ) != 0 ) { NumSectors++; }

		if ( iFile->Flags & FF_Directory )
		{
			NumSectors = 0x800 / pFSMap->SecSize;

			if ( ( 0x800 % pFSMap->SecSize ) != 0 ) { NumSectors++; }
		}

		if ( FileFrags == 1 )
		{
			if ( FragMaps.find( FileFragID ) == FragMaps.end() )
			{
				FragMaps[ FileFragID ] = (BYTE *) malloc( FileSize );

				ZeroMemory( FragMaps[ FileFragID ], FileSize );
			}

			for ( DWORD i = FileSector; i<( FileSector + NumSectors ); i++ )
			{
				FragMaps[ FileFragID ][ i ] = 0xFF;
			}

			FragSizes[ FileFragID ] = FileSize;
		}
	}

	/* Step 1c: Find a fragment with sufficient space */
	std::map< DWORD, DWORD >::iterator iSingleFrag;

	bool FoundSpace = false;
	DWORD SecLoc    = 0;
	DWORD FragLoc   = 0;

	for ( iSingleFrag = FragSizes.begin(); iSingleFrag != FragSizes.end(); iSingleFrag++ )
	{
		/* Discard any fragment that is clearly not big enough */
		if ( iSingleFrag->second < SecLength )
		{
			continue;
		}

		/* Step 1c-ii: See if there's a gap here that can fit the file */

		for ( DWORD i=0; i<=( iSingleFrag->second - SecLength ); i++ )
		{
			bool Contiguous = true;
			for ( DWORD n=i; n<(i+SecLength); n++ )
			{
				if ( FragMaps[ iSingleFrag->first ][ n ] == 0xFF )
				{
					Contiguous = false;
				}
			}

			if ( Contiguous )
			{
				FoundSpace  = true;
				SecLoc      = i;
				FragLoc     = iSingleFrag->first;

				break;
			}
		}
	}

	/* If we found a space, return it. But free the frag map */
	for ( iSingleFrag = FragSizes.begin(); iSingleFrag != FragSizes.end(); iSingleFrag++ )
	{
		free( FragMaps[ iSingleFrag->first ] );
	}

	if ( ( FoundSpace ) && ( !ForDir ) )
	{
		TargetedFileFragments Frags;

		FileFragment f;

		f.Length = SecLength * pFSMap->SecSize;
		
		/* Step 1d: Find the fragment in the free space map, and determine it's location */
		f.Sector = pFSMap->SectorForSingleFragment( FragLoc );

		f.Sector += SecLoc;

		Frags.FragID       = FragLoc;
		Frags.SectorOffset = SecLoc + 1;
		Frags.Frags.push_back( f );

		return Frags;
	}

	/* Step 2: No sub-fragments, so we must find a fragment in the map */
	TargetedFileFragments Frags = pFSMap->GetWriteFileFragments( SecLength, 0, false );

	return Frags;
}

int	ADFSEFileSystem::CreateDirectory( BYTE *Filename, bool EnterAfter ) {

	unsigned char DirectorySector[0x800];

	if ( pDirectory->Files.size() >= 77 )
	{
		return NUTSError( 0x80, L"Directory Full" );
	}

	/* This is allocated bigger than needed in order to store small files in the same fragment */
	TargetedFileFragments DirSpace = FindSpace( 0x1000, true );

	ADFSEDirectory *pNewDirectory = new ADFSEDirectory( pSource );

	pNewDirectory->DirSector    = ( ( DirSpace.FragID ) << 8 ) | ( DirSpace.SectorOffset & 0xFF );
	pNewDirectory->ParentSector = pEDirectory->DirSector;
	pNewDirectory->MasterSeq    = 0;
	pNewDirectory->pMap         = pFSMap;
	
	if ( pFSMap->FormatVersion == 0x00000001 )
	{
		pNewDirectory->BigDirName = rstrndup( Filename, 255 );
	}
	else
	{
		BBCStringCopy( (char *) pNewDirectory->DirName,  (char *) Filename, 10 );
		BBCStringCopy( (char *) pNewDirectory->DirTitle, (char *) Filename, 10 );
	}

	int Err = pNewDirectory->WriteDirectory();

	if ( Err != DS_SUCCESS )
	{
		delete pNewDirectory;

		return -1;
	}

	NativeFile	DirEnt;

	rstrncpy( DirEnt.Filename, Filename, 10 ); DirEnt.Filename[ 10 ] = 0;

	DirEnt.ExecAddr   = 0;
	DirEnt.LoadAddr   = 0;
	DirEnt.Length     = 0;
	DirEnt.Flags      = FF_Directory;
	DirEnt.AttrLocked = 0xFFFFFFFF;
	DirEnt.AttrRead   = 0xFFFFFFFF;
	DirEnt.AttrWrite  = 0xFFFFFFFF;
	DirEnt.SeqNum     = 0;
	DirEnt.SSector    = pNewDirectory->DirSector;

	pDirectory->Files.push_back(DirEnt);

	/* No longer need this */
	delete pNewDirectory;

	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	if ( EnterAfter )
	{
		pEDirectory->DirSector = DirEnt.SSector;

		rstrncat(path, (BYTE *) ".", 512 );
		rstrncat(path, Filename, 512 );
	}

	FreeAppIcons( pDirectory );

	int r = pDirectory->ReadDirectory();

	ResolveAppIcons<ADFSEFileSystem>( this );

	return r;
}

int ADFSEFileSystem::DeleteFile( NativeFile *pFile, int FileOp )
{
	if ( Override )
	{
		pFile = &OverrideFile;

		Override = false;
	}

	std::vector<NativeFile>::iterator iFile;

	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); )
	{
		bool DidErase = false;

		if ( FilenameCmp( pFile, &*iFile ) )
		{
			/* This is way easier than writing a file. Because of the two allocation strategies,
			   there is therefore two deletion strategies to match. If the fragment is an isolated
			   fragment (that is there is only one fragment with that fragment ID), then examine
			   the directory to see what other files are using it. If it's only one (i.e. this one)
			   then change the fragment ID to 0 (via NewFSMap) and write out the FSMap, otherwise
			   do nothing.

			   If on the other hand, the fragment count > 1, then this is a multi-fragment file spread
			   across the disk, and we just instruct NewFSMap to delete all of them by changing their
			   IDs to 0. We'll let NewFSMap collapse adjacent spaces before writing.

			   Finally, remove the directory entry and write to disk.
			*/

			if ( pFSMap->GetMatchingFragmentCount( iFile->SSector >> 8 ) == 1 )
			{
				/* Step one: Count the files using this fragment */
				DWORD FilesUsing = 0;

				NativeFileIterator iOther;

				for ( iOther = pDirectory->Files.begin(); iOther != pDirectory->Files.end(); iOther++ )
				{
					if ( ( iOther->SSector & 0xFFFFFF00 ) == ( iFile->SSector & 0xFFFFFF00 ) )
					{
						FilesUsing++;
					}
				}
			
				/* Must consider this directory itself */
				if ( ( pEDirectory->DirSector & 0xFFFFFF00 ) == ( iFile->SSector & 0xFFFFFF00 ) )
				{
					FilesUsing++;
				}

				if ( FilesUsing == 1 )
				{
					pFSMap->ReleaseFragment( iFile->SSector >> 8 );
				}

				/* If more than one file is using this fragment, leave it alone */
			}
			else
			{
				/* Multiple fragment file (or fragment doesn't somehow exist). Just delete them */
				pFSMap->ReleaseFragment( iFile->SSector >> 8 );
			}			

			DidErase = true;

			iFile = pDirectory->Files.erase( iFile );
		}

		if ( !DidErase )
		{
			iFile++;
		}
		else
		{
			if ( pDirectory->WriteDirectory() != DS_SUCCESS )
			{
				return -1;
			}

			iFile = pDirectory->Files.end();
		}
	}

	FreeAppIcons( pDirectory );

	if ( pDirectory->ReadDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	ResolveAppIcons<ADFSEFileSystem>( this );

	return FILEOP_SUCCESS;
}

ADFSEFileSystem *pSystem = nullptr;

INT_PTR CALLBACK FormatProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			pSystem = (ADFSEFileSystem *) lParam;

			pSystem->IncludeEmuHeader;

			::PostMessage( GetDlgItem( hwndDlg, IDC_SLIDER1 ), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) 0x00200006 );
			::PostMessage( GetDlgItem( hwndDlg, IDC_SLIDER2 ), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) 0x00150006 );

			switch ( pSystem->FSID )
			{
			case FSID_ADFS_E:
			case FSID_ADFS_EP:
				{
					::PostMessage(  GetDlgItem( hwndDlg, IDC_SLIDER1 ), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 0x0007 );
					::PostMessage(  GetDlgItem( hwndDlg, IDC_SLIDER2 ), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 0x000F );
					::SendMessageA( GetDlgItem( hwndDlg, IDC_BPMB ), WM_SETTEXT, 0, (LPARAM)  "128 bytes" );
					::SendMessageA( GetDlgItem( hwndDlg, IDC_IDLEN ), WM_SETTEXT, 0, (LPARAM) "15 bits" );

					pSystem->pFSMap->LogBPMB = 7;
					pSystem->pFSMap->BPMB    = 128;
					pSystem->pFSMap->IDLen   = 15;
				}
				break;

			case FSID_ADFS_F:
			case FSID_ADFS_FP:
				{
					::PostMessage(  GetDlgItem( hwndDlg, IDC_SLIDER1 ), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 0x0006 );
					::PostMessage(  GetDlgItem( hwndDlg, IDC_SLIDER2 ), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 0x000F );
					::SendMessageA( GetDlgItem( hwndDlg, IDC_BPMB ), WM_SETTEXT, 0, (LPARAM)  "64 bytes" );
					::SendMessageA( GetDlgItem( hwndDlg, IDC_IDLEN ), WM_SETTEXT, 0, (LPARAM) "15 bits" );

					pSystem->pFSMap->LogBPMB = 6;
					pSystem->pFSMap->BPMB    = 64;
					pSystem->pFSMap->IDLen   = 15;
				}
				break;

			case FSID_ADFS_HN:
			case FSID_ADFS_HP:
				{
					::PostMessage(  GetDlgItem( hwndDlg, IDC_SLIDER1 ), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 0x000A );
					::PostMessage(  GetDlgItem( hwndDlg, IDC_SLIDER2 ), TBM_SETPOS, (WPARAM) TRUE, (LPARAM) 0x000F );
					::SendMessageA( GetDlgItem( hwndDlg, IDC_BPMB ), WM_SETTEXT, 0, (LPARAM)  "1024 bytes" );
					::SendMessageA( GetDlgItem( hwndDlg, IDC_IDLEN ), WM_SETTEXT, 0, (LPARAM) "15 bits" );

					pSystem->pFSMap->LogBPMB = 0x0A;
					pSystem->pFSMap->BPMB    = 1024;
					pSystem->pFSMap->IDLen   = 15;

					::EnableWindow( GetDlgItem( hwndDlg, IDC_EMUHEADER ), TRUE );
				}
				break;
			}

			return TRUE;

		case WM_HSCROLL:
			{
				pSystem->pFSMap->LogBPMB  = ::SendMessage( GetDlgItem( hwndDlg, IDC_SLIDER1 ), TBM_GETPOS, 0 ,0 );
				pSystem->pFSMap->IDLen    = ::SendMessage( GetDlgItem( hwndDlg, IDC_SLIDER2 ), TBM_GETPOS, 0 ,0 );
				pSystem->pFSMap->BPMB     = 1 << pSystem->pFSMap->LogBPMB;

				BYTE TextBuf[ 128 ];

				rsprintf( TextBuf, "%d bytes", pSystem->pFSMap->BPMB );

				::SendMessageA( GetDlgItem( hwndDlg, IDC_BPMB ), WM_SETTEXT, 0, (LPARAM) TextBuf );

				rsprintf( TextBuf, "%d bits", pSystem->pFSMap->IDLen );

				::SendMessageA( GetDlgItem( hwndDlg, IDC_IDLEN ), WM_SETTEXT, 0, (LPARAM) TextBuf );
			}
			return FALSE;

		case WM_COMMAND:
			if ( wParam == IDOK )
			{
				/* Some sanity checks here. The BPMB and IDLen values must be able to reference the whole disk, and
				   identify each fragment at it's smallest size uniquely. */

				QWORD DiskSize  = pSystem->pSource->PhysicalDiskSize;
				      DiskSize /= pSystem->pFSMap->BPMB;

				/* DiskSize now contains the number of map bits required to represnt the disk */
				QWORD RequiredFrags = DiskSize / ( pSystem->pFSMap->IDLen + 1 );

				/* RequiredFrags is now the maximum number of fragments that need to be representable */
				DWORD MaxFragID = ( (1 << (pSystem->pFSMap->IDLen + 1 ) ) - 1 );

				DWORD IDsPerZone = ( ( 1 << (pSystem->pFSMap->LogSecSize + 3 ) ) - pSystem->pFSMap->ZoneSpare ) / ( pSystem->pFSMap->IDLen + 1 );

				DWORD Zones = 0;

				while ( ( Zones * ( IDsPerZone * ( pSystem->pFSMap->IDLen + 1 ) ) ) < DiskSize )
				{
					Zones++;
				}

				if ( ( RequiredFrags > ( MaxFragID + 3 ) ) || ( Zones > 240 ) )
				{
					/* Oh dear. This is not addressable. */
					MessageBox( hwndDlg,
						L"The Bytes Per Map Bit (BPMB) and ID Length values chosen do not allow the entire disk to be presented in the map.\n\nPlease select alternate values.",
						L"NUTS ADFS New Map FileSystem",
						MB_ICONEXCLAMATION | MB_OK
					);
				}
				else
				{
					EndDialog( hwndDlg, 0 );
				}
			}

			if ( wParam == IDC_EMUHEADER )
			{
				pSystem->IncludeEmuHeader = !pSystem->IncludeEmuHeader;

				::SendMessage( GetDlgItem( hwndDlg, IDC_EMUHEADER ), BM_SETCHECK, (pSystem->IncludeEmuHeader)?BST_CHECKED:BST_UNCHECKED, 0 );
			}

			return FALSE;
	}

	return FALSE;
}

int ADFSEFileSystem::Format_PreCheck( int FormatType, HWND hWnd )
{
	pFSMap = new NewFSMap( pSource );

	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_ADFSFORMAT ), hWnd, FormatProc, (LPARAM) this );

	/* If we don't want an emu header, we must strip the offset data source that the DLL front end stuck on */
	if ( !IncludeEmuHeader )
	{
		OffsetDataSource *pWrapper = reinterpret_cast<OffsetDataSource *>(pSource);

		pSource->Release();

		pSource = pWrapper->pSrc;
	}

	return 0;
}

int ADFSEFileSystem::Format_Process( FormatType FT, HWND hWnd )
{
	static WCHAR * const InitMsg  = L"Initialising";
	static WCHAR * const EraseMsg = L"Erasing";
	static WCHAR * const MapMsg   = L"Creating FS Map";
	static WCHAR * const RootMsg  = L"Creating Root Directory";
	static WCHAR * const DoneMsg  = L"Complete";

	PostMessage( hWnd, WM_FORMATPROGRESS, 0, (LPARAM) InitMsg );

	DWORD SecSize = 512;

	if ( ( FSID == FSID_ADFS_E ) || ( FSID == FSID_ADFS_F ) || ( FSID == FSID_ADFS_EP ) || ( FSID == FSID_ADFS_FP ) )
	{
		SecSize = 1024;
	}

	DWORD Sectors = pSource->PhysicalDiskSize / (DWORD) SecSize;

	/* This will hold two sectors later, for setting up the disc name for D format discs */
	BYTE SectorBuf[ 1024 ];

	if ( FT == FormatType_Full )
	{
		ZeroMemory( SectorBuf, 1024 );

		for ( DWORD Sector=0; Sector < Sectors; Sector++ )
		{
			if ( pSource->WriteSector( Sector, SectorBuf, SecSize ) != DS_SUCCESS )
			{
				return -1;
			}

			PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 1, 3, Sector, Sectors, false ), (LPARAM) EraseMsg );
		}
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 2, 3, 0, 1, false ), (LPARAM) MapMsg );

	pFSMap->ConfigureDisk( FSID );

	if ( pDirectory == nullptr )
	{
		pEDirectory = new ADFSEDirectory( pSource );
		pDirectory  = (Directory *) pEDirectory;

		pEDirectory->pMap = pFSMap;
	}
		
	if ( ( FSID == FSID_ADFS_E ) || ( FSID==FSID_ADFS_F ) || ( FSID == FSID_ADFS_EP ) || ( FSID==FSID_ADFS_FP ) || ( FSID==FSID_ADFS_G ) )
	{
		time_t t = time(NULL);
		struct tm *pT = localtime( &t );

		static const char * const days[8] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

		BYTE Title[ 20 ];

		rsprintf( Title, "%02d_%02d_%s", pT->tm_hour, pT->tm_min, days[ pT->tm_wday ] );

		rstrncpy( pEDirectory->DirTitle, (BYTE *) "$",   19 );
		rstrncpy( pFSMap->DiscName,               Title, 10 );
	}
	else
	{
		rstrncpy( pEDirectory->DirTitle, (BYTE *) "$",         19 );
		rstrncpy( pFSMap->DiscName,      (BYTE *) "HardDisc4", 10 );
	}

	if ( pFSMap->WriteFSMap() != DS_SUCCESS )
	{
		return -1;
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 3, 3, 0, 1, false ), (LPARAM) RootMsg );

	pEDirectory->DirSector    = pFSMap->RootLoc;
	pEDirectory->ParentSector = pEDirectory->DirSector;
	pEDirectory->MasterSeq    = 0;
	pEDirectory->BigDirName   = rstrndup( (BYTE *) "$", 4 );

	rstrncpy( (BYTE *) pEDirectory->DirName, (BYTE *) "$", 10 );

	pDirectory->Files.clear();
	
	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 3, 3, 1, 1, true ), (LPARAM) DoneMsg );

	return 0;
}

int ADFSEFileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	DWORD PreType = pDirectory->Files[ FileID ].RISCTYPE;

	for ( BYTE i=0; i<16; i++ )
	{
		pDirectory->Files[ FileID ].Attributes[ i ] = Changes->Attributes[ i ];
	}

	DWORD PostType = pDirectory->Files[ FileID ].RISCTYPE;

	if ( PostType != PreType )
	{
		/* The type was changed, update the load/exec stuff from it */
		InterpretNativeType(  &pDirectory->Files[ FileID ]  );
	}

	int r = pDirectory->WriteDirectory();
	{
		if ( r == DS_SUCCESS )
		{
			r =pDirectory->ReadDirectory();
		}
	}

	return 0;
}