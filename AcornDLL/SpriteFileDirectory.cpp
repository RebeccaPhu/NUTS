#include "StdAfx.h"
#include "SpriteFileDirectory.h"
#include "RISCOSIcons.h"
#include "Defs.h"
#include "../nuts/libfuncs.h"

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
		if ( ( SpriteOffset + 16 ) > MaxFile )
		{
			return 0;
		}

		BYTE SpriteHeader[16];

		pSource->ReadRaw( SpriteOffset, 16, SpriteHeader );

		DWORD NextSprite = * (DWORD *) &SpriteHeader[ 0 ];

		NativeFile file;

		file.EncodingID = ENCODING_ACORN;
		file.fileID     = SpriteNum;
		file.FSFileType = FT_SPRITE;
		file.XlatorID   = GRAPHIC_SPRITE;
		file.Type       = FT_Graphic;
		file.Icon       = RISCOSIcons::GetIconForType( 0xFF9 );
		file.Length     = NextSprite;

		file.HasResolvedIcon = UseResolvedIcons;

		file.Attributes[ 0 ] = SpriteOffset;
		file.Filename[  13 ] = 0;

		rstrncpy( file.Filename, &SpriteHeader[ 4 ], 12 );

		Files.push_back( file );

		SpriteOffset += NextSprite;
	}

	return 0;
}

int	SpriteFileDirectory::WriteDirectory(void)
{

	return 0;
}
