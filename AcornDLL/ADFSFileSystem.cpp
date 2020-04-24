#include "StdAfx.h"
#include "ADFSDirectory.h"
#include "ADFSFileSystem.h"
#include "../NUTS/NestedImageSource.h"
#include "SpriteFile.h"
#include "Sprite.h"
#include "RISCOSIcons.h"
#include "resource.h"

DWORD ADFSFileSystem::TranslateSector(DWORD InSector)
{
	if ( !UseLFormat )
	{
		return InSector;
	}

	DWORD Track  = InSector / 16;
	DWORD Sector = InSector % 16;

	DWORD OutSec;

	if ( Track >= 80 )
	{
		OutSec = ( (Track -  80) * 32) + 16 + Sector;
	}
	else
	{
		OutSec = ( Track * 32 ) + Sector;
	}

	return OutSec;
}

int	ADFSFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	int	Sectors	= (pFile->Length & 0xffffff00) >> 8;

	if (pFile->Length & 0xFF)
		Sectors++;

	BYTE Sector[ 1024 ];

	DWORD offset   = 0;
	DWORD fileSize = (DWORD) pFile->Length;

	while (Sectors) {
		pSource->ReadSector( TranslateSector( pFile->SSector + offset ), Sector, 256);

		if (fileSize > 256)
			store.Write( Sector, 256 );
		else
			store.Write( Sector, fileSize );

		offset++;
		Sectors--;
		fileSize -= 256;
	}

	return 0;
}

int	ADFSFileSystem::WriteFile(NativeFile *pFile, CTempFile &store)
{
	unsigned long	SectorsRequired	= (pFile->Length & 0xFFFFFF00) >> 8;

	if (pFile->Length & 0xFF)
		SectorsRequired++;

	FreeSpace	space;

	space.OccupiedSectors	= SectorsRequired;

	pFSMap->GetStartSector(space);

	BYTE buffer[ 1024 ];

	store.Seek( 0 );

	QWORD DataLeft = pFile->Length;
	int   SectNum  = 0;

	while (DataLeft) {
		store.Read( buffer, 256 );

		pSource->WriteSector( TranslateSector( space.StartSector + SectNum ), &buffer[SectNum * 256], 256);

		if (DataLeft > 256)	
			DataLeft	-= 256;
		else
			DataLeft	= 0;

		SectNum++;
	}
	
	pFSMap->OccupySpace(space);

	NativeFile	DestFile	= *pFile;

	DestFile.SeqNum  = 0;
	DestFile.SSector = space.StartSector;

	pDirectory->Files.push_back(DestFile);

	pDirectory->WriteDirectory();

	return 0;
}

int ADFSFileSystem::ChangeDirectory( DWORD FileID )
{
	if ( FileID > pDirectory->Files.size() )
	{
		return -1;
	}

	NativeFile *file = &pDirectory->Files[ FileID ];

	pADFSDirectory->SetSector( file->SSector );

	rstrncat(path, (BYTE *) ".", 512 );
	rstrncat(path, file->Filename, 512 );

	pDirectory->ReadDirectory();

	ResolveAppIcons();

	return 0;
}

bool ADFSFileSystem::IsRoot() {
	if ( !UseDFormat )
	{
		if ( pADFSDirectory->GetSector() == 2)
		{
			return true;
		}
	}
	else
	{
		if ( pADFSDirectory->GetSector() == 4 )
		{
			return true;
		}
	}

	return false;
}

BYTE *ADFSFileSystem::DescribeFile(DWORD FileIndex) {
	static BYTE status[ 128 ];

	NativeFile	*pFile	= &pDirectory->Files[FileIndex];

	if ( !pADFSDirectory->LooksRISCOSIsh )
	{
		sprintf_s( (char *) status, 128, "[%s%s%s%s] %06X bytes, &%08X/&%08X",
			(pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-",
			pFile->Length, pFile->LoadAddr, pFile->ExecAddr);

		return status;
	}
		
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

BYTE *ADFSFileSystem::GetStatusString( int FileIndex, int SelectedItems )
{
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
		NativeFile *pFile = &pDirectory->Files[FileIndex];

		if ( !pADFSDirectory->LooksRISCOSIsh )
		{
			rsprintf( status, "%s | [%s%s%s%s] - %0X bytes - Load: &%08X Exec: &%08X",
				pFile->Filename, (pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-",
				pFile->Length, pFile->LoadAddr, pFile->ExecAddr);
		
			return status;
		}

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

int	ADFSFileSystem::Parent() {
	
	if ( IsRoot() )
	{
		return -1;
	}

	DWORD ParentSector = pADFSDirectory->GetParentSector();

	pADFSDirectory->SetSector( ParentSector );

	pDirectory->ReadDirectory();

	BYTE *p = rstrrchr(path, (BYTE) '.', 512 );

	if (p)
		*p	= 0;

	ResolveAppIcons();

	return 0;
}

int	ADFSFileSystem::CreateDirectory( BYTE *Filename ) {

	unsigned char DirectorySector[0x500];

	if (pDirectory->Files.size() >= 47)
		return -1;

	DirectorySector[0x000]	= 0;
	DirectorySector[0x001]	= 'H';
	DirectorySector[0x002]	= 'u';
	DirectorySector[0x003]	= 'g';
	DirectorySector[0x004]	= 'o';

	DirectorySector[0x005]	= 0;	// No directory entries

	BBCStringCopy((char *) &DirectorySector[0x4cc], (char *) Filename, 10);
	BBCStringCopy((char *) &DirectorySector[0x4d9], (char *) Filename, 18);

	DWORD ItsParent = pADFSDirectory->GetSector();

	DirectorySector[0x4d6]	= (unsigned char)   ItsParent & 0xFF;
	DirectorySector[0x4d7]	= (unsigned char) ((ItsParent & 0xFF00) >> 8);
	DirectorySector[0x4d8]	= (unsigned char) ((ItsParent & 0xFF0000) >> 16);

	DirectorySector[0x4fa]	= 0;
	DirectorySector[0x4fb]	= 'H';
	DirectorySector[0x4fc]	= 'u';
	DirectorySector[0x4fd]	= 'g';
	DirectorySector[0x4fe]	= 'o';

	FreeSpace	space;

	space.OccupiedSectors	= 5;

	pFSMap->GetStartSector(space);

	pSource->WriteSector(TranslateSector( space.StartSector + 0 ), &DirectorySector[0x000], 256);
	pSource->WriteSector(TranslateSector( space.StartSector + 1 ), &DirectorySector[0x100], 256);
	pSource->WriteSector(TranslateSector( space.StartSector + 2 ), &DirectorySector[0x200], 256);
	pSource->WriteSector(TranslateSector( space.StartSector + 3 ), &DirectorySector[0x300], 256);
	pSource->WriteSector(TranslateSector( space.StartSector + 4 ), &DirectorySector[0x400], 256);

	pFSMap->OccupySpace(space);

	NativeFile	DirEnt;

	memcpy( DirEnt.Filename, Filename, 17 );

	DirEnt.ExecAddr   = 0;
	DirEnt.LoadAddr   = 0;
	DirEnt.Length     = 0;
	DirEnt.Flags      = FF_Directory;
	DirEnt.AttrLocked = 0xFFFFFFFF;
	DirEnt.AttrRead   = 0xFFFFFFFF;
	DirEnt.AttrWrite  = 0x00000000;
	DirEnt.SeqNum     = 0;
	DirEnt.SSector    = space.StartSector;

	pDirectory->Files.push_back(DirEnt);

	pDirectory->WriteDirectory();

	return 0;
}

FSHint ADFSFileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FS_Null;

	BYTE SectorBuf[ 1024 ];

	pSource->ReadSector(2, SectorBuf, 256);

	if (memcmp(&SectorBuf[1], "Hugo", 4) == 0)
	{
		hint.Confidence = 30;

		pSource->ReadSector( 0, SectorBuf, 256 );

		DWORD Sectors = * (DWORD *) &SectorBuf[ 0xFC ];

		Sectors &= 0xFFFFFF;

		if ( Sectors <= ( 16 * 40 ) )
			hint.FSID = FSID_ADFS_S;
		else if ( Sectors <= ( 16 * 80 ) )
			hint.FSID = FSID_ADFS_M;
		else if ( Sectors <= ( 16 * 160 ) )
			hint.FSID = FSID_ADFS_L;
		else
			hint.FSID = FSID_ADFS_H;

		if ( ( Extension ) && ( memcmp( Extension, "ADL", 3 ) == 0 ) )
		{
			hint.FSID = FSID_ADFS_L;
		}
	}

	pSource->ReadSector( 1, SectorBuf, 1024 );

	if (memcmp(&SectorBuf[1], "Hugo", 4) == 0)
	{
		hint.Confidence = 30;
		hint.FSID       = FSID_ADFS_D;
	}

	pSource->ReadSector( 1, SectorBuf, 1024 );

	if (memcmp(&SectorBuf[1], "Nick", 4) == 0)
	{
		hint.Confidence = 30;
		hint.FSID       = FSID_ADFS_D;
	}

	return hint;
}

BYTE *ADFSFileSystem::GetTitleString( NativeFile *pFile )
{
	static BYTE ADFSPath[512];

	std::string sPath = "ADFS::";
	
	if ( UseDFormat )
	{
		sPath += std::string( (char *) DiscName );
	}
	else
	{
		sPath += "0";
	}
	
	sPath += "." + std::string( (char *) path );

	if ( pFile != nullptr )
	{
		sPath += "." + std::string( (char *) pFile->Filename ) + " > ";
	}

	rstrncpy( ADFSPath, (BYTE *) sPath.c_str(), 511 );

	return ADFSPath;
}

int ADFSFileSystem::CalculateSpaceUsage( HWND hSpaceWnd, HWND hBlockWnd )
{
	BYTE Sector[ 1024 ];

	static FSSpace Map;

	pSource->ReadSector( 0, Sector, 256 );

	DWORD NumBlocks = * (DWORD *) &Sector[0x0fc];

	NumBlocks &= 0xFFFFFF;

	Map.Capacity  = NumBlocks * 256;
	Map.pBlockMap = pBlockMap;
	Map.UsedBytes = Map.Capacity;

	std::vector<FreeSpace>::iterator iMap;

	double BlockRatio = ( (double) NumBlocks / (double) TotalBlocks );

	if ( BlockRatio < 1.0 ) { BlockRatio = 1.0 ; }
	
	memset( pBlockMap, BlockAbsent, TotalBlocks );
	memset( pBlockMap, BlockUsed, (DWORD) ( NumBlocks / BlockRatio ) );

	DWORD BlkNum;

	for ( DWORD FixedBlk=0; FixedBlk != 7; FixedBlk++ )
	{
		BlkNum = (DWORD) ( (double) FixedBlk / BlockRatio );
	
		pBlockMap[ BlkNum ] = (BYTE) BlockFixed;
	}

	if ( pFSMap != nullptr )
	{
		pFSMap->ReadFSMap();

		for ( iMap = pFSMap->Spaces.begin(); iMap != pFSMap->Spaces.end(); iMap++ )
		{
			if ( iMap->Length != 0U )
			{
				Map.UsedBytes -= iMap->Length * 256;

				for ( DWORD Blk = iMap->StartSector; Blk != iMap->StartSector + iMap->Length; Blk++ )
				{
					BlkNum = (DWORD) ( (double) Blk / BlockRatio );

					if ( pBlockMap[ BlkNum ] != BlockFixed )
					{
						pBlockMap[ BlkNum ] = BlockFree;
					}
				}

				SendMessage( hSpaceWnd, WM_NOTIFY_FREE, (WPARAM) &Map, 0 );
			}
		}
	}

	return 0;
}

AttrDescriptors ADFSFileSystem::GetAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	/* Start sector. Hex, visible, disabled */
	Attr.Index = 0;
	Attr.Type  = AttrVisible | AttrNumeric | AttrHex | AttrFile | AttrDir;
	Attr.Name  = L"Start sector";
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
	Attr.Name  = L"Read";
	Attrs.push_back( Attr );

	/* Load address. Hex. */
	Attr.Index = 4;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;;
	Attr.Name  = L"Load address";
	Attrs.push_back( Attr );

	/* Exec address. Hex. */
	Attr.Index = 5;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrWarning | AttrFile;;
	Attr.Name  = L"Execute address";
	Attrs.push_back( Attr );

	/* Sequence number. Hex. */
	Attr.Index = 6;
	Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrFile  | AttrDir;
	Attr.Name  = L"Sequence #";
	Attrs.push_back( Attr );

	if ( pADFSDirectory->LooksRISCOSIsh )
	{
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
	}

	return Attrs;
}

AttrDescriptors ADFSFileSystem::GetFSAttributeDescriptions( void )
{
	static std::vector<AttrDesc> Attrs;

	Attrs.clear();

	AttrDesc Attr;

	if ( pADFSDirectory->UseDFormat )
	{
		/* Disc Title */
		Attr.Index = 0;
		Attr.Type  = AttrVisible | AttrString | AttrEnabled;
		Attr.Name  = L"Disc Title";

		Attr.MaxStringLength = 16;
		Attr.pStringVal      = rstrndup( DiscName, 16 );

		Attrs.push_back( Attr );
	}

	/* Boot Option */
	/* File type */
	Attr.Index = 1;
	Attr.Type  = AttrVisible | AttrEnabled | AttrSelect;
	Attr.Name  = L"Boot Option";

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

DataSource *ADFSFileSystem::FileDataSource( DWORD FileID )
{
	CTempFile FileObj;

	ReadFile( FileID, FileObj );

	FileObj.Keep();

	std::string sName = FileObj.Name();

	const char *pName = sName.c_str();

	return new NestedImageSource( this, &pDirectory->Files[ FileID ], UString( (char *) pName ) );
}

int ADFSFileSystem::ResolveAppIcons( void )
{
	if ( !UseResolvedIcons )
	{
		return 0;
	}

	DWORD MaskColour = GetSysColor( COLOR_WINDOW );

	NativeFileIterator iFile;

	for (iFile=pDirectory->Files.begin(); iFile!=pDirectory->Files.end(); iFile++)
	{
		if ( iFile->Flags & FF_Directory )
		{
			ADFSFileSystem TempFS( *this );

			TempFS.ChangeDirectory( iFile->fileID );

			NativeFileIterator iSpriteFile;

			for ( iSpriteFile = TempFS.pDirectory->Files.begin(); iSpriteFile != TempFS.pDirectory->Files.end(); iSpriteFile++)
			{
				if ( rstricmp( iSpriteFile->Filename, (BYTE *) "!Sprites" ) )
				{
					DataSource *pSpriteSource = TempFS.FileDataSource( iSpriteFile->fileID );

					SpriteFile spriteFile( pSpriteSource );

					spriteFile.Init();

					NativeFileIterator iSprite;

					for ( iSprite = spriteFile.pDirectory->Files.begin(); iSprite != spriteFile.pDirectory->Files.end(); iSprite++ )
					{
						if ( rstricmp( iSprite->Filename, iFile->Filename ) )
						{
							CTempFile FileObj;

							spriteFile.ReadFile( iSprite->fileID, FileObj );

							Sprite sprite( FileObj );

							IconDef icon;

							sprite.GetNaturalBitmap( &icon.bmi, &icon.pImage, MaskColour );

							icon.Aspect = sprite.SpriteAspect;

							pDirectory->ResolvedIcons[ iFile->fileID ] = icon;

							iFile->HasResolvedIcon = true;
						}
						/*
						if ( rstrnicmp( iSprite->Filename, (BYTE *) "file_", 5 ) )
						{
							ResolveAuxFileType( &*iSprite, &*iFile, spriteFile );
						}
						*/
					}
				}
			}
		}
	}

	return 0;
}

int ADFSFileSystem::Init(void) {
	pADFSDirectory	= new ADFSDirectory( pSource );
	pDirectory = (Directory *) pADFSDirectory;

	if ( UseLFormat )
	{
		pADFSDirectory->SetLFormat();
	}

	pADFSDirectory->SetSector( 2 );

	if ( UseDFormat )
	{
		pADFSDirectory->SetDFormat();
		pADFSDirectory->SetSector( 4 );
	}

	pFSMap  = new OldFSMap( pSource );

	pFSMap->ReadFSMap();
	pDirectory->ReadDirectory();

	if ( UseDFormat )
	{
		BYTE Sectors[ 512 ];

		pSource->ReadSector( 0, Sectors, 512 );

		ZeroMemory( DiscName, 11 );

		for (BYTE i=0; i<9; i++ )
		{
			if ( i & 1 )
			{
				DiscName[ i ] = Sectors[ 0x1f6+ (i >> 1 )];
			}
			else
			{
				DiscName[ i ] = Sectors[ 0x0f7 + (i>> 1) ];
			}
		}

		for (BYTE i=9; i<10; i-- )
		{
			if ( ( DiscName[i] != 0x20 ) && ( DiscName[i] != 0 ) ) { break; } else { DiscName[i] = 0; }
		}
	}

	ResolveAppIcons();

	return 0;
}

int ADFSFileSystem::Refresh( void )
{
	pDirectory->ReadDirectory();

	ResolveAppIcons();

	return 0;
}

WCHAR *ADFSFileSystem::Identify( DWORD FileID )
{
	if ( !pADFSDirectory->LooksRISCOSIsh )
	{
		return FileSystem::Identify( FileID );
	}

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


FSToolList ADFSFileSystem::GetToolsList( void )
{
	HMODULE hModule  = GetModuleHandle( L"AcornDLL" );

	FSToolList list;

	/* Repair Icon, Large Android Icons, Aha-Soft, CC Attribution 3.0 US */
	HBITMAP hRepair = LoadBitmap( hModule, MAKEINTRESOURCE( IDB_REPAIR ) );

	/* Moving and Packing 02 Brown Icon, Moving and Packing Icon Set, My Moving Reviews, www.mymovingreviews.com/move/webmasters/moving-company-icon-sets (Linkware) */
	HBITMAP hCompact = LoadBitmap( hModule, MAKEINTRESOURCE( IDB_COMPACT ) );

	FSTool ValidateTool = { hRepair, L"Validate", L"Scan the file system and fix any errors found." };
	FSTool CompactTool  = { hCompact, L"Compact", L"Remove gaps between file objects to create one large space at the end of the file system." };

	list.push_back( ValidateTool );
	list.push_back( CompactTool );

	return list;
}