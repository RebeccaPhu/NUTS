#include "StdAfx.h"
#include "IECATADirectory.h"
#include "../nuts/libfuncs.h"

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

		pSource->ReadSector( nextBlock, d64cache, 512 );

		nextBlock	= * (DWORD *) &d64cache[0];

		for (int f=0; f<18; f++) {
			BYTE *fp = &d64cache[4 + (f * 28)];

			int	startSect = * (int *) &fp[16];

			if (startSect != 0) {	//	Files can't be on sector 0, so this is an indicator of a null entry.
				NativeFile	file;

				file.Flags           = 0;
				file.fileID          = FileID;
				file.Attributes[ 0 ] = startSect;
				file.AttrLocked      = (fp[0x2] & 64)?true:false;
				file.AttrClosed      = (fp[26])?true:false;	//	Used to mean "closed"
				file.AttrType        = fp[24] & 0x03;
				file.XlatorID        = NULL;
				file.HasResolvedIcon = false;

				file.Type = ftypes[fp[24] & 0x3];
				file.Icon = ftypes[fp[24] & 0x3];

				if ((fp[24] & 0x3) == 3) {
					file.Flags |= FF_Directory;
				}
				else
				{
					file.Flags |= FF_Extension;
					_snprintf_s( (char *) file.Extension, 4, 3, (char *) extns[ fp[24] & 0x3 ] );
				}

				file.FSFileType = FT_C64;
				file.EncodingID = ENCODING_PETSCII;

				memcpy( file.Filename, &fp[0x00], 16 );

				file.Filename[ 16 ] = 0;

				//	The length is stored in two parts. Firstly, the number of sectors (512 bytes), then the number
				//	of trailing bytes in the last sector. Thus the value is ((s - 1) * 512) + t.
				WORD sects = * (WORD *) &fp[20];
				WORD bytes = * (WORD *) &fp[22];

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

	unsigned int TotalFiles     = Files.size();
	unsigned int BlocksRequired = TotalFiles / 18;

	BYTE Data[512];

	if ( TotalFiles == 0 )
	{
		BlocksRequired = 1;

		memset(Data, 0, 512);	//	Must blank this so non-existent entries are not accidentally read.

		pSource->WriteSector( 1, Data, 512 );
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
		ReleaseBlock( &UnneededBlocks );
	}

	//	Now we can write the directory entries. Entries are 28 bytes long starting at 4 bytes in. The first 4 bytes
	//	are the link to the next sector in the directory chain.

	DWORD i;
	BYTE  *fp;
	DWORD CSector;
	DWORD FileID = 0;

	std::vector<int>::iterator	iSector	= DirectorySectorChain.begin();

	CSector	= *iSector;
	i		= 0;

	for (std::vector<NativeFile>::iterator iFile = Files.begin(); iFile != Files.end(); iFile++) {
		if (i == 0) {
			iSector++;

			memset(Data, 0, 512);	//	Must blank this so non-existent entries are not accidentally read.

			if (iSector != DirectorySectorChain.end()) {
				* (DWORD *) &Data[0]	= *iSector;
			} else {
				* (DWORD *) &Data[0]	= 0;
			}
		}

		fp	= &Data[4 + (i * 28)];

		WORD blks = (WORD) ( iFile->Length / 512 );

		if (iFile->Length % 512)
			blks++;

		rstrncpy( &fp[0], iFile->Filename, 16 );

		* (DWORD *) &fp[16] = iFile->Attributes[ 0 ];
		* (WORD *)  &fp[20] = blks;
		* (WORD *)  &fp[22] = iFile->Length % 512;

		* (BYTE *)  &fp[24] = (BYTE) iFile->AttrType;

		if ( iFile->Flags & FF_Directory )
			* (BYTE *) &fp[24]	= 0x03;
	
		* (BYTE *)  &fp[25] = (iFile->AttrClosed)?0xFF:0x00;
		* (BYTE *)  &fp[26] = (iFile->AttrClosed)?0x00:0xFF;
		* (BYTE *)  &fp[27] = 0;
		
		i++;

		iFile->fileID = FileID;

		FileID++;

		if (i == 18) {
			pSource->WriteSector(CSector, Data, 512);

			if ( iSector != DirectorySectorChain.end() )
			{
				CSector = *iSector;
			}

			i = 0;
		}
	}

	if (i != 18)
	{
		pSource->WriteSector(CSector, Data, 512);
	}

	return 0;
}

int IECATADirectory::GetFreeBlock() {

	unsigned char	DataBuf[512];
	DWORD			*pBlocks	= (DWORD *) DataBuf;

	//	Read the start of the free blocks list from the superblock. Because the start shrinks as chunks of 64 indexes
	//	are used up, getting a free block is guaranteed to take up no more than 3 sector cycles.

	pSource->ReadSector(0, DataBuf, 512);

	long	FBSect	= pBlocks[0];	//	First DWORD holds the start sector of the FBL.
	long	Block;

	if (FBSect == 0)	//	This means no more space.
		return -1;

	pSource->ReadSector(FBSect, DataBuf, 512);

	//	Now scan the read in block for free blocks. Since 0 can't be free by design, 0 indicates a null entry. Entry 127
	//	is zero if this is the end of the FBL chain, or non-zero for the next FBL sector. If indexes 0-126 are all zero, then
	//	the next sector in the chain potentially holds the first valid free block. This means the current block is actually
	//	free since it holds dead entries. In this event, the next sector is written back to the superblock as the start
	//	of EBL, and the current sector returned as the entry.

	for (int i=0; i<127; i++) {
		if (pBlocks[i] != 0) {
			//	This entry holds a valid value, so claim it and return it.

			Block	= pBlocks[i];

			pBlocks[i]	= 0;	// Claimed.

			pSource->WriteSector(FBSect, DataBuf, 512);

			return Block;
		}
	}

	//	All of the entries were zero, so actually this block is technically free.
	Block	= FBSect;
	FBSect	= pBlocks[127];

	pSource->ReadSector(0, DataBuf, 512);	//	Re-read superblock

	pBlocks[0]	= FBSect;					//	Change the start of FBL.

	pSource->WriteSector(0, DataBuf, 512);	//	Write the superblock back

	return Block;	//	Return the original start sector of the FBL as a free block.
}

void IECATADirectory::ReleaseBlock( std::vector<DWORD> *pBlocks ) {
	//	The trick here is to start with the first link table as described by the superblock, and find an entry that
	//	is zero. Then replace it with the specified block and write that sector back.
	
	DWORD	LinkTable[128];

	int		CSector;

	pSource->ReadSector(0, LinkTable, 512);

	//	Free blocks list pointed to by first 4 bytes.
	CSector	= LinkTable[0];

	pSource->ReadSector(CSector, LinkTable, 512);

	while (1) {
		for (int i=0; i<127; i++) {
			if (LinkTable[i] == 0) {
				DWORD Block = pBlocks->back();

				pBlocks->pop_back();

				//	Found an available entry. Fill it, write it and exit.
				LinkTable[i] = Block;

				if ( pBlocks->size() == 0 )
				{
					pSource->WriteSector(CSector, LinkTable, 512);

					return;
				}
			}
		}

		//	Entry 63 is a link to the next table. If it is zero, then we ironically, need to steal a block to return one.
		if (LinkTable[127] == 0) {
			int	NewSector	= GetFreeBlock();

			if (NewSector == -1)
				return;	//	No free sectors to record this free sector.

			LinkTable[127]	= NewSector;

			pSource->WriteSector(CSector, LinkTable, 512);

			memset(LinkTable, 0, 512);
		} else {
			CSector	= LinkTable[127];

			pSource->ReadSector(CSector, LinkTable, 512);
		}
	}
}