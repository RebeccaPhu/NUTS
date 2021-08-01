#include "StdAfx.h"
#include "TRDDirectory.h"
#include "SinclairDefs.h"

int TRDDirectory::ReadDirectory(void)
{
	Files.clear();

	BYTE Sector[ 256 ];

	DWORD FileID = 0;

	/* Directory is described as 8 setors long, but byte 1 of sector 8 is
	   also described as "end of directory marker" - this plugin favours
	   a fixed directory length, since the disc record is always at sector 8. */
	for ( BYTE sec=0; sec<8; sec++ )
	{
		if ( pSource->ReadSectorCHS( 0, 0, sec, Sector ) != DS_SUCCESS )
		{
			return -1;
		}

		/* Each sector holds 16 x 16byte entries */
		for ( DWORD f=0; f<16; f++ )
		{
			BYTE *pEnt = &Sector[ f * 16 ];

			if ( pEnt[ 0 ] == 0 )
			{
				continue;
			}

			if ( pEnt[ 0 ] == 1 )
			{
				// TODO: Deleted files
				continue;
			}

			NativeFile file;

			file.EncodingID = ENCODING_SINCLAIR;
			file.Filename   = BYTEString( &pEnt[ 0 ], 8 );
			file.Extension  = BYTEString( &pEnt[ 8 ], 1 );
			file.fileID     = FileID;
			file.Flags      = FF_Extension;
			file.FSFileType = FT_SINCLAIR_TRD;
			file.Length     = * (WORD *) &pEnt[ 11 ];
			file.XlatorID   = NULL;
			file.Type       = FT_Arbitrary;
			file.Icon       = FT_Arbitrary;

			file.Attributes[ 0 ] = pEnt[ 14 ]; // Start sector
			file.Attributes[ 1 ] = pEnt[ 15 ]; // Start track
			file.Attributes[ 2 ] = * (WORD *) &pEnt[ 9 ]; // Load address

			file.HasResolvedIcon = false;

			/* Do a little filename tidying */
			for ( BYTE i=7; i != 0xFF; i-- )
			{
				if ( file.Filename[ i ] == 0x20 )
				{
					file.Filename[ i ] = 0;
				}
				else
				{
					break;
				}
			}

			Files.push_back( file );

			FileID++;
		}
	}

	return 0;
}

int TRDDirectory::WriteDirectory(void)
{
	return 0;
}
