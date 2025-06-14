#include "StdAfx.h"
#include "ADFSDirectoryCommon.h"

#include "Defs.h"
#include "RISCOSIcons.h"
#include "AcornDLL.h"
#include "../NUTS/NUTSError.h"

ADFSDirectoryCommon::ADFSDirectoryCommon(void)
{
	FloppyFormat = false;
}


ADFSDirectoryCommon::~ADFSDirectoryCommon(void)
{
}

void ADFSDirectoryCommon::TranslateType( NativeFile *file )
{
	DWORD Type = ( file->LoadAddr & 0x000FFF00 ) >> 8;

	file->RISCTYPE = Type;

	file->Type = RISCOSIcons::GetIntTypeForType( Type );
	file->Icon = RISCOSIcons::GetIconForType( Type );

	/*
	if ( file->Icon == 0x0 )
	{
		file->Icon            = RISCOSIcons::GetIconForType( FT_Arbitrary );
		file->HasResolvedIcon = true;
	}
	*/

	/* This timestamp converstion is a little esoteric. RISC OS uses centiseconds
	   since 0:0:0@1/1/1900. This is *nearly* unixtime. */
	QWORD UnixTime = file->LoadAddr & 0xFF;

	UnixTime <<= 32;
	UnixTime |= file->ExecAddr;

	UnixTime /= 100;
	UnixTime -= 2208988800; // Secs difference between 1/1/1970 and 1/1/1900.

	file->TimeStamp = (DWORD) UnixTime;

	if ( file->Flags & FF_Directory )
	{
		file->Type = FT_Directory;

		file->RISCTYPE = 0xA00;

		if ( file->Filename[ 0 ] == '!' )
		{
			file->Icon = RISCOSIcons::GetIconForType( 0xA01 );

			file->RISCTYPE = 0xA01;
		}
		else
		{
			file->Icon = RISCOSIcons::GetIconForType( 0xA00 );
		}
	}
	else
	{
		if ( ( file->RISCTYPE == 0xCC5 ) || ( file->RISCTYPE == 0xCB6 ) )
		{
			file->Icon = FT_Sound;
			file->Type = FT_Sound;
		}
	}

	switch ( file->RISCTYPE )
	{
		case 0xFFF: // Text
		case 0xFFE: // EXEC
		case 0xFEB: // Obey
			{
				file->XlatorID = TUID_TEXT;
			}
			break;

		case 0xFFB: // BASIC
			{
				file->XlatorID = BBCBASIC;
			}
			break;

		case 0xCC5: // MusMod2 Tracker file
		case 0xCB6: // ProTracker (?) file
			{
				file->XlatorID = TUID_MOD_MUSIC;
			}
			break;

		case 0xD3C: // ARMadeus Sample file
			{
				file->XlatorID = AUDIO_ARMADEUS;
			}
			break;

	}
}
