#include "StdAfx.h"
#include "ADFSDirectory.h"
#include "ADFSFileSystem.h"
#include "../NUTS/NestedImageSource.h"
#include "SpriteFile.h"
#include "Sprite.h"
#include "RISCOSIcons.h"
#include "../NUTS/NUTSError.h"
#include "resource.h"

#include <time.h>
#include <sstream>
#include <iterator>

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
	if ( FileID > pDirectory->Files.size() )
	{
		return NUTSError( 0x3D, L"Invalid file index" );
	}

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	int	Sectors	= (pFile->Length & 0xffffff00) >> 8;

	if (pFile->Length & 0xFF)
		Sectors++;

	BYTE Sector[ 1024 ];

	DWORD offset   = 0;
	DWORD fileSize = (DWORD) pFile->Length;

	while (Sectors) {
		if ( pSource->ReadSector( TranslateSector( pFile->SSector + offset ), Sector, 256) != DS_SUCCESS )
		{
			return -1;
		}

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
	if ( Override )
	{
		pFile = &OverrideFile;

		Override = false;
	}

	if ( ( pFile->EncodingID != ENCODING_ASCII ) && ( pFile->EncodingID != ENCODING_ACORN ) )
	{
		return FILEOP_NEEDS_ASCII;
	}

	if ( UseDFormat )
	{
		if ( pDirectory->Files.size() >= 77 )
		{
			return NUTSError( 0x80, L"Directory Full" );
		}
	}
	else
	{
		if (pDirectory->Files.size() >= 47)
		{
			return NUTSError( 0x80, L"Directory Full" );
		}
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

	DWORD SectorsRequired = (pFile->Length & 0xFFFFFF00) >> 8;

	if (pFile->Length & 0xFF)
		SectorsRequired++;

	FreeSpace	space;

	space.OccupiedSectors = SectorsRequired;

	pFSMap->GetStartSector( space );

	BYTE buffer[ 1024 ];

	store.Seek( 0 );

	QWORD DataLeft = pFile->Length;
	int   SectNum  = 0;

	while (DataLeft) {
		store.Read( buffer, 256 );

		if ( pSource->WriteSector( TranslateSector( space.StartSector + SectNum ), buffer, 256) != DS_SUCCESS )
		{
			return -1;
		}

		if (DataLeft > 256)	
			DataLeft	-= 256;
		else
			DataLeft	= 0;

		SectNum++;
	}
	
	if ( pFSMap->OccupySpace(space) != DS_SUCCESS )
	{
		return -1;
	}

	NativeFile	DestFile	= *pFile;

	DestFile.SeqNum  = 0;
	DestFile.SSector = space.StartSector;

	if ( pFile->FSFileType == FT_ACORN )
	{
		/* Preset the read and write attribtues, as DFS doesn't have them */
		DestFile.AttrRead  = 0xFFFFFFFF;
		DestFile.AttrWrite = 0xFFFFFFFF;
		DestFile.AttrExec  = 0x00000000;
	}
	else if ( pFile->FSFileType != FT_ACORNX )
	{
		/* Preset EVERYTHING! */
		DestFile.AttrRead   = 0xFFFFFFFF;
		DestFile.AttrWrite  = 0xFFFFFFFF;
		DestFile.AttrLocked = 0x00000000;
		DestFile.AttrExec   = 0x00000000;

		if ( ( FSID == FSID_ADFS_HO ) || ( FSID == FSID_ADFS_D ) || ( FSID == FSID_ADFS_L2 ) )
		{
			InterpretImportedType( &DestFile );
		}
		else
		{
			DestFile.LoadAddr = 0xFFFFFFFF;
			DestFile.ExecAddr = 0xFFFFFFFF;
			DestFile.AttrExec = 0x00000000;
		}
	}

	if ( ( FSID == FSID_ADFS_HO ) || ( FSID == FSID_ADFS_D ) || ( FSID == FSID_ADFS_L2 ) )
	{
		SetTimeStamp( &DestFile );
	}

	DestFile.Flags &= ( 0xFFFFFFFF ^ FF_Extension );

	pDirectory->Files.push_back(DestFile);

	pDirectory->WriteDirectory();

	FreeAppIcons( pDirectory );

	int r = pDirectory->ReadDirectory();

	ResolveAppIcons<ADFSFileSystem>( this );

	return r;
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

	FreeAppIcons( pDirectory );

	int r = pDirectory->ReadDirectory();

	ResolveAppIcons<ADFSFileSystem>( this );

	return r;
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

	if ( ( FSID == FSID_ADFS_L2 ) || ( FSID == FSID_ADFS_D ) || ( FSID == FSID_ADFS_HO ) )
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

		if ( ( FSID != FSID_ADFS_L2 ) && ( FSID != FSID_ADFS_D ) && ( FSID != FSID_ADFS_HO ) )
		{
			rsprintf( status, "%s | [%s%s%s%s] - %0X bytes - Load: &%08X Exec: &%08X",
				pFile->Filename, (pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-",
				(DWORD) pFile->Length, pFile->LoadAddr, pFile->ExecAddr);
		
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

	FreeAppIcons( pDirectory );

	int r = pDirectory->ReadDirectory();

	BYTE *p = rstrrchr(path, (BYTE) '.', 512 );

	if (p)
	{
		*p = 0;
	}

	ResolveAppIcons<ADFSFileSystem>( this );

	return r;
}

int	ADFSFileSystem::CreateDirectory( BYTE *Filename, bool EnterAfter ) {

	unsigned char DirectorySector[0x800];

	if ( UseDFormat )
	{
		if ( pDirectory->Files.size() >= 77 )
		{
			return NUTSError( 0x80, L"Directory Full" );
		}
	}
	else
	{
		if (pDirectory->Files.size() >= 47)
		{
			return NUTSError( 0x80, L"Directory Full" );
		}
	}

	DirectorySector[0x000]	= 0;
	DirectorySector[0x001]	= 'H';
	DirectorySector[0x002]	= 'u';
	DirectorySector[0x003]	= 'g';
	DirectorySector[0x004]	= 'o';

	DirectorySector[0x005]	= 0;	// No directory entries

	if ( UseDFormat )
	{
		BBCStringCopy((char *) &DirectorySector[0x7F0], (char *) Filename, 10);
		BBCStringCopy((char *) &DirectorySector[0x7DD], (char *) Filename, 18);

		DWORD ItsParent = pADFSDirectory->GetSector();

		DirectorySector[0x7DA]	= (unsigned char)   ItsParent & 0xFF;
		DirectorySector[0x7DB]	= (unsigned char) ((ItsParent & 0xFF00) >> 8);
		DirectorySector[0x7DC]	= (unsigned char) ((ItsParent & 0xFF0000) >> 16);

		DirectorySector[0x7fa]	= 0;
		DirectorySector[0x7fb]	= 'H';
		DirectorySector[0x7fc]	= 'u';
		DirectorySector[0x7fd]	= 'g';
		DirectorySector[0x7fe]	= 'o';
		DirectorySector[0x7ff]	= 0; // TODO: This should be a checksum byte!
	}
	else
	{
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
		DirectorySector[0x4ff]	= 0;
	}

	FreeSpace space;

	space.OccupiedSectors = (UseDFormat)?8:5;

	pFSMap->GetStartSector(space);

	int Err = DS_SUCCESS;

	Err += pSource->WriteSector(TranslateSector( space.StartSector + 0 ), &DirectorySector[0x000], 256);
	Err += pSource->WriteSector(TranslateSector( space.StartSector + 1 ), &DirectorySector[0x100], 256);
	Err += pSource->WriteSector(TranslateSector( space.StartSector + 2 ), &DirectorySector[0x200], 256);
	Err += pSource->WriteSector(TranslateSector( space.StartSector + 3 ), &DirectorySector[0x300], 256);
	Err += pSource->WriteSector(TranslateSector( space.StartSector + 4 ), &DirectorySector[0x400], 256);

	if ( UseDFormat )
	{
		Err += pSource->WriteSector( space.StartSector + 5, &DirectorySector[ 0x500 ], 256 );
		Err += pSource->WriteSector( space.StartSector + 6, &DirectorySector[ 0x600 ], 256 );
		Err += pSource->WriteSector( space.StartSector + 7, &DirectorySector[ 0x700 ], 256 );
	}

	if ( Err != DS_SUCCESS )
	{
		return -1;
	}

	if ( pFSMap->OccupySpace(space) != DS_SUCCESS )
	{
		return -1;
	}

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

	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	if ( EnterAfter )
	{
		pADFSDirectory->SetSector( space.StartSector );

		rstrncat(path, (BYTE *) ".", 512 );
		rstrncat(path, Filename, 512 );
	}

	FreeAppIcons( pDirectory );

	int r = pDirectory->ReadDirectory();

	ResolveAppIcons<ADFSFileSystem>( this );

	return r;
}

FSHint ADFSFileSystem::Offer( BYTE *Extension )
{
	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FSID;

	BYTE SectorBuf[ 1024 ];

	pSource->ReadSector(2, SectorBuf, 256);

	if (memcmp(&SectorBuf[1], "Hugo", 4) == 0)
	{
		/* This is ADFS S/M/L */
		if ( pSource->ReadSector( 0, SectorBuf, 256 ) != DS_SUCCESS )
		{
			return hint;
		}

		DWORD Sectors = * (DWORD *) &SectorBuf[ 0xFC ];

		Sectors &= 0xFFFFFF;

		hint.FSID       = FSID;
		hint.Confidence = 20;

		if ( ( Sectors == 0x280 ) && ( FSID == FSID_ADFS_S )  ) { hint.Confidence = 30; }
		if ( ( Sectors == 0x500 ) && ( FSID == FSID_ADFS_M )  ) { hint.Confidence = 30; }
		if ( ( Sectors == 0xA00 ) && ( FSID == FSID_ADFS_L2 ) ) { hint.Confidence = 30; }
		if ( ( Sectors >  0xA00 ) && ( FSID == FSID_ADFS_H )  ) { hint.Confidence = 30; }
		if ( ( Sectors >  0xA00 ) && ( FSID == FSID_ADFS_H8 ) ) { hint.Confidence = 30; }

		if ( ( Extension ) && ( memcmp( Extension, "ADL", 3 ) == 0 ) && ( FSID == FSID_ADFS_L ) )
		{
			hint.Confidence = 30;
		}
	}

	if ( pSource->ReadSector( 1, SectorBuf, 1024 ) != DS_SUCCESS )
	{
		return hint;
	}

	if ( (memcmp(&SectorBuf[1], "Hugo", 4) == 0) && ( FSID == FSID_ADFS_D ) )
	{
		hint.Confidence = 30;
		hint.FSID       = FSID;
	}

	pSource->ReadSector( 1, SectorBuf, 1024 );

	if ( ( memcmp(&SectorBuf[1], "Nick", 4) == 0) && ( FSID == FSID_ADFS_D ) )
	{
		hint.Confidence = 30;
		hint.FSID       = FSID;
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
	ResetEvent( hCancelFree );

	BYTE Sector[ 1024 ];

	static FSSpace Map;

	if ( pSource->ReadSector( 0, Sector, 256 ) != DS_SUCCESS )
	{
		return -1;
	}

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
		if ( pFSMap->ReadFSMap() != DS_SUCCESS )
		{
			return -1;
		}

		for ( iMap = pFSMap->Spaces.begin(); iMap != pFSMap->Spaces.end(); iMap++ )
		{
			if ( WaitForSingleObject( hCancelFree, 10 ) == WAIT_OBJECT_0 )
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
	Attr.Name  = L"Write";
	Attrs.push_back( Attr );

	/* Execute - Not ADFS D/L2/HO */
	if ( ( FSID != FSID_ADFS_D ) && ( FSID != FSID_ADFS_L2 ) && ( FSID != FSID_ADFS_HO ) )
	{
		Attr.Index = 9;
		Attr.Type  = AttrVisible | AttrEnabled | AttrBool | AttrFile | AttrDanger;
		Attr.Name  = L"Execute Only";
		Attrs.push_back( Attr );
	}

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

	/* Sequence number. Hex. Not in D Format. */
	if ( !UseDFormat )
	{
		Attr.Index = 6;
		Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrFile  | AttrDir;
		Attr.Name  = L"Sequence #";
		Attrs.push_back( Attr );
	}

	if ( ( FSID == FSID_ADFS_L2 ) || ( FSID == FSID_ADFS_D ) || ( FSID == FSID_ADFS_HO ) )
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

		Attr.MaxStringLength = 10;
		Attr.pStringVal      = rstrndup( DiscName, 10 );

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

int ADFSFileSystem::SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal )
{
	if ( PropID == 0 )
	{
		BBCStringCopy( (char *) DiscName, (char *) pNewVal, 10 );

		for ( BYTE i=0; i<10; i++ )
		{
			if ( i & 1 )
			{
				pFSMap->DiscName1[ i >> 1 ] = DiscName[ i ];
			}
			else
			{
				pFSMap->DiscName0[ i >> 1 ] = DiscName[ i ];
			}
		}
	}

	if ( PropID == 1 )
	{
		pFSMap->BootOption = NewVal;
	}

	return pFSMap->WriteFSMap();
}

int ADFSFileSystem::Init(void) {
	pADFSDirectory	= new ADFSDirectory( pSource );
	pDirectory = (Directory *) pADFSDirectory;

	pADFSDirectory->FSID = FSID;

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

	if ( pFSMap->ReadFSMap() != DS_SUCCESS )
	{
		return -1;
	}

	FreeAppIcons( pDirectory );

	CommonUseResolvedIcons = UseResolvedIcons;

	if ( pDirectory->ReadDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	if ( UseDFormat )
	{
		BYTE Sectors[ 512 ];

		if ( pSource->ReadSector( 0, Sectors, 512 ) != DS_SUCCESS )
		{
			return -1;
		}

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

	ResolveAppIcons<ADFSFileSystem>( this );

	return 0;
}

int ADFSFileSystem::Refresh( void )
{
	CommonUseResolvedIcons = UseResolvedIcons;

	FreeAppIcons( pDirectory );

	if ( pDirectory->ReadDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	ResolveAppIcons<ADFSFileSystem>( this );

	return 0;
}

WCHAR *ADFSFileSystem::Identify( DWORD FileID )
{
	if ( ( FSID == FSID_ADFS_L2 ) || ( FSID == FSID_ADFS_D ) || ( FSID == FSID_ADFS_HO ) )
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

int ADFSFileSystem::Format_PreCheck( int FormatType, HWND hWnd )
{
	// Nothing extra to do
	return 0;
}

int ADFSFileSystem::Format_Process( FormatType FT, HWND hWnd )
{
	static WCHAR * const InitMsg  = L"Initialising";
	static WCHAR * const EraseMsg = L"Erasing";
	static WCHAR * const MapMsg   = L"Creating FS Map";
	static WCHAR * const RootMsg  = L"Creating Root Directory";
	static WCHAR * const DoneMsg  = L"Complete";

	PostMessage( hWnd, WM_FORMATPROGRESS, 0, (LPARAM) InitMsg );

	DWORD Sectors = pSource->PhysicalDiskSize / (DWORD) 256;

	/* This will hold two sectors later, for setting up the disc name for D format discs */
	BYTE SectorBuf[ 512 ];

	if ( FT == FormatType_Full )
	{
		ZeroMemory( SectorBuf, 256 );

		for ( DWORD Sector=0; Sector < Sectors; Sector++ )
		{
			if ( pSource->WriteSector( Sector, SectorBuf, 256 ) != DS_SUCCESS )
			{
				return -1;
			}

			PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 1, 4, Sector, Sectors, false ), (LPARAM) EraseMsg );
		}
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 2, 4, 0, 1, false ), (LPARAM) MapMsg );

	if( pFSMap == nullptr )
	{
		pFSMap = new OldFSMap( pSource );
	}

	pFSMap->Spaces.clear();

	FreeSpace space;

	space.StartSector =  (UseDFormat)?0x04:0x02; // D starts at sector 4, S/M/L at sector 2
	space.StartSector += (UseDFormat)?0x8:0x05; // D is 8 sectors long, S/M/L is 5 sectors
	space.Length      =  Sectors - space.StartSector;

	pFSMap->Spaces.push_back(space);

	pFSMap->NextEntry      = 1;
	pFSMap->DiscIdentifier = 0;
	pFSMap->BootOption     = 0;
	pFSMap->TotalSectors   = Sectors;

	if ( pFSMap->WriteFSMap() != DS_SUCCESS )
	{
		return -1;
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 3, 4, 0, 1, false ), (LPARAM) RootMsg );

	if ( pDirectory == nullptr )
	{
		pADFSDirectory = new ADFSDirectory( pSource );
		pDirectory     = (Directory *) pADFSDirectory;
	}
		
	pADFSDirectory->DirSector    = (UseDFormat)?0x4:0x02;
	pADFSDirectory->ParentSector = pADFSDirectory->DirSector;
	pADFSDirectory->MasterSeq    = 0;

	time_t t = time(NULL);
	struct tm *pT = localtime( &t );

	static const char * const days[8] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

	BYTE Title[ 20 ];

	rsprintf( Title, "%02d_%02d_%s", pT->tm_hour, pT->tm_min, days[ pT->tm_wday ] );

	rstrncpy( (BYTE *) pADFSDirectory->DirTitle,  Title, 19 );
	rstrncpy( (BYTE *) pADFSDirectory->DirString, (BYTE *) "$", 10 );

	if ( UseDFormat )
	{
		pADFSDirectory->SetDFormat();
	}

	pDirectory->Files.clear();
	
	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	if ( UseDFormat )
	{
		/* Fixup disc name */
		if ( pSource->ReadSector( 0, &SectorBuf[ 0x000 ], 256 ) != DS_SUCCESS ) { return -1; }
		if ( pSource->ReadSector( 1, &SectorBuf[ 0x100 ], 256 ) != DS_SUCCESS ) { return -1; }

		char NewDiscName[ 10 ];

		BBCStringCopy( NewDiscName, (char *) Title, 10 );

		for (BYTE i=0; i<9; i++ )
		{
			if ( i & 1 )
			{
				SectorBuf[ 0x1f6+ (i >> 1 )] = NewDiscName[ i ];
			}
			else
			{
				SectorBuf[ 0x0f7 + (i>> 1) ] = NewDiscName[ i ];
			}
		}

		if ( pSource->WriteSector( 0, &SectorBuf[ 0x000 ], 256 ) != DS_SUCCESS ) { return -1; }
		if ( pSource->WriteSector( 1, &SectorBuf[ 0x100 ], 256 ) != DS_SUCCESS ) { return -1; }
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 3, 4, 1, 1, true ), (LPARAM) DoneMsg );

	return 0;
}

int ADFSFileSystem::DeleteFile( NativeFile *pFile, int FileOp )
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
			DWORD FileSectors;
			
			if ( iFile->Flags & FF_Directory )
			{
				FileSectors = (UseDFormat)?0x08:0x05;
			}
			else
			{
				FileSectors = iFile->Length / 256;

				if ( iFile->Length % 256 )
				{
					FileSectors++;
				}
			}

			FreeSpace space;

			space.StartSector = iFile->SSector;
			space.Length      = FileSectors;
			
			if ( pFSMap->ReleaseSpace( space ) != DS_SUCCESS )
			{
				return -1;
			}

			iFile = pDirectory->Files.erase( iFile );

			DidErase = true;
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

			return FILEOP_SUCCESS;
		}
	}

	FreeAppIcons( pDirectory );

	if ( pDirectory->ReadDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	ResolveAppIcons<ADFSFileSystem>( this );

	return FILEOP_SUCCESS;
}

int ADFSFileSystem::RunTool( BYTE ToolNum, HWND ProgressWnd )
{
	if ( ToolNum == 0 )
	{
		/* Validate FS */

		std::string Log ="";

		::SendMessage( ProgressWnd, WM_FSTOOL_SETDESC, 0, (LPARAM) L"This tool will examine the disk or image shape, directory structure and space allocations, and make corrections where necessary" );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGLIMIT, 0, 100 );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 0, 3, 0, 0, false), 0 );

		/* Stage 1: Validate disk shape */
		/* Stage 2: Scan structure, fix directories */
		/* Stage 3: Validate Free Space Map */

		DWORD TotalSectors = pSource->PhysicalDiskSize / 256;

		::SendMessage( ProgressWnd, WM_FSTOOL_CURRENTOP, 0, (LPARAM) L"Checking disk/image shape information" );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 1, 3, 0, 1, false), 0 );

		if ( ( pFSMap->TotalSectors != TotalSectors ) || ( pSource->PhysicalDiskSize % 256 ) )
		{
			/* Eh up. Disk shape doesn't match the file. This *could* be due to a short file and thus valid.
			   Ask the user what they fancy. */
			BYTE ShapeQuestion[ 512 ];

			rsprintf( ShapeQuestion,
				"The disk shape appears to be incorrect.\n\n"
				"The size of the disk/image is %d sectors (or has a size that is not a whole number of sectors), "
				"but the disk data indicates it is %d sectors. "
				"If this is a disk image that has been truncated to save space, or the wrong image format has been detected, the recorded value may be correct.\n\n"
				"Do you want to correct this value?",
				TotalSectors, pFSMap->TotalSectors
			);

			if ( MessageBoxA( ProgressWnd, (char *) ShapeQuestion, "NUTS ADFS File System Validator", MB_ICONWARNING | MB_YESNO ) == IDYES )
			{
				pFSMap->TotalSectors = TotalSectors;

				pFSMap->WriteFSMap();

				Log += "Fixed disk shape information\n";
			}
		}

		::SendMessage( ProgressWnd, WM_FSTOOL_CURRENTOP, 0, (LPARAM) L"Examining and fixing directory structure" );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 2, 3, 0, 1, false), 0 );

		/* This is drastic. What we will do, is wipe the free space map, create a single free space representing
		   the whole image, then "occupy spaces" as we require. We can do this because the FS map is independent
		   of the directory structure, and thus does not need to be updated while we traverse it.
		*/

		DWORD FixedParentLinks = 0;
		DWORD FixedDirSigs     = 0;

		pFSMap->Spaces.clear();

		FreeSpace space;
		space.Length      = TotalSectors - ( (UseDFormat)?0x0A:0x07 );
		space.StartSector = (UseDFormat)?0x0A:0x07;

		pFSMap->Spaces.push_back( space );

		if ( pFSMap->WriteFSMap() != DS_SUCCESS )
		{
			return -1;
		}

		ValidateItems  = TotalSectors;
		ValidatedItems = (UseDFormat)?0x08:0x05;
		ValidateWnd    = ProgressWnd;

		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 2, 3, 0, (int) TotalSectors, false), 0 );

		if ( ValidateDirectory ( (UseDFormat)?0x04:0x02, (UseDFormat)?0x04:0x02, FixedParentLinks, FixedDirSigs ) != DS_SUCCESS )
		{
			return -1;
		}

		/* The free space map has now been reset */
		::SendMessage( ProgressWnd, WM_FSTOOL_CURRENTOP, 0, (LPARAM) L"Writing calculated free space map" );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 3, 3, 0, 1, false), 0 );

		if ( pFSMap->WriteFSMap() != DS_SUCCESS )
		{
			return -1;
		}

		::SendMessage( ProgressWnd, WM_FSTOOL_CURRENTOP, 0, (LPARAM) L"Finished" );
		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 3, 3, 1, 1, true), 0 );

		Log += "Fixed " + std::to_string( (QWORD) FixedDirSigs ) + " directory signatures\n";
		Log += "Fixed " + std::to_string( (QWORD) FixedDirSigs ) + " parent directory links\n";
		Log += "Fixed free space map";

		MessageBoxA( ProgressWnd, Log.c_str(), "NUTS ADFS File System Validation Tool Report", MB_OK );
	}

	return 0;
}

int ADFSFileSystem::ValidateDirectory( DWORD DirSector, DWORD ParentSector, DWORD &FixedParents, DWORD &FixedSigs )
{
	if ( WaitForSingleObject( hToolEvent, 0 ) == WAIT_OBJECT_0 )
	{
		return -1;
	}

	BYTE DirBytes[ 0x800 ];

	DWORD DirSectors = 5;

	if ( UseDFormat )
	{
		DirSectors = 8;
	}

	for ( DWORD s=0; s<DirSectors; s++ )
	{
		if ( pSource->ReadSector( TranslateSector( DirSector + s ), &DirBytes[ s * 0x100 ], 256 ) != DS_SUCCESS )
		{
			return -1;
		}
	}

	/* Check the signatures are intact */
	if ( ( !rstrncmp( &DirBytes[ 1 ], (BYTE *) "Hugo", 4 ) ) && ( !rstrncmp( &DirBytes[ 1 ], (BYTE *) "Nick", 4 ) ) )
	{
		FixedSigs++;

		rstrncpy( &DirBytes[ 1 ], (BYTE *) "Hugo", 4 );
	}

	DWORD SecondSig = (UseDFormat)?0x7fa:0x4fa;

	if ( !rstrncmp( &DirBytes[ 0 ], &DirBytes[ SecondSig ], 5 ) )
	{
		FixedSigs++;
	
		rstrncpy( &DirBytes[ 0 ], &DirBytes[ SecondSig ], 5 );
	}

	DWORD PSectorOffset = (UseDFormat)?0x7da:0x4d6;

	/* Check the parent sector matches - fix it if it doesn't */
	DWORD ApparentParentSector = * (DWORD *) &DirBytes[ PSectorOffset ]; ApparentParentSector &= 0xFFFFFF;

	if ( ParentSector != ApparentParentSector )
	{
		FixedParents++;

		DirBytes[ PSectorOffset + 0 ] = ( ParentSector & 0x0000FF ) >> 0;
		DirBytes[ PSectorOffset + 1 ] = ( ParentSector & 0x00FF00 ) >> 8;
		DirBytes[ PSectorOffset + 2 ] = ( ParentSector & 0xFF0000 ) >> 16;
	}

	/* Scan the directory items and remove the occupied bits */
	DWORD MaxPtr = 1227;
	DWORD ptr    = 5;

	if ( UseDFormat ) { MaxPtr = 3162; }

	bool IsDir;

	while ( ptr < MaxPtr ) {
		if ( DirBytes[ptr] == 0)
			break;

		IsDir = false;

		if ( !UseDFormat )
		{
			if (DirBytes[ptr+3] & 128)
			{
				IsDir = true;
			}
		}
		else
		{
			if ( DirBytes[ ptr + 0x19 ] & 8 )
			{
				IsDir = true;
			}
		}		

		DWORD Length       = * (DWORD *) &DirBytes[ptr + 18];
		DWORD StartSector  = * (DWORD *) &DirBytes[ptr + 22]; StartSector &= 0xFFFFFF;

		if ( IsDir )
		{
			Length = (UseDFormat)?0x0800:0x0500;
		}

		FreeSpace space;

		space.StartSector = StartSector;
		space.Length      = Length / 256;

		if ( Length % 256 )
		{
			space.Length++;
		}

		if ( UseDFormat )
		{
			/* D format requires objects align on a 1024-byte sector boundary == 4x256-sectors boundary */
			while (  space.Length % 4 )
			{
				space.Length++;
			}
		}

		space.OccupiedSectors = space.Length;

		pFSMap->ClaimSpace( space );

		ValidatedItems += space.OccupiedSectors;

		::PostMessage( ValidateWnd, WM_FSTOOL_PROGRESS, Percent( 2, 3, (int) ValidatedItems, (int) ValidateItems, false), 0 );

		ptr	+= 26;
	}

	/* Write the fixed directory back */
	for ( DWORD s=0; s<DirSectors; s++ )
	{
		if ( pSource->WriteSector( TranslateSector( DirSector + s ), &DirBytes[ s * 0x100 ], 256 ) != DS_SUCCESS )
		{
			return -1;
		}
	}

	/* Rescan the directory for subdirectories, and fix them */
	ptr = 5;

	while ( ptr < MaxPtr ) {
		if ( DirBytes[ptr] == 0)
			break;

		IsDir = false;

		if ( !UseDFormat )
		{
			if (DirBytes[ptr+3] & 128)
			{
				IsDir = true;
			}
		}
		else
		{
			if ( DirBytes[ ptr + 0x19 ] & 8 )
			{
				IsDir = true;
			}
		}		

		if ( IsDir )
		{
			DWORD StartSector  = * (DWORD *) &DirBytes[ptr + 22]; StartSector &= 0xFFFFFF;

			if ( ValidateDirectory( StartSector, DirSector, FixedParents, FixedSigs ) != DS_SUCCESS )
			{
				return -1;
			}
		}

		ptr	+= 26;
	}

	return 0;
}

FileSystem *ADFSFileSystem::FileFilesystem( DWORD FileID )
{
	if ( FileID > pDirectory->Files.size() )
	{
		NUTSError( 0x3E, L"Invalid file index" );

		return nullptr;
	}

	if ( pDirectory->Files[ FileID ].RISCTYPE == 0xFF9 )
	{
		DataSource *pSource = FileDataSource( FileID );

		if ( pSource == nullptr )
		{
			return nullptr;
		}

		FileSystem *pNewFS = new SpriteFile( pSource );

		pSource->Release();

		return pNewFS;
	}

	return nullptr;
}

int ADFSFileSystem::SetProps( DWORD FileID, NativeFile *Changes )
{
	DWORD PreType = pDirectory->Files[ FileID ].RISCTYPE;

	for ( BYTE i=0; i<16; i++ )
	{
		pDirectory->Files[ FileID ].Attributes[ i ] = Changes->Attributes[ i ];
	}

	/* If this is a RISC OS filesystem, translate the type to the load/exec addrs */
	if ( ( FSID == FSID_ADFS_D ) || ( FSID == FSID_ADFS_HO ) || ( FSID == FSID_ADFS_L2 ) )
	{
		DWORD PostType = pDirectory->Files[ FileID ].RISCTYPE;

		if ( PostType != PreType )
		{
			/* The type was changed, update the load/exec stuff from it */
			InterpretImportedType(  &pDirectory->Files[ FileID ]  );
		}
	}

	int r = pDirectory->WriteDirectory();
	{
		if ( r == DS_SUCCESS )
		{
			r =pDirectory->ReadDirectory();
		}
	}

	return r;
}