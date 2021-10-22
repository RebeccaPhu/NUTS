#include "StdAfx.h"
#include "MFSFAT.h"

#include "..\NUTS\NUTSError.h"

MFSFAT::MFSFAT( DataSource *pSrc )
{
	pRecord = nullptr;

	pSource = pSrc;

	if ( pSource != nullptr )
	{
		DS_RETAIN( pSource );
	}
}


MFSFAT::~MFSFAT(void)
{
	if ( pSource != nullptr )
	{
		DS_RELEASE( pSource );
	}
}

void MFSFAT::ReadFAT()
{
	if ( ( pSource == nullptr ) || ( pRecord == nullptr ) )
	{
		return;
	}

	// There are VolRecord:AllocBlocks entries to read, starting from 
	// byte 36 + 28 of sector 2. Each entry is 12 bits, so that's
	// ( AllocBlocks / 2 ) * 3 bytes. If AllocBlocks is odd, add 1.

	DWORD AllocBlocks = pRecord->AllocBlocks;

	if ( AllocBlocks & 1 ) { AllocBlocks++; }

	// Two extra bytes to account for overspill.
	BYTE Buffer[ 0x202 ];

	pSource->ReadSectorLBA( 2, Buffer, 0x200 );

	DWORD EntryPtr = 36 + 28;
	DWORD SecNum   = 2;

	WORD  blkid = 2;
	DWORD AllocBlockBytes = ( AllocBlocks / 2 ) * 3;
	DWORD b     = 0;
	DWORD fb[3];

	char err[256];

	for ( DWORD a=0; a<AllocBlockBytes; a++ )
	{
		if ( EntryPtr >= 0x200 )
		{
			// Out of data. Read next block
			SecNum++;

			pSource->ReadSectorLBA( SecNum, Buffer, 0x200 );

			EntryPtr -= 0x200;

			// Fix up entry numbers
			DWORD oblkid = blkid - 2;

		}

		fb[ b ] = Buffer[ EntryPtr ];

		b = ( b + 1 ) % 3;

		if ( b == 0 )
		{
			// EntryPtr points to 2 entries across 3 bytes.
			WORD Entry1 = (WORD) fb[ 0 ]; Entry1 <<= 4;
			WORD Entry2 = (WORD) fb[ 2 ];

			Entry1 |= (WORD) ( ( fb[ 1 ] & 0xF0 ) >> 4 );
			Entry2 |= ( (WORD) ( fb[ 1 ] & 0x0F ) ) << 8;

			FAT[ blkid ] = Entry1; blkid++;
			FAT[ blkid ] = Entry2; blkid++;
		}

		EntryPtr++;
	}
}

WORD MFSFAT::NextAllocBlock( WORD ThisAllocBlock )
{
	WORD r = 0x001;

	if ( FAT.find( ThisAllocBlock ) != FAT.end() )
	{
		r = FAT[ ThisAllocBlock ];
	}

	return r;
}

WORD MFSFAT::GetFreeAllocs()
{
	WORD FreeAllocs = 0;

	std::map<WORD,WORD>::iterator iFAT;

	for ( iFAT = FAT.begin(); iFAT != FAT.end(); iFAT++ )
	{
		if ( iFAT->second == 0x000 )
		{
			FreeAllocs++;
		}
	}

	return FreeAllocs;
}

int MFSFAT::ClearAllocs( WORD StartAlloc )
{
	WORD ThisAlloc = StartAlloc;

	while ( ( ThisAlloc != 0x000 ) && ( ThisAlloc != 0x001 ) && ( ThisAlloc != 0xFFF ) )
	{
		if ( FAT.find( ThisAlloc ) == FAT.end() )
		{
			return NUTSError( 0x801, L"Fork not found in FAT" );
		}

		WORD NextAlloc = FAT[ ThisAlloc ];

		FAT[ ThisAlloc ] = 0x000;

		ThisAlloc = NextAlloc;
	}

	return 0;
}

WORD MFSFAT::GetFileAllocCount( WORD StartAlloc )
{
	if ( StartAlloc == 0 )
	{
		return 0;
	}

	WORD AllocCount = 0;
	WORD ThisAlloc  = StartAlloc;

	while ( ( ThisAlloc != 0x000 ) && ( ThisAlloc != 0x001 ) && ( ThisAlloc != 0xFFF ) )
	{
		if ( FAT.find( ThisAlloc ) == FAT.end() )
		{
			return NUTSError( 0x801, L"Fork not found in FAT" );
		}

		WORD NextAlloc = FAT[ ThisAlloc ];

		ThisAlloc = NextAlloc;

		AllocCount++;
	}

	return AllocCount;
}

int MFSFAT::WriteFAT()
{
	if ( ( pSource == nullptr ) || ( pRecord == nullptr ) )
	{
		return NUTSError( 0x802, L"No data source, or no volume record. Aborting." );
	}

	// All this shenanigans starts on sector 2. But the FAT is prefixed by the Volume Record.
	// We'll load the first sector in, but not touch the volume record.

	BYTE Buffer[512];

	if ( pSource->ReadSectorLBA( 2, Buffer, 0x200 ) != DS_SUCCESS )
	{
		return -1;
	}

	// We have the volume record now, FAT starts at byte 36 + 28;
	DWORD EntryPtr = 36 + 28;
	DWORD SecNum   = 2;

	BYTE b = 0;
	BYTE fb[3];

	std::map<WORD,WORD>::iterator iFAT = FAT.begin();

	for ( WORD ia = 0; ia < pRecord->AllocBlocks; )
	{
		if ( b == 0 )
		{
			if ( ia != 0 ) { iFAT++; }

			fb[ 0 ] = 0;
			fb[ 1 ] = 0;
			fb[ 2 ] = 0;

			if ( iFAT == FAT.end() )
			{
				// ran out of FAT entries. No bother
				break;
			}

			fb[ 0 ] = (BYTE) ( iFAT->second >> 4 );
			fb[ 1 ] = (BYTE) ( ( iFAT->second & 0xF ) << 4 );

			iFAT++;
			ia++;

			if ( iFAT != FAT.end() )
			{
				fb[ 1 ] |= (BYTE) ( ( iFAT->second & 0xF00 ) >> 8 );
				fb[ 2 ]  = (BYTE) ( iFAT->second & 0xFF);
			}

			ia++;
		}

		Buffer[ EntryPtr ] = fb[ b ];

		EntryPtr++;

		b = ( b + 1 ) % 3;

		if ( EntryPtr == 0x200 )
		{
			if ( pSource->WriteSectorLBA( SecNum, Buffer, 0x200 ) != DS_SUCCESS )
			{
				return -1;
			}

			EntryPtr -= 0x0200;
			SecNum++;
		}

		if ( iFAT == FAT.end() )
		{
			break;
		}
	}

	if ( pSource->WriteSectorLBA( SecNum, Buffer, 0x200 ) != DS_SUCCESS )
	{
		return -1;
	}

	return 0;
}

std::vector<WORD> MFSFAT::AllocateBlocks( WORD RequiredBlocks )
{
	std::vector<WORD> Blocks;

	WORD b = 0;
	WORD LastBlock = 0;

	std::map<WORD,WORD>::iterator iFAT = FAT.begin();
	
	while ( b < RequiredBlocks )
	{
		if ( iFAT->second == 0x000 )
		{
			Blocks.push_back( iFAT->first );

			b++;
		}

		iFAT++;

		if ( iFAT == FAT.end() )
		{
			break;
		}
	}

	if ( b != RequiredBlocks )
	{
		Blocks.clear();

		return Blocks;
	}

	// Got enough, now assign them
	std::vector<WORD>::iterator ThisBlock = Blocks.begin();
	std::vector<WORD>::iterator NextBlock = Blocks.begin();

	if ( Blocks.size() == 1 )
	{
		FAT[ Blocks[0] ] = 0x001;

		return Blocks;
	}

	NextBlock++;

	while ( NextBlock != Blocks.end() )
	{
		FAT[ *ThisBlock ] = *NextBlock;

		ThisBlock++;
		NextBlock++;

		if ( NextBlock == Blocks.end() )
		{
			FAT[ *ThisBlock ] = 0x001;
		}
	}

	return Blocks;
}

void MFSFAT::CreateFAT( WORD Entries )
{
	FAT.clear();
	
	for ( WORD f=0; f<Entries; f++ )
	{
		FAT[ f + 2 ] = 0x000;
	}
}