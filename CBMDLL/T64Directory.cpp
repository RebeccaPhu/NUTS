#include "StdAfx.h"
#include "T64Directory.h"

#include "CBMFunctions.h"
#include "CBMDefs.h"
#include "../NUTS/libfuncs.h"

int T64Directory::ReadDirectory( void )
{
	Files.clear();
	ResolvedIcons.clear();

	BYTE Buffer[ 0x40 ];

	pSource->ReadRaw( 0, 0x40, Buffer );

	TapeVersion = * (WORD *) &Buffer[ 0x20 ];

	WORD MaxEntries  = * (WORD *) &Buffer[ 0x22 ];
	WORD UsedEntries = * (WORD *) &Buffer[ 0x24 ];

	memcpy( Container, &Buffer[ 0x28 ], 0x18 );

	TToString( Container, 0x18 );

	DWORD FileID = 0;

	FileType ftypes[8] = {
		FT_Arbitrary, //DEL
		FT_Data,      // SEQ
		FT_Binary,    // PRG
		FT_Data,      // USR
		FT_Data,      // REL
		FT_None,      // Illegal
		FT_None,      // Illegal
		FT_None       // Illegal
	};

	for ( WORD i=0; i<MaxEntries; i++ )
	{
		pSource->ReadRaw( 0x40 + ( 0x20 * i ), 0x20, Buffer );

		if ( Buffer[ 0 ] == 0 )
		{
			/* deleted entry - apparently, this doesn't work with C64S, but still exists in the spec */
			continue;
		}

		NativeFile file;

		file.Attributes[ 0 ] = 0x40 + ( 0x20* i );         // Directory offset
		file.Attributes[ 1 ] = * (WORD *) &Buffer[ 0x08 ]; // Data offset
		file.Attributes[ 2 ] = * (WORD *) &Buffer[ 0x02 ]; // Load address

		memcpy( file.Filename, &Buffer[ 0x10 ], 0x10 );

		TToString( file.Filename, 0x10 );

		file.EncodingID = ENCODING_PETSCII;
		file.fileID     = FileID;
		file.Flags      = FF_Extension;
		file.FSFileType = FT_CBM_TAPE;
		file.Icon       = FT_Arbitrary;
		file.Type       = FT_Arbitrary;
		file.XlatorID   = NULL;

		/* Heh. File type. You assholes. */
		const BYTE *extns[ 6 ] = { (BYTE *) "DEL", (BYTE *) "SEQ", (BYTE *) "PRG", (BYTE *) "USR", (BYTE *) "REL", (BYTE *) "FRZ" };

		if ( Buffer[ 0 ] == 3 )
		{
			file.Attributes[ 3 ] = 0x05;

			file.Type = FT_Binary;
			file.Icon = FT_Binary;
		}
		else
		{
			if ( ( Buffer[ 0x01 ] >= 0x80 ) && ( Buffer[ 0x01 ] <= 0x84 ) )
			{
				file.Attributes[ 3 ] = Buffer[ 0x01 ] & 0x7;

				file.Type = ftypes[ file.Attributes[ 3 ] ];
				file.Icon = ftypes[ file.Attributes[ 3 ] ];
			}
			else
			{
				/* Apparently if it's not a real value then it's really a PRG. WHYYYYYYY */
				file.Attributes[ 3 ] = 0x02;

				file.Type = FT_Binary;
				file.Icon = FT_Binary;
			}
		}

		rstrncpy( file.Extension, (BYTE *) extns[ file.Attributes[ 3 ] ], 3 );

		WORD EndAddr    = * (WORD *) &Buffer[ 0x04 ];

		file.Length     = ( EndAddr - file.Attributes[ 2 ] ) + 1;

		file.Length    += 2;

		file.HasResolvedIcon = false;

		Files.push_back( file );

		FileID++;
	}

	return 0;
}
