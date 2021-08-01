#include "StdAfx.h"
#include "TZXDirectory.h"

#include "TZXIcons.h"
#include "../NUTS/Preference.h"

#include "../NUTS/libfuncs.h"

int TZXDirectory::ReadDirectory( void )
{
	Files.clear();
	ResolvedIcons.clear();
	BlockLengths.clear();

	ReadOffset = 0x0A;

	DWORD FileID = 0;

	while ( ReadOffset < pSource->PhysicalDiskSize )
	{
		NativeFile file;

		ZeroMemory( &file.Attributes, sizeof( file.Attributes ) );

		BYTE BlockID;
		WORD BlockSize;

		file.Attributes[  2 ] = 0xFFFFFFFE;
		file.Attributes[ 15 ] = ReadOffset;

		ParseBlock( &BlockID, &BlockSize );

		file.Attributes[ 14 ] = BlockID;
		file.Attributes[ 13 ] = FileID;
		file.EncodingID       = tzxpb.Encoding;
		file.fileID           = FileID;
		file.Flags            = FF_NotRenameable;
		file.FSFileType       = FT_TZX;
		file.HasResolvedIcon  = false;
		file.Length           = BlockSize;
		file.Type             = FT_Arbitrary;
		file.Icon             = IconMap[ BlockID ];
		file.XlatorID         = NULL;

		file.Flags |= FF_Audio;

		BYTE tf[ 256 ];

		rsprintf( tf, "%03d: %s", FileID, IdentifyBlock( BlockID ) );

		file.Filename = tf;

		Files.push_back( file );

		BlockLengths.push_back( BlockSize );

		FileID++;
	}

	bool Resolve = Preference( tzxpb.RegistryPrefix + L"ResolveFiles", true );

	if ( Resolve )
	{
		ResolveFiles();

		DWORD TextOpt = Preference( tzxpb.RegistryPrefix + L"_ExportTextOption", (DWORD) 0 );
		DWORD DataOpt = Preference( tzxpb.RegistryPrefix + L"_ExportDataOption", (DWORD) 0 );

		/* Process the text-like files */
		NativeFileIterator iFile;

		for ( iFile = Files.begin(); iFile != Files.end(); iFile++ )
		{
			BYTE BlockID = iFile->Attributes[ 14 ];

			if ( iFile->FSFileType == FT_TZX )
			{
				if ( ( BlockID >= 0x30 ) && ( BlockID <= 0x35 ) && ( TextOpt == 1 ) )
				{
					iFile->XlatorID = TUID_TEXT;
					iFile->Icon     = FT_Text;
					iFile->Type     = FT_Text;
					iFile->Flags   |= FF_EncodingOverride;
				}

				if ( DataOpt == 1 )
				{
					if ( BlockID == 0x10 )
					{
						/* If this hasn't been resolved as some more explicit type, then assume
						   that the flag and checksum bytes are in fact real data bytes, and include them */
						iFile->Length -= 0x05;
					}

					if ( BlockID == 0x11 )
					{
						iFile->Length -= 0x13;
					}

					if ( BlockID == 0x14 )
					{
						iFile->Length -= 0x0B;
					}
				}
			}
		}
	}

	return 0;
}

void TZXDirectory::ParseBlock( BYTE *BlockID, WORD *BlockSize )
{
	QWORD Target = ReadOffset;

	*BlockID = GetByte();

	switch ( *BlockID )
	{
	case 0x10:
		(void) GetWord();

		ReadOffset += GetWord();
		
		break;

	case 0x11:
		(void) GetWord();
		(void) GetWord();
		(void) GetWord();
		(void) GetWord();
		(void) GetWord();
		(void) GetWord();
		(void) GetByte();
		(void) GetWord();

		ReadOffset += GetTriByte();

		break;

	case 0x12:
		ReadOffset += 4;

		break;

	case 0x13:
		ReadOffset += 2 * GetByte();

		break;

	case 0x14:
		(void) GetWord();
		(void) GetWord();
		(void) GetByte();
		(void) GetWord();

		ReadOffset += GetTriByte();

		break;

	case 0x15:
		(void) GetWord();
		(void) GetWord();
		(void) GetByte();

		ReadOffset += GetTriByte();

		break;

	case 0x18:
	case 0x19:
	case 0x2B:
		ReadOffset += GetDWord();

		break;

	case 0x20:
	case 0x23:
	case 0x24:
		ReadOffset += 2;

		break;

	case 0x21:
	case 0x30:
		ReadOffset += GetByte();

		break;

	case 0x22:
	case 0x25:
	case 0x27:
		break;

	case 0x26:
		ReadOffset += 2 * GetWord();

		break;

	case 0x28:
		ReadOffset += GetWord();

		break;

	case 0x2A:
		ReadOffset += 4;
		break;

	case 0x31:
		(void) GetByte();
		ReadOffset += GetByte();

		break;
	
	case 0x32:
		ReadOffset += GetWord();
		break;

	case 0x33:
		ReadOffset += 3 * GetByte();
		break;

	case 0x35:
		(void) GetDWord();
		(void) GetDWord();
		(void) GetWord();
		
		ReadOffset += GetDWord();

		break;

	case 0x5A:
		ReadOffset += 9;
		break;

	default:
		/* Extension rule: If the block type is unrecognised, it should
		   have a four-byte length at the front. */

		ReadOffset += GetDWord();

		break;
	}

	*BlockSize = (WORD) ( ReadOffset - Target );
}

const BYTE *TZXDirectory::IdentifyBlock( BYTE BlockID )
{
	switch ( BlockID )
	{
	case 0x10:
		return (const BYTE *) "Standard data block";
	case 0x11:
		return (const BYTE *) "Turbo data block";
	case 0x12:
		return (const BYTE *) "Pure Tone";
	case 0x13:
		return (const BYTE *) "Pulse Sequence";
	case 0x14:
		return (const BYTE *) "Pure Data";
	case 0x15:
		return (const BYTE *) "Direct Recording";
	case 0x18:
		return (const BYTE *) "Compressed Square Wave";
	case 0x19:
		return (const BYTE *) "Generalized Data";
	case 0x20:
		return (const BYTE *) "Pause";
	case 0x21:
		return (const BYTE *) "Group Start";
	case 0x22:
		return (const BYTE *) "Group End";
	case 0x23:
		return (const BYTE *) "Jump";
	case 0x24:
		return (const BYTE *) "Loop Start";
	case 0x25:
		return (const BYTE *) "Loop End";
	case 0x26:
		return (const BYTE *) "Call Sequence";
	case 0x27:
		return (const BYTE *) "Return";
	case 0x28:
		return (const BYTE *) "Select";
	case 0x2A:
		return (const BYTE *) "Stop if not 48K";
	case 0x2B:
		return (const BYTE *) "Set Signal";
	case 0x30:
		return (const BYTE *) "Description";
	case 0x31:
		return (const BYTE *) "Message";
	case 0x32:
		return (const BYTE *) "Archive Information";
	case 0x33:
		return (const BYTE *) "Hardware Information";
	case 0x35:
		return (const BYTE *) "Custom Data";
	case 0x4B:
		return (const BYTE *) "Kansas City Standard Data";
	case 0x5A:
		return (const BYTE *) "Glue";
	}

	static BYTE unk[256];

	rsprintf( unk, "Unknown block %02X", BlockID );

	return (const BYTE *) unk;
}