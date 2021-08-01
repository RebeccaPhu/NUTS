#include "StdAfx.h"
#include "SpriteFileDirectory.h"
#include "RISCOSIcons.h"
#include "Defs.h"
#include "../nuts/libfuncs.h"
#include "Sprite.h"

int	SpriteFileDirectory::ReadDirectory( void ) {

	BYTE Header[ 16 ];

	Files.clear();
	ResolvedIcons.clear();

	pSource->ReadRaw( 0, 16, Header );

	DWORD NumSprites   = * (DWORD *) &Header[ 0 ];
	DWORD SpriteOffset = * (DWORD *) &Header[ 4 ];

	DWORD MaxFile = (DWORD) pSource->PhysicalDiskSize;

	/* The sprite file is a save copy of the RISC OS Sprite area, less the preceeding Area Size DWORD */
	SpriteOffset -= 4;

	for ( DWORD SpriteNum = 0; SpriteNum < NumSprites; SpriteNum++ )
	{
		if ( ( SpriteOffset + 0x2C ) > MaxFile )
		{
			return 0;
		}

		BYTE SpriteHeader[ 0x2C ];

		pSource->ReadRaw( SpriteOffset, 0x2C, SpriteHeader );

		DWORD NextSprite = * (DWORD *) &SpriteHeader[ 0 ];

		NativeFile file;

		file.EncodingID = ENCODING_RISCOS;
		file.fileID     = SpriteNum;
		file.FSFileType = FT_SPRITE;
		file.XlatorID   = GRAPHIC_SPRITE;
		file.Type       = FT_Graphic;
		file.Icon       = RISCOSIcons::GetIconForType( 0xFF9 );
		file.Length     = NextSprite;
		file.Flags      = 0;

		file.HasResolvedIcon = UseResolvedIcons;

		file.Attributes[ 0 ] = SpriteOffset;

		file.Filename = BYTEString( &SpriteHeader[ 4 ], 12 );

		/* Fill in some attributes. None editable, but informational to the user */
		DWORD WidthWords = * (DWORD *) &SpriteHeader[ 0x10 ];
		DWORD LeftBit    = * (DWORD *) &SpriteHeader[ 0x18 ];
		DWORD RightBit   = * (DWORD *) &SpriteHeader[ 0x1C ];
		DWORD Mode       = * (DWORD *) &SpriteHeader[ 0x28 ];

		if ( Mode > 53 ) { Mode = 0; }

		file.Attributes[ 1 ] = Mode;
		file.Attributes[ 4 ] = Sprite::BPPs[ Mode ];
		file.Attributes[ 2 ] = ( ( WidthWords * 32 ) + RightBit + 1 - LeftBit ) / file.Attributes[ 4 ];
		file.Attributes[ 3 ] = * (DWORD *) &SpriteHeader[ 0x14 ];

		Files.push_back( file );

		SpriteOffset += NextSprite;
	}

	return 0;
}

int	SpriteFileDirectory::WriteDirectory(void)
{

	return 0;
}
