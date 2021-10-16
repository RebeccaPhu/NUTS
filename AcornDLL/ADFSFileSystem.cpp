#include "StdAfx.h"

#include "ADFSDirectory.h"
#include "ADFSFileSystem.h"
#include "../NUTS/NestedImageSource.h"
#include "SpriteFile.h"
#include "Sprite.h"
#include "RISCOSIcons.h"
#include "../NUTS/NUTSError.h"
#include "BBCFunctions.h"
#include "resource.h"

#include "AcornDLL.h"

#include <time.h>
#include <sstream>
#include <iterator>

int	ADFSFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	if ( FileID > pDirectory->Files.size() )
	{
		return NUTSError( 0x3D, L"Invalid file index" );
	}

	NativeFile *pFile = &pDirectory->Files[ FileID ];

	int	Sectors	= pFile->Length / DSectorSize;

	if ( ( pFile->Length % DSectorSize ) != 0 )
	{
		Sectors++;
	}

	BYTE Sector[ 1024 ];

	DWORD SectorNum = 0;
	QWORD BytesToGo = pFile->Length;

	while (Sectors) {
		if ( ReadTranslatedSector( DSector( pFile->SSector ) + SectorNum, Sector, DSectorSize, pSource ) != DS_SUCCESS )
		{
			return -1;
		}

		QWORD BytesToRead = BytesToGo;

		if ( BytesToRead > DSectorSize )
		{
			BytesToRead = DSectorSize;
		}

		store.Write( Sector, BytesToRead );

		SectorNum++;
		Sectors--;

		BytesToGo -= BytesToRead;
	}

	return 0;
}

int ADFSFileSystem::ReplaceFile(NativeFile *pFile, CTempFile &store)
{
	NativeFileIterator iFile;

	/* Delete the original file */
	NativeFile theFile = pDirectory->Files[ pFile->fileID ];

	DeleteFile( theFile.fileID );

	/* Write the new data */
	pFile->Length = store.Ext();

	WriteFile( pFile, store );

	/* Update the reference */
	for ( iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( FilenameCmp( pFile, &*iFile ) )
		{
			*pFile = *iFile;
		}
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

	if ( ( pFile->EncodingID != ENCODING_ASCII ) && ( pFile->EncodingID != ENCODING_ACORN ) && ( pFile->EncodingID != ENCODING_RISCOS )  )
	{
		return FILEOP_NEEDS_ASCII;
	}

	/* Filter out spaces */
	BYTE NoChars[] = { (BYTE) ' ', 0 };

	BYTEString t = BYTEString( pFile->Filename.size() );

	rstrnecpy( t, pFile->Filename, pFile->Filename.length(), NoChars );

	pFile->Filename = t;

	if ( MYFSID==FSID_ADFS_D )
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
		if ( rstrnicmp( iFile->Filename, pFile->Filename, 10 ) )
		{
			if ( iFile->Flags & FF_Directory )
			{
				return FILEOP_ISDIR;
			}
			else
			{
				return FILEOP_EXISTS;
			}
		}
	}

	DWORD SectorsRequired = pFile->Length / DSectorSize;

	if ( ( pFile->Length % DSectorSize ) != 0 )
	{
		SectorsRequired++;
	}

	/* Account for the sector size discrepency */
	if ( MYFSID==FSID_ADFS_D )
	{
		SectorsRequired <<= 2;
	}

	FreeSpace	space;

	space.OccupiedSectors = SectorsRequired;

	pFSMap->GetStartSector( space );

	BYTE buffer[ 1024 ];

	store.Seek( 0 );

	QWORD DataLeft = pFile->Length;
	int   SectNum  = 0;

	while (DataLeft) {
		QWORD BytesWrite = DSectorSize;

		if ( DataLeft < DSectorSize )
		{
			BytesWrite = DataLeft;
		}

		store.Read( buffer, BytesWrite );

		if ( WriteTranslatedSector( DSector( space.StartSector ) + SectNum, buffer, DSectorSize, pSource ) )
		{
			return -1;
		}

		DataLeft -= BytesWrite;

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

		if ( ( MYFSID == FSID_ADFS_HO ) || ( MYFSID == FSID_ADFS_D ) || ( MYFSID == FSID_ADFS_L2 ) )
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

	if ( ( MYFSID == FSID_ADFS_HO ) || ( MYFSID == FSID_ADFS_D ) || ( MYFSID == FSID_ADFS_L2 ) )
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
	if ( ! (MYFSID==FSID_ADFS_D) )
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

	if ( ( MYFSID == FSID_ADFS_L2 ) || ( MYFSID == FSID_ADFS_D ) || ( MYFSID == FSID_ADFS_HO ) )
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

		if ( ( MYFSID != FSID_ADFS_L2 ) && ( MYFSID != FSID_ADFS_D ) && ( MYFSID != FSID_ADFS_HO ) )
		{
			rsprintf( status, "%s | [%s%s%s%s] - %0X bytes - Load: &%08X Exec: &%08X",
				(BYTE *) pFile->Filename, (pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-",
				(DWORD) pFile->Length, pFile->LoadAddr, pFile->ExecAddr);
		
			return status;
		}

		DWORD Type = ( pFile->LoadAddr & 0x000FFF00 ) >> 8;

		std::string FileTypeName = RISCOSIcons::GetNameForType( Type );

		const char *pTypeName = FileTypeName.c_str();

		if ( pFile->Flags & FF_Directory )
		{
			rsprintf( status, "%s | [%s%s%s%s] - Directory",
				(BYTE *) pFile->Filename, (pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-"
			);
		}
		else
		{
			rsprintf( status, "%s | [%s%s%s%s] - %0X bytes - %s/%03X",
				(BYTE *) pFile->Filename, (pFile->Flags & FF_Directory)?"D":"-", (pFile->AttrLocked)?"L":"-", (pFile->AttrRead)?"R":"-", (pFile->AttrWrite)?"W":"-",
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

int	ADFSFileSystem::CreateDirectory( NativeFile *pDir, DWORD CreateFlags ) {

	if ( ( pDir->EncodingID != ENCODING_ASCII ) && ( pDir->EncodingID != ENCODING_ACORN ) && ( pDir->EncodingID != ENCODING_RISCOS )  )
	{
		return FILEOP_NEEDS_ASCII;
	}

	NativeFile SourceDir = *pDir;

	/* Filter out spaces */
	BYTE NoChars[] = { (BYTE) ' ', 0 };

	BYTEString t = BYTEString( pDir->Filename.size() );

	rstrnecpy( t, pDir->Filename, pDir->Filename.length(), NoChars );

	SourceDir.Filename = t;

	for ( NativeFileIterator iFile = pDirectory->Files.begin(); iFile != pDirectory->Files.end(); iFile++ )
	{
		if ( rstrnicmp( iFile->Filename, SourceDir.Filename, 10 ) )
		{
			if ( ( CreateFlags & ( CDF_MERGE_DIR | CDF_RENAME_DIR ) ) == 0 )
			{
				if ( iFile->Flags & FF_Directory )
				{
					return FILEOP_EXISTS;
				}
				else
				{
					return FILEOP_ISFILE;
				}
			}
			else if ( CreateFlags & CDF_MERGE_DIR )
			{
				return ChangeDirectory( iFile->fileID );
			}
			else if ( CreateFlags & CDF_RENAME_DIR )
			{
				/* No old map FSes use big directories, so all filenames are max. 10 chars */
				if ( RenameIncomingDirectory( &SourceDir, pDirectory, false ) != NUTS_SUCCESS )
				{
					return -1;
				}
			}
		}
	}

	unsigned char DirectorySector[0x800];

	if ( MYFSID==FSID_ADFS_D )
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

	DirectorySector[0x005]	= 0;	// No directory entries

	if ( MYFSID==FSID_ADFS_D )
	{
		DirectorySector[0x000]	= 0;
		DirectorySector[0x001]	= 'N';
		DirectorySector[0x002]	= 'i';
		DirectorySector[0x003]	= 'c';
		DirectorySector[0x004]	= 'k';

		BBCStringCopy( &DirectorySector[0x7F0], SourceDir.Filename, 10);
		BBCStringCopy( &DirectorySector[0x7DD], SourceDir.Filename, 18);

		DWORD ItsParent = pADFSDirectory->GetSector();

		DirectorySector[0x7DA]	= (unsigned char)   ItsParent & 0xFF;
		DirectorySector[0x7DB]	= (unsigned char) ((ItsParent & 0xFF00) >> 8);
		DirectorySector[0x7DC]	= (unsigned char) ((ItsParent & 0xFF0000) >> 16);

		DirectorySector[0x7fa]	= 0;
		DirectorySector[0x7fb]	= 'N';
		DirectorySector[0x7fc]	= 'i';
		DirectorySector[0x7fd]	= 'c';
		DirectorySector[0x7fe]	= 'k';
		DirectorySector[0x7ff]	= 0; // TODO: This should be a checksum byte!
	}
	else
	{
		DirectorySector[0x000]	= 0;
		DirectorySector[0x001]	= 'H';
		DirectorySector[0x002]	= 'u';
		DirectorySector[0x003]	= 'g';
		DirectorySector[0x004]	= 'o';

		BBCStringCopy( &DirectorySector[0x4cc], SourceDir.Filename, 10);
		BBCStringCopy( &DirectorySector[0x4d9], SourceDir.Filename, 18);

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

	space.OccupiedSectors = (MYFSID==FSID_ADFS_D)?8:5;

	pFSMap->GetStartSector(space);

	int Err = DS_SUCCESS;

	if ( ! (MYFSID==FSID_ADFS_D) )
	{
		Err += WriteTranslatedSector( space.StartSector + 0, &DirectorySector[ 0x000 ], 256, pSource );
		Err += WriteTranslatedSector( space.StartSector + 1, &DirectorySector[ 0x100 ], 256, pSource );
		Err += WriteTranslatedSector( space.StartSector + 2, &DirectorySector[ 0x200 ], 256, pSource );
		Err += WriteTranslatedSector( space.StartSector + 3, &DirectorySector[ 0x300 ], 256, pSource );
		Err += WriteTranslatedSector( space.StartSector + 4, &DirectorySector[ 0x400 ], 256, pSource );
	}
	else
	{
		Err += WriteTranslatedSector( ( space.StartSector >> 2 ) + 0, &DirectorySector[ 0x000 ], 1024, pSource );
		Err += WriteTranslatedSector( ( space.StartSector >> 2 ) + 1, &DirectorySector[ 0x400 ], 1024, pSource );
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

	DirEnt.Filename = SourceDir.Filename;

	DirEnt.ExecAddr   = 0;
	DirEnt.LoadAddr   = 0;
	DirEnt.Length     = 0;
	DirEnt.Flags      = FF_Directory;
	DirEnt.AttrLocked = 0xFFFFFFFF;
	DirEnt.AttrRead   = 0x00000000;
	DirEnt.AttrWrite  = 0x00000000;
	DirEnt.AttrExec   = 0x00000000;
	DirEnt.SeqNum     = 0;
	DirEnt.SSector    = space.StartSector;

	if ( SourceDir.FSFileType == FT_ACORNX )
	{
		DirEnt.AttrLocked = SourceDir.AttrLocked;
		DirEnt.AttrRead   = SourceDir.AttrRead;
		DirEnt.AttrWrite  = SourceDir.AttrWrite;
		DirEnt.AttrExec   = SourceDir.AttrExec;

		/* On RISCOS formats, the load and exec addresses are used to hold a timestamp */
		if ( ( MYFSID == FSID_ADFS_D ) || ( MYFSID == FSID_ADFS_HO ) || ( MYFSID == FSID_ADFS_L2 ) )
		{
			DirEnt.ExecAddr = SourceDir.ExecAddr;
			DirEnt.LoadAddr = SourceDir.LoadAddr;
		}
	}

	pDirectory->Files.push_back(DirEnt);

	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	if ( CreateFlags & CDF_ENTER_AFTER )
	{
		pADFSDirectory->SetSector( space.StartSector );

		rstrncat(path, (BYTE *) ".", 512 );
		rstrncat(path, SourceDir.Filename, 512 );
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

	/* To work with floppies, the source needs the media shape. Make
	   some broad assumptions here, as we only need track 0. */
	SetShape();

	if ( pSource->ReadSectorCHS( 0, 0, 2, SectorBuf ) != DS_SUCCESS )
	{
		return hint;
	}

	if (memcmp(&SectorBuf[1], "Hugo", 4) == 0)
	{
		/* This is ADFS S/M/L */
		if ( pSource->ReadSectorCHS( 0, 0, 0, SectorBuf ) != DS_SUCCESS )
		{
			return hint;
		}

		DWORD Sectors = * (DWORD *) &SectorBuf[ 0xFC ];

		Sectors &= 0xFFFFFF;

		hint.FSID       = FSID;
		hint.Confidence = 20;

		if ( ( Sectors == 0x280 ) && ( MYFSID == FSID_ADFS_S )  ) { hint.Confidence = 30; }
		if ( ( Sectors == 0x500 ) && ( MYFSID == FSID_ADFS_M )  ) { hint.Confidence = 30; }
		if ( ( Sectors == 0xA00 ) && ( MYFSID == FSID_ADFS_L2 ) ) { hint.Confidence = 30; }
		if ( ( Sectors >  0xA00 ) && ( MYFSID == FSID_ADFS_H )  ) { hint.Confidence = 30; }
		if ( ( Sectors >  0xA00 ) && ( MYFSID == FSID_ADFS_H8 ) ) { hint.Confidence = 30; }

		if ( ( Extension ) && ( memcmp( Extension, "ADL", 3 ) == 0 ) && ( MYFSID == FSID_ADFS_L ) )
		{
			hint.Confidence = 30;
		}
	}

	if ( pSource->ReadSectorCHS( 0, 0, 1, SectorBuf ) != DS_SUCCESS )
	{
		return hint;
	}

	if ( (memcmp(&SectorBuf[1], "Hugo", 4) == 0) && ( MYFSID == FSID_ADFS_D ) )
	{
		hint.Confidence = 30;
		hint.FSID       = FSID;
	}

	/* Some D format disks have Nick as the identifier instead of Hugo */
	pSource->ReadSectorCHS( 0, 0, 1, SectorBuf );

	if ( ( memcmp(&SectorBuf[1], "Nick", 4) == 0) && ( MYFSID == FSID_ADFS_D ) )
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
	
	if ( MYFSID==FSID_ADFS_D )
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

	if ( pSource->ReadSectorCHS( 0, 0, 0, Sector ) != DS_SUCCESS )
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

	DWORD FixedBlks = 7;

	if ( MYFSID == FSID_ADFS_D )
	{
		FixedBlks = 12;
	}

	for ( DWORD FixedBlk=0; FixedBlk != FixedBlks; FixedBlk++ )
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
	if ( ( MYFSID != FSID_ADFS_D ) && ( MYFSID != FSID_ADFS_L2 ) && ( MYFSID != FSID_ADFS_HO ) )
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
	if ( ! (MYFSID==FSID_ADFS_D) )
	{
		Attr.Index = 6;
		Attr.Type  = AttrVisible | AttrEnabled | AttrNumeric | AttrHex | AttrFile  | AttrDir;
		Attr.Name  = L"Sequence #";
		Attrs.push_back( Attr );
	}

	if ( ( MYFSID == FSID_ADFS_L2 ) || ( MYFSID == FSID_ADFS_D ) || ( MYFSID == FSID_ADFS_HO ) )
	{
		/* File Type. Hex. */
		Attr.Index = 7;
		Attr.Type  = AttrVisible | AttrEnabled | AttrCombo | AttrHex | AttrFile;
		Attr.Name  = L"File Type";

		static DWORD    FileTypes[ ] = {
			0xFFB, 0xFFD, 0xAFF, 0xFFE, 0xFF7, 0xC85, 0xFFA, 0xFEB, 0xFF9, 0xFFF, 0xFFC
		};

		static std::wstring Names[ ] = {
			L"BASIC Program", L"Data", L"Draw File", L"Exec (Spool)", L"Font", L"JPEG Image", L"Module", L"Obey (Script)", L"Sprite", L"Text File", L"Utility"
		};

		for ( BYTE i=0; i<11; i++ )
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

	Attr.StartingValue = pFSMap->BootOption;

	Attrs.push_back( Attr );

	return Attrs;
}

int ADFSFileSystem::SetFSProp( DWORD PropID, DWORD NewVal, BYTE *pNewVal )
{
	if ( PropID == 0 )
	{
		BBCStringCopy( DiscName, pNewVal, 10 );

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

void ADFSFileSystem::SetShape(void)
{
	FloppyFormat = false;

	/* Set the disk shape according to the format */
	switch ( MYFSID )
	{
		case FSID_ADFS_S:
		case FSID_ADFS_M:
		case FSID_ADFS_L:
		case FSID_ADFS_D:
			{
				DiskShape shape;

				shape.Heads            = 2;
				shape.InterleavedHeads = false;
				shape.Sectors          = 16;
				shape.SectorSize       = 256;
				shape.Tracks           = 80;

				if ( MYFSID == FSID_ADFS_L )
				{
					shape.InterleavedHeads = true;
				}

				if ( MYFSID == FSID_ADFS_S )
				{
					shape.Tracks = 40;
					shape.Heads  = 1;
				}

				if ( MYFSID == FSID_ADFS_M )
				{
					shape.Heads = 1;
				}

				if ( MYFSID == FSID_ADFS_D )
				{
					shape.Sectors    = 5;
					shape.SectorSize = 1024;
				}

				pSource->SetDiskShape( shape );

				MediaShape = shape;

				if ( pADFSDirectory != nullptr )
				{
					pADFSDirectory->MediaShape = shape;
				}

				FloppyFormat = true;
			}
			break;

		case FSID_ADFS_H:
		case FSID_ADFS_H8:
			{
				FloppyFormat = false;
			}
			break;

		default:
			{
				FloppyFormat = false;
			}
			break;
	}

	if ( pADFSDirectory != nullptr )
	{
		pADFSDirectory->FloppyFormat = FloppyFormat;
	}

	if ( pFSMap != nullptr )
	{
		pFSMap->MediaShape   = MediaShape;
		pFSMap->FloppyFormat = FloppyFormat;
	}
}

int ADFSFileSystem::Init(void) {
	pADFSDirectory	= new ADFSDirectory( pSource );
	pDirectory = (Directory *) pADFSDirectory;

	pADFSDirectory->FSID = FSID;

	SetShape();

	if ( MYFSID==FSID_ADFS_L )
	{
		pADFSDirectory->SetLFormat();
	}

	pADFSDirectory->SetSector( 2 );

	if ( MYFSID==FSID_ADFS_D )
	{
		pADFSDirectory->SetDFormat();
		pADFSDirectory->SetSector( 4 );
	}

	if ( ( MYFSID==FSID_ADFS_H ) || ( MYFSID==FSID_ADFS_H8 ) )
	{
		TopicIcon = FT_HardImage;
	}
	else
	{
		TopicIcon = FT_DiskImage;
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

	if ( MYFSID==FSID_ADFS_D )
	{
		BYTE Sectors[ 1024 ];

		if ( pSource->ReadSectorCHS( 0, 0, 0, &Sectors[ 0x000 ] ) != DS_SUCCESS )
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

	pFSMap->FloppyFormat = FloppyFormat;
	pFSMap->UseDFormat   = MYFSID==FSID_ADFS_D;

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
	if ( ( MYFSID == FSID_ADFS_L2 ) || ( MYFSID == FSID_ADFS_D ) || ( MYFSID == FSID_ADFS_HO ) )
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
	FSToolList list;

	/* Repair Icon, Large Android Icons, Aha-Soft, CC Attribution 3.0 US */
	HBITMAP hRepair = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_REPAIR ) );

	/* Moving and Packing 02 Brown Icon, Moving and Packing Icon Set, My Moving Reviews, www.mymovingreviews.com/move/webmasters/moving-company-icon-sets (Linkware) */
	HBITMAP hCompact = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_COMPACT ) );

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

int ADFSFileSystem::Format_Process( DWORD FT, HWND hWnd )
{
	static WCHAR * const InitMsg  = L"Initialising";
	static WCHAR * const EraseMsg = L"Erasing";
	static WCHAR * const MapMsg   = L"Creating FS Map";
	static WCHAR * const RootMsg  = L"Creating Root Directory";
	static WCHAR * const DoneMsg  = L"Complete";

	PostMessage( hWnd, WM_FORMATPROGRESS, 0, (LPARAM) InitMsg );

	DWORD Sectors = pSource->PhysicalDiskSize / 256;

	/* This will hold two sectors later, for setting up the disc name for D format discs */
	BYTE SectorBuf[ 1024 ];

	if ( FT & FTF_Blank )
	{
		ZeroMemory( SectorBuf, 256 );

		for ( DWORD Sector=0; Sector < Sectors; Sector++ )
		{
			if ( WaitForSingleObject( hCancelFormat, 0 ) == WAIT_OBJECT_0 )
			{
				return 0;
			}

			if ( WriteTranslatedSector( Sector, SectorBuf, DSectorSize, pSource ) != DS_SUCCESS )
			{
				return -1;
			}

			PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 0, 3, Sector, Sectors, false ), (LPARAM) EraseMsg );
		}
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 1, 3, 0, 1, false ), (LPARAM) MapMsg );

	if( pFSMap == nullptr )
	{
		pFSMap = new OldFSMap( pSource );
	}

	pFSMap->Spaces.clear();

	FreeSpace space;

	space.StartSector =  (MYFSID==FSID_ADFS_D)?0x04:0x02; // D starts at sector 4, S/M/L at sector 2
	space.StartSector += (MYFSID==FSID_ADFS_D)?0x8:0x05; // D is 8 sectors long, S/M/L is 5 sectors
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

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 2, 3, 0, 1, false ), (LPARAM) RootMsg );

	if ( pDirectory == nullptr )
	{
		pADFSDirectory = new ADFSDirectory( pSource );
		pDirectory     = (Directory *) pADFSDirectory;
	}
		
	pADFSDirectory->DirSector    = (MYFSID==FSID_ADFS_D)?0x4:0x02;
	pADFSDirectory->ParentSector = pADFSDirectory->DirSector;
	pADFSDirectory->MasterSeq    = 0;

	time_t t = time(NULL);
	struct tm *pT = localtime( &t );

	static const char * const days[8] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

	BYTE Title[ 20 ];

	rsprintf( Title, "%02d_%02d_%s", pT->tm_hour, pT->tm_min, days[ pT->tm_wday ] );

	rstrncpy( (BYTE *) pADFSDirectory->DirTitle,  Title, 19 );
	rstrncpy( (BYTE *) pADFSDirectory->DirString, (BYTE *) "$", 10 );

	if ( MYFSID == FSID_ADFS_D )
	{
		pADFSDirectory->SetDFormat();
	}

	pDirectory->Files.clear();
	
	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		return -1;
	}

	if ( MYFSID==FSID_ADFS_D )
	{
		/* Fixup disc name */
		if ( ReadTranslatedSector( 0, SectorBuf, 1024, pSource ) != DS_SUCCESS ) { return -1; }

		BYTE NewDiscName[ 10 ];

		BBCStringCopy( NewDiscName, Title, 10 );

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

		if ( WriteTranslatedSector( 0, SectorBuf, 1024, pSource ) != DS_SUCCESS ) { return -1; }
	}

	PostMessage( hWnd, WM_FORMATPROGRESS, Percent( 3, 3, 1, 1, true ), (LPARAM) DoneMsg );

	return 0;
}

int ADFSFileSystem::DeleteFile( DWORD FileID )
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	DWORD FileSectors;
			
	if ( pFile->Flags & FF_Directory )
	{
		FileSectors = (MYFSID==FSID_ADFS_D)?0x08:0x05;
	}
	else
	{
		FileSectors = pFile->Length / DSectorSize;

		if ( pFile->Length % DSectorSize )
		{
			FileSectors++;
		}

		if ( MYFSID == FSID_ADFS_D )
		{
			/* Must round this up to whole sectors */
			FileSectors <<= 2;
		}
	}

	FreeSpace space;

	space.StartSector = pFile->SSector;
	space.Length      = FileSectors;

	if ( pFSMap->ReleaseSpace( space ) != DS_SUCCESS )
	{
		return -1;
	}

	(void) pDirectory->Files.erase( pDirectory->Files.begin() + FileID );

	if ( pDirectory->WriteDirectory() != DS_SUCCESS )
	{
		return -1;
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
	if ( ToolNum == 1 )
	{
		ValidateWnd = ProgressWnd;

		return CompactImage();
	}

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

		if ( ( pFSMap->TotalSectors != TotalSectors ) || ( pSource->PhysicalDiskSize % DSectorSize ) )
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
				TotalSectors, DSector( pFSMap->TotalSectors )
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
		space.Length      = TotalSectors - ( ( MYFSID == FSID_ADFS_D )?0x0C:0x07 );
		space.StartSector = ( MYFSID == FSID_ADFS_D )?0x0C:0x07;

		pFSMap->Spaces.push_back( space );

		if ( pFSMap->WriteFSMap() != DS_SUCCESS )
		{
			return -1;
		}

		ValidateItems  = TotalSectors;
		ValidatedItems = (MYFSID==FSID_ADFS_D)?0x08:0x05;
		ValidateWnd    = ProgressWnd;

		::SendMessage( ProgressWnd, WM_FSTOOL_PROGRESS, Percent( 2, 3, 0, (int) TotalSectors, false), 0 );

		if ( ValidateDirectory ( (MYFSID==FSID_ADFS_D)?0x04:0x02, (MYFSID==FSID_ADFS_D)?0x04:0x02, FixedParentLinks, FixedDirSigs ) != DS_SUCCESS )
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

	if ( MYFSID == FSID_ADFS_D )
	{
		DirSectors = 2;
	}

	for ( DWORD s=0; s<DirSectors; s++ )
	{
		if ( ReadTranslatedSector( DSector( DirSector + s ), &DirBytes[ s * DSectorSize ], DSectorSize, pSource ) != DS_SUCCESS )
		{
			return -1;
		}
	}

	/* Check the signatures are intact */
	if ( ( !rstrncmp( &DirBytes[ 1 ], (BYTE *) "Hugo", 4 ) ) && ( !rstrncmp( &DirBytes[ 1 ], (BYTE *) "Nick", 4 ) ) )
	{
		FixedSigs++;

		if ( MYFSID == FSID_ADFS_D )
		{
			rstrncpy( &DirBytes[ 1 ], (BYTE *) "Nick", 4 );
		}
		else
		{
			rstrncpy( &DirBytes[ 1 ], (BYTE *) "Hugo", 4 );
		}
	}

	DWORD SecondSig = (MYFSID == FSID_ADFS_D)?0x7fa:0x4fa;

	if ( !rstrncmp( &DirBytes[ 0 ], &DirBytes[ SecondSig ], 5 ) )
	{
		FixedSigs++;
	
		rstrncpy( &DirBytes[ 0 ], &DirBytes[ SecondSig ], 5 );
	}

	DWORD PSectorOffset = (MYFSID == FSID_ADFS_D)?0x7da:0x4d6;

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

	if ( MYFSID==FSID_ADFS_D ) { MaxPtr = 3162; }

	bool IsDir;

	while ( ptr < MaxPtr ) {
		if ( DirBytes[ptr] == 0)
			break;

		IsDir = false;

		if ( ! ( MYFSID==FSID_ADFS_D ) )
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
			Length = (MYFSID==FSID_ADFS_D)?0x0800:0x0500;
		}

		FreeSpace space;

		space.StartSector = StartSector;
		space.Length      = Length / 256;

		if ( Length % 256 )
		{
			space.Length++;
		}

		if ( MYFSID==FSID_ADFS_D )
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
		if ( WriteTranslatedSector( DSector( DirSector + s), &DirBytes[ s * DSectorSize ], DSectorSize, pSource ) != DS_SUCCESS )
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

		if ( ! (MYFSID==FSID_ADFS_D) )
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

		pNewFS->FSID = MAKEFSID( PLID, 0x01, 0x0A );

		DS_RELEASE( pSource );

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
	if ( ( MYFSID == FSID_ADFS_D ) || ( MYFSID == FSID_ADFS_HO ) || ( MYFSID == FSID_ADFS_L2 ) )
	{
		DWORD PostType = pDirectory->Files[ FileID ].RISCTYPE;

		if ( PostType != PreType )
		{
			/* The type was changed, update the load/exec stuff from it */
			InterpretNativeType(  &pDirectory->Files[ FileID ]  );
		}
	}

	FreeAppIcons( pDirectory );

	int r = pDirectory->WriteDirectory();
	{
		if ( r == DS_SUCCESS )
		{
			r =pDirectory->ReadDirectory();
		}
	}

	ResolveAppIcons<ADFSFileSystem>( this );

	return r;
}

int ADFSFileSystem::CompactImage( void )
{
	::SendMessage( ValidateWnd, WM_FSTOOL_SETDESC, 0, (LPARAM) L"This tool will move occupied sections of the disk up together to create one free space." );
	::SendMessage( ValidateWnd, WM_FSTOOL_PROGLIMIT, 0, 100 );
	::SendMessage( ValidateWnd, WM_FSTOOL_PROGRESS, Percent( 0, 1, 0, 1, false), 0 );

	BYTE Buffer[ 1024 ];

	DWORD CompactionSteps = pFSMap->Spaces.size();

	if ( CompactionSteps == 1 )
	{
		::PostMessage( ValidateWnd, WM_FSTOOL_PROGRESS, 100, 0 );

		return 0;
	}

	ADFSDirectory *pDir = new ADFSDirectory( pSource );

	pDir->FSID       = FSID;

	DWORD StepsRemaining = CompactionSteps;

	while ( StepsRemaining > 1 )
	{
		if ( WaitForSingleObject( hToolEvent, 0 ) == WAIT_OBJECT_0 )
		{
			return 0;
		}

		/* Get the first free space, add on the sectors, and then find the object */
		FreeSpace FirstSpace = pFSMap->Spaces.front();

		DWORD SeekSector = FirstSpace.StartSector + FirstSpace.Length;

		CompactionObject obj = FindCompactableObject( (MYFSID==FSID_ADFS_D)?0x04:0x02, SeekSector );

		if ( obj.StartSector == 0 )
		{
			/* EEP! Unfound object! */
			return NUTSError( 0x41, L"Compaction failed due to file system corruption." );
		}

		/* Copy the data 1 sector at a time from old place to new place */
		for ( DWORD sec=0; sec<obj.NumSectors; sec++ )
		{
			ReadTranslatedSector( DSector( SeekSector + sec ), Buffer, DSectorSize, pSource );
			WriteTranslatedSector( DSector( FirstSpace.StartSector + sec ), Buffer, DSectorSize, pSource );
		}

		/* Release the originating space */
		FreeSpace GoneSpace;

		GoneSpace.StartSector     = SeekSector;
		GoneSpace.OccupiedSectors = obj.NumSectors;
		GoneSpace.Length          = obj.NumSectors;

		pFSMap->ReleaseSpace( GoneSpace );

		/* Claim the new space */
		FirstSpace.Length = obj.NumSectors;
		FirstSpace.OccupiedSectors = obj.NumSectors;

		pFSMap->OccupySpace( FirstSpace );

		/* Update the directory */
		pDir->DirSector  = obj.ContainingDirSector;

		pDir->ReadDirectory();

		pDir->Files[ obj.ReferenceFileID ].SSector = FirstSpace.StartSector;

		pDir->WriteDirectory();

		StepsRemaining = pFSMap->Spaces.size();

		::PostMessage( ValidateWnd, WM_FSTOOL_PROGRESS, Percent( 0, 1, CompactionSteps - StepsRemaining, CompactionSteps, false ), 0 );
	}

	delete pDir;

	::PostMessage( ValidateWnd, WM_FSTOOL_PROGRESS, 100, 0 );

	return 0;
}

CompactionObject ADFSFileSystem::FindCompactableObject( DWORD DirSector, DWORD SeekSector )
{
	CompactionObject obj;

	obj.StartSector = 0;

	if ( WaitForSingleObject( hToolEvent, 0 ) == WAIT_OBJECT_0 )
	{
		return obj;
	}

	ADFSDirectory *pDir = new ADFSDirectory( pSource );

	pDir->FSID = FSID;
	pDir->DirSector    = DirSector;
	pDir->FloppyFormat = FloppyFormat;
	pDir->MediaShape   = MediaShape;

	pDir->ReadDirectory();

	NativeFileIterator iFile;

	/* See if it is an object in this directory */
	for ( iFile = pDir->Files.begin(); iFile != pDir->Files.end(); iFile++ )
	{
		if ( iFile->SSector == SeekSector )
		{
			/* Got it */
			obj.StartSector         = SeekSector;
			obj.ReferenceFileID     = iFile->fileID;
			obj.ContainingDirSector = DirSector;

			if ( iFile->Flags & FF_Directory )
			{
				obj.NumSectors = (MYFSID==FSID_ADFS_D)?0x02:0x05;
			}
			else
			{
				obj.NumSectors = iFile->Length / DSectorSize;

				if ( iFile->Length % DSectorSize )
				{
					obj.NumSectors++;
				}
			}

			return obj;
		}
	}

	/* No match. Try directories */
	for ( iFile = pDir->Files.begin(); iFile != pDir->Files.end(); iFile++ )
	{
		if ( iFile->Flags & FF_Directory )
		{
			obj = FindCompactableObject( iFile->SSector, SeekSector );

			if ( obj.StartSector != 0 )
			{
				return obj;
			}
		}
	}

	/* Still no match. Abort. */
	return obj;
}