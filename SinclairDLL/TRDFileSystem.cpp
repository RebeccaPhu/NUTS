#include "StdAfx.h"
#include "TRDFileSystem.h"
#include "SinclairDefs.h"

#include "../NUTS/NUTSError.h"

TRDFileSystem::TRDFileSystem(DataSource *pDataSource) : FileSystem(pDataSource) 
{
	pSource = pDataSource;

	TopicIcon = FT_DiskImage;

	Flags = FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Uses_Extensions;

	pDirectory = nullptr;

	PreferredArbitraryExtension = (BYTE *) "C";
}


TRDFileSystem::~TRDFileSystem(void)
{
	if ( pDirectory != nullptr )
	{
		delete pDirectory;
	}
}

void TRDFileSystem::SetShape()
{
	DiskShape shape;

	if ( ( FSID == FSID_TRD_40DS ) || ( FSID == FSID_TRD_80DS ) )
	{
		shape.Heads = 2;
	}
	else
	{
		shape.Heads = 1;
	}

	if ( ( FSID == FSID_TRD_40SS ) || ( FSID == FSID_TRD_40DS ) )
	{
		shape.Tracks = 40;
	}
	else
	{
		shape.Tracks = 80;
	}

	shape.SectorSize = 256;
	shape.Sectors    = 16;

	shape.InterleavedHeads = false;
	
	pSource->SetDiskShape( shape );
}

FSHint TRDFileSystem::Offer( BYTE *Extension )
{
	SetShape();

	FSHint hint;

	hint.Confidence = 0;
	hint.FSID       = FS_Null;

	if ( Extension == nullptr )
	{
		return hint;
	}

	if ( rstrncmp( Extension, (BYTE *) "TRD", 3 ) )
	{
		hint.Confidence = 10;
		hint.FSID       = FSID;

		BYTE Sector[ 256 ];

		pSource->ReadSectorCHS( 0, 0, 8, Sector );

		if ( Sector[ 231 ] == 0x10 )
		{
			hint.Confidence += 10;
		}

		if ( ( FSID == FSID_TRD_80DS ) && ( Sector[ 227 ] == 0x16 ) ) { hint.Confidence += 10; }
		if ( ( FSID == FSID_TRD_40DS ) && ( Sector[ 227 ] == 0x17 ) ) { hint.Confidence += 10; }
		if ( ( FSID == FSID_TRD_80SS ) && ( Sector[ 227 ] == 0x18 ) ) { hint.Confidence += 10; }
		if ( ( FSID == FSID_TRD_40SS ) && ( Sector[ 227 ] == 0x19 ) ) { hint.Confidence += 10; }
	}

	return hint;
}

int TRDFileSystem::ReadFile(DWORD FileID, CTempFile &store)
{
	NativeFile *pFile = &pDirectory->Files[ FileID ];

	BYTE Track  = pFile->Attributes[ 1 ];
	BYTE Sector = pFile->Attributes[ 0 ];
	BYTE Head   = 0;
	BYTE Tracks = 80;

	if ( ( FSID == FSID_TRD_40SS ) || ( FSID == FSID_TRD_40DS ) )
	{
		Tracks = 40;
	}

	if ( Track > Tracks )
	{
		Track -= Tracks;
		Head++;
	}

	DWORD BytesToGo = pFile->Length;

	BYTE SectorBuf[ 256 ];

	store.Seek( 0 );

	while ( BytesToGo > 0 )
	{
		DWORD BytesRead = min( BytesToGo, 256 );

		if ( pSource->ReadSectorCHS( Head, Track, Sector, SectorBuf ) != DS_SUCCESS )
		{
			return -1;
		}

		store.Write( SectorBuf, BytesRead );

		BytesToGo -= BytesRead;

		Sector++;

		if ( Sector == 16) { Sector = 0; Track++; }
		if ( Track == Tracks ) { Track = 0; Head++; }

		if ( Head == 2 )
		{
			return NUTSError( 0x204, L"TRD read beyond head 1" );
		}
	}

	return 0;
}