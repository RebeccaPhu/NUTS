#include "StdAfx.h"
#include "IECATADirectory.h"
#include "../nuts/libfuncs.h"
#include "../NUTS/NUTSError.h"

int	IECATADirectory::ReadDirectory(void) {
	Files.clear();

	unsigned char d64cache[512];

	DWORD nextBlock = DirSector;

	//	This is used to record a chain of directory sectors for later. If we add or remove from the catalogue,
	//	then either the chain is cut short and the trailing sectors should be marked as free, or the chain is extended,
	//	in which case the a sector from the free map must be claimed and tagged on the end. By recording the existing
	//	chain we simply re-use the currently allocated sectors and save a lot of hassle.
	DirectorySectorChain.clear();

	FileType ftypes[4] = {
		FT_None,     // Illegal ?
		FT_Data,     // SEQ
		FT_Binary,   // PRG
		FT_Directory // Directory - technically not a file type, but a placeholder.
	};

	char extns[4][4] = { "ILL", "SEQ", "PRG", "DIR" };

	DWORD FileID = 0;

	while ( nextBlock != 0U ) {
		DirectorySectorChain.push_back(nextBlock);

		if ( pSource->ReadSectorLBA( nextBlock, d64cache, 512 ) != DS_SUCCESS )
		{
			return -1;
		}

		nextBlock	= * (DWORD *) &d64cache[0];

		for (int f=0; f<18; f++) {
			BYTE *fp = &d64cache[4 + (f * 28)];

			DWORD startSect = LEDWORD( &fp[16] );

			if (startSect != 0) {	//	Files can't be on sector 0, so this is an indicator of a null entry.
				NativeFile	file;

				file.Flags           = 0;
				file.fileID          = FileID;
				file.Attributes[ 0 ] = startSect;
				file.AttrLocked      = (fp[25])?0xFFFFFFFF:0x00000000;
				file.AttrClosed      = (fp[26])?0xFFFFFFFF:0x00000000;	//	Used to mean "closed"
				file.AttrType        = fp[24] & 0x03;
				file.HasResolvedIcon = false;

				file.Type = ftypes[fp[24] & 0x3];
				file.Icon = ftypes[fp[24] & 0x3];

				if ((fp[24] & 0x3) == 3) {
					file.Flags |= FF_Directory;
				}
				else
				{
					file.Flags |= FF_Extension;
					file.Extension = BYTEString( (BYTE *) extns[ fp[24] & 0x3 ] );
				}

				file.FSFileType = FT_C64;
				file.EncodingID = ENCODING_PETSCII;

				file.Filename = BYTEString( &fp[0x00], 16 );

				//	The length is stored in two parts. Firstly, the number of sectors (512 bytes), then the number
				//	of trailing bytes in the last sector. Thus the value is ((s - 1) * 512) + t.
				WORD sects = LEWORD( &fp[20] );
				WORD bytes = LEWORD( &fp[22] );

				file.Length	= ((sects - 1) * 512) + bytes;

				Files.push_back(file);

				FileID++;
			}
		}
	}

	return 0;
}

int	IECATADirectory::WriteDirectory( void ) {

	//	Writing the directory requires adding or removing sectors from the directory chain.
	//	We calculate the number of sectors required (being there are 18 entries per sector) and then determine
	//	the difference in that number and the number recorded by the DirectorySectorChain list. In theory, if
	//	DeleteFile() is called, then this should never be more than 1 sector. But we will handle wider gaps. If
	//	there are more sectors than needed, then sectors are picked off from the back end and released. If there
	//	are less, then blocks are allocated and tacked on the end.

	DWORD TotalFiles     = (DWORD) Files.size();
	DWORD BlocksRequired = TotalFiles / 18;

	BYTE Data[512];

	if ( TotalFiles == 0 )
	{
		BlocksRequired = 1;

		memset(Data, 0, 512);	//	Must blank this so non-existent entries are not accidentally read.

		if ( pSource->WriteSectorLBA( 1, Data, 512 ) != DS_SUCCESS )
		{
			return -1;
		}
	}
	else if (TotalFiles % 18)
	{
		BlocksRequired++;
	}

	while (BlocksRequired > DirectorySectorChain.size())
	{
		DirectorySectorChain.push_back(GetFreeBlock());
	}

	std::vector<DWORD> UnneededBlocks;

	while (BlocksRequired < DirectorySectorChain.size()) {
		DWORD Block = *(DirectorySectorChain.rbegin());

		UnneededBlocks.push_back( Block );

		DirectorySectorChain.erase(--DirectorySectorChain.end());
	}

	if ( UnneededBlocks.size() != 0 )
	{
		if ( ReleaseBlock( &UnneededBlocks ) != DS_SUCCESS )
		{
			return -1;
		}
	}

	//	Now we can write the directory entries. Entries are 28 bytes long starting at 4 bytes in. The first 4 bytes
	//	are the link to the next sector in the directory chain.

	DWORD i;
	BYTE  *fp;
	DWORD CSector;
	DWORD FileID = 0;

	std::vector<DWORD>::iterator iSector = DirectorySectorChain.begin();

	CSector	= *iSector;
	i		= 0;

	for (std::vector<NativeFile>::iterator iFile = Files.begin(); iFile != Files.end(); iFile++) {
		if (i == 0) {
			iSector++;

			memset(Data, 0, 512);	//	Must blank this so non-existent entries are not accidentally read.

			if (iSector != DirectorySectorChain.end()) {
				WLEDWORD( &Data[0], *iSector );
			} else {
				WLEDWORD( &Data[0], 0 );
			}
		}

		fp	= &Data[4 + (i * 28)];

		WORD blks = (WORD) ( iFile->Length / 512 );

		if (iFile->Length % 512)
			blks++;

		rstrncpy( &fp[0], iFile->Filename, 16 );

		WLEDWORD( &fp[16], iFile->Attributes[ 0 ] );
		WLEWORD(  &fp[20], blks );
		WLEWORD(  &fp[22], iFile->Length % 512 );

		fp[24] = (BYTE) iFile->AttrType;

		if ( iFile->Flags & FF_Directory )
			* (BYTE *) &fp[24]	= 0x03;
	
		fp[25] = (iFile->AttrLocked)?0xFF:0x00;
		fp[26] = (iFile->AttrClosed)?0xFF:0x00;
		fp[27] = 0;
		
		i++;

		iFile->fileID = FileID;

		FileID++;

		if (i == 18) {
			if ( pSource->WriteSectorLBA(CSector, Data, 512) != DS_SUCCESS )
			{
				return -1;
			}

			if ( iSector != DirectorySectorChain.end() )
			{
				CSector = *iSector;
			}

			i = 0;
		}
	}

	if (i != 18)
	{
		if ( pSource->WriteSectorLBA(CSector, Data, 512) != DS_SUCCESS )
		{
			return -1;
		}
	}

	return 0;
}

DWORD IECATADirectory::GetFreeBlock()
{

	BYTE   DataBuf[512];
	DWORD *pBlocks = (DWORD *) DataBuf;

	//	Read the start of the free blocks list from the superblock. Because the start shrinks as chunks of 64 indexes
	//	are used up, getting a free block is guaranteed to take up no more than 3 sector cycles.

	if ( pSource->ReadSectorLBA(0, DataBuf, 512) != DS_SUCCESS )
	{
		return 0;
	}

	DWORD FBSect = pBlocks[0]; // First DWORD holds the start sector of the FBL.
	DWORD Block;

	if ( FBSect == 0 )
	{
		// This means no more space.
		int i = NUTSError( 0x104, L"No free sectors" );

		return 0;
	}

	if ( pSource->ReadSectorLBA(FBSect, DataBuf, 512) != DS_SUCCESS )
	{
		return 0;
	}

	//	Now scan the read in block for free blocks. Since 0 can't be free by design, 0 indicates a null entry. Entry 127
	//	is zero if this is the end of the FBL chain, or non-zero for the next FBL sector. If indexes 0-126 are all zero, then
	//	the next sector in the chain potentially holds the first valid free block. This means the current block is actually
	//	free since it holds dead entries. In this event, the next sector is written back to the superblock as the start
	//	of EBL, and the current sector returned as the entry.

	for ( BYTE i = 0; i < 127; i++ )
	{
		if ( pBlocks[i] != 0 )
		{
			//	This entry holds a valid value, so claim it and return it.
			Block = pBlocks[i];

			pBlocks[i] = 0; // Claimed.

			if ( pSource->WriteSectorLBA(FBSect, DataBuf, 512) != DS_SUCCESS )
			{
				return 0;
			}

			return Block;
		}
	}

	// All of the entries were zero, so actually this block is technically free.
	Block  = FBSect;
	FBSect = pBlocks[127]; // Next block in the FB table

	// Re-read superblock
	if ( pSource->ReadSectorLBA( 0, DataBuf, 512 ) != DS_SUCCESS )
	{
		return 0;
	}

	pBlocks[0] = FBSect; // Change the start of FBL.

	// Write the superblock back
	if ( pSource->WriteSectorLBA( 0, DataBuf, 512 ) != DS_SUCCESS )
	{
		return 0;
	}

	return Block; // Return the original start sector of the FBL as a free block.
}

int IECATADirectory::ReleaseBlock( std::vector<DWORD> *pBlocks ) {
	// The trick here is to start with the first link table as described by the superblock, and find an entry that
	// is zero. Then replace it with the specified block and write that sector back.
	
	DWORD LinkTable[128];
	DWORD CSector;

	if ( pSource->ReadSectorLBA( 0, (BYTE *) LinkTable, 512 ) != DS_SUCCESS )
	{
		return -1;
	}

	// Free blocks list is pointed to by first 4 bytes.
	CSector = LinkTable[ 0 ];

	if ( pSource->ReadSectorLBA( CSector, (BYTE *) LinkTable, 512 ) != DS_SUCCESS )
	{
		return -1;
	}

	while ( 1 )
	{
		for ( BYTE i = 0; i < 127; i++ )
		{
			if ( LinkTable[ i ] == 0 )
			{
				DWORD Block = pBlocks->back();

				pBlocks->pop_back();

				// Found an available entry. Fill it, write it and exit.
				LinkTable[ i ] = Block;

				if ( pBlocks->size() == 0 )
				{
					// All done.
					if ( pSource->WriteSectorLBA( CSector, (BYTE *) LinkTable, 512 ) != DS_SUCCESS )
					{
						return -1;
					}

					return 0;
				}
			}
		}

		// If we're here, then the current FB sector run out of entries, and the Blocks vector still has some.
		// We'll need to move on to the next sector, and keep filling in.

		// Entry 127 is a link to the next table. If it is zero, then we ironically, need to steal a block to return one.
		if ( LinkTable[127] == 0 )
		{
			DWORD NewSector = GetFreeBlock();

			if ( NewSector == 0 )
			{
				return -1;	//	No free sectors to record this free sector.
			}

			LinkTable[ 127 ] = NewSector;

			if ( pSource->WriteSectorLBA( CSector, (BYTE *) LinkTable, 512 ) != DS_SUCCESS )
			{
				return -1;
			}

			memset( LinkTable, 0, 512 );

			CSector = NewSector;
		} else {
			// Write out current sector as we've probably made changes.
			if ( pSource->WriteSectorLBA( CSector, (BYTE *) LinkTable, 512 ) != DS_SUCCESS )
			{
				return -1;
			}

			// Another sector to follow. Load that FB sector in and carry on filling in stuff.
			CSector	= LinkTable[ 127 ];

			if ( pSource->ReadSectorLBA(CSector, (BYTE *) LinkTable, 512) != DS_SUCCESS )
			{
				return -1;
			}
		}
	}

	return -1;
}