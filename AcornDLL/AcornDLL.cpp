// AcornDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AcornDLL.h"

#include "Defs.h"

#include "ADFSFileSystem.h"
#include "ADFSEFileSystem.h"
#include "AcornDFSFileSystem.h"
#include "AcornDFSDSD.h"
#include "SpriteFile.h"
#include "../nuts/IDE8Source.h"
#include "AcornSCREENTranslator.h"
#include "SpriteTranslator.h"
#include "BBCBASICTranslator.h"
#include "../NUTS/Defs.h"
#include "RISCOSIcons.h"

#include "resource.h"

BYTE *pAcornFont;
BYTE *pTeletextFont;

HMODULE hInstance;

FSDescriptor AcornFS[13] = {
	{
		/* .FriendlyName = */ L"Acorn DFS (40T)",
		/* .PUID         = */ FSID_DFS_40,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_FixedSize,
		0, { }, { },
		2, { L"SSD", L"IMG" }, { FT_MiscImage, FT_MiscImage }, { FT_DiskImage, FT_DiskImage },
		256, 256U * 10U * 40U
	},
	{
		/* .FriendlyName = */ L"Acorn DFS (80T)",
		/* .PUID         = */ FSID_DFS_80,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_FixedSize,
		0, { }, { },
		0, { }, { }, { },
		256, 256U * 10U * 80U
	},
	{
		/* .FriendlyName = */ L"Acorn DFS (Double Sided Image)",
		/* .PUID         = */ FSID_DFS_DSD,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize,
		0, { }, { },
		1, { L"DSD" }, { FT_MiscImage }, { FT_DiskImage },
		0
	},
	{
		/* .FriendlyName = */ L"Acorn ADFS 160K 40T SS (S)",
		/* .PUID         = */ FSID_ADFS_S,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		0, { }, { },
		2, { L"ADF", L"ADL" }, { FT_MiscImage, FT_MiscImage }, { FT_DiskImage, FT_DiskImage },
		256, 256U * 16U * 40U
	},
	{
		/* .FriendlyName = */ L"Acorn ADFS 320K 80T SS (M)",
		/* .PUID         = */ FSID_ADFS_M,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		0, { }, { },
		0, { }, { }, { },
		256, 256U * 16U * 80U
	},
	{
		/* .FriendlyName = */ L"Acorn ADFS 640K 80T DS (L)",
		/* .PUID         = */ FSID_ADFS_L,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		0, { }, { },
		0, { }, { }, { },
		256, 256U * 16U * 160U
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS 640K (L)",
		/* .PUID         = */ FSID_ADFS_L2,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		0, { }, { },
		0, { }, { }, { },
		1024, 1024U * 5U * 80U
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS 800K (D)",
		/* .PUID         = */ FSID_ADFS_D,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		0, { }, { },
		0, { }, { }, { },
		1024, 1024U * 5U * 80U
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS 800K (E)",
		/* .PUID         = */ FSID_ADFS_E,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		13, { }, { },
		0, { }, { }, { },
		1024, 1024U * 5U * 80U
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS 1.6M (F)",
		/* .PUID         = */ FSID_ADFS_F,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		0, { }, { },
		0, { }, { }, { },
		1024, 1024U * 10U * 80U
	},
	{
		/* .FriendlyName = */ L"Acorn ADFS Hard Disk (Old Map)",
		/* .PUID         = */ FSID_ADFS_H,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_ArbitrarySize | FSF_UseSectors,
		0, { }, { },
		0, { }, { }, { },
		256, 0
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS Hard Disk (New Map)",
		/* .PUID         = */ FSID_ADFS_HN,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_ArbitrarySize | FSF_UseSectors,
		0, { }, { },
		0, { }, { }, { },
		256, 0
	},
	{
		/* .FriendlyName = */ L"RISC OS Sprite File",
		/* .PUID         = */ FSID_SPRITE,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize,
		0, { }, { },
		0, { }, { }, { },
		0
	}
};

FontDescriptor AcornFonts[2] =
	{
		{
		/* .FriendlyName  = */ L"Acorn",
		/* .PUID          = */ FONTID_ACORN,
		/* .Flags         = */ 0,
		/* .MinChar       = */ 32,
		/* .MaxChar       = */ 255,
		/* .ProcessableControlCodes = */ { 21, 6, 0xFF },
		/* .pFontData     = */ NULL,
		/* .MatchingFSIDs = */ {
			ENCODING_ACORN,
			0,
			},
		},
		{
		/* .FriendlyName  = */ L"Teletext",
		/* .PUID          = */ FONTID_TELETEXT,
		/* .Flags         = */ 0,
		/* .MinChar       = */ 0,
		/* .MaxChar       = */ 255,
		/* .ProcessableControlCodes = */ { 21, 6, 0xFF },
		/* .pFontData     = */ NULL,
		/* .MatchingFSIDs = */ {
			ENCODING_ACORN,
			0,
			}
		}
};

TextTranslator BBCBASICTXT[] = {
	{
		L"BBC BASIC I - V",
		BBCBASIC,
		0
	}
};


GraphicTranslator AcornGFX[] = {
	{
		L"Acorn Modes 0-7",
		GRAPHIC_ACORN,
		GFXLogicalPalette | GFXMultipleModes
	},
	{
		L"RISC OS Sprite",
		GRAPHIC_SPRITE,
		GFXMultipleModes
	}
};

PluginDescriptor AcornDescriptor = {
	/* .Provider = */ L"Acorn",
	/* .PUID     = */ PLUGINID_ACORN,
	/* .NumFS    = */ 13,
	/* .NumFonts = */ 2,
	/* .BASXlats = */ 1,
	/* .GFXXlats = */ 2,

	/* .FSDescriptors  = */ AcornFS,
	/* .FontDescriptor = */ AcornFonts,
	/* .BASICXlators   = */ BBCBASICTXT,
	/* .GFXXlators     = */ AcornGFX
};

ACORNDLL_API PluginDescriptor *GetPluginDescriptor(void)
{
	if ( pAcornFont == nullptr )
	{
		HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_ACORN ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hInstance, hResource);
		DWORD dwSize     = SizeofResource(hInstance, hResource);
		LPVOID lpAddress = LockResource(hMemory);

		// Font data starts from character 32
		pAcornFont = (BYTE *) malloc( (size_t) dwSize + ( 32 * 8 ) );

		memcpy( &pAcornFont[ 32 * 8 ], lpAddress, dwSize );

		AcornFonts[0].pFontData = pAcornFont;

		hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_TELETEXT ), RT_RCDATA);
		hMemory    = LoadResource(hInstance, hResource);
		dwSize     = SizeofResource(hInstance, hResource);
		lpAddress  = LockResource(hMemory);

		// Font data starts from character 32
		pTeletextFont = (BYTE *) malloc( (size_t) dwSize );

		memcpy( &pTeletextFont[ 0 * 8   ], lpAddress, dwSize / 2 );
		memcpy( &pTeletextFont[ 128 * 8 ], lpAddress, dwSize / 2 );

		AcornFonts[0].pFontData = pAcornFont;
		AcornFonts[1].pFontData = pTeletextFont;

		AcornSCREENTranslator::pTeletextFont = pTeletextFont;

		UINT IconBitmapIDs[ 13 ] = { IDB_BASIC, IDB_DATA, IDB_DRAWFILE, IDB_EXEC, IDB_FONT, IDB_JPEG, IDB_MODULE, IDB_OBEY, IDB_SPRITE, IDB_TEXT, IDB_UTIL, IDB_FOLDER, IDB_APP };

		for ( BYTE i=0; i<13; i++ )
		{
			HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( IconBitmapIDs[ i ] ) );

			AcornDescriptor.FSDescriptors[ 8 ].Icons[ i ] = (void *) bmp;
		}
	}

	return &AcornDescriptor;
}

void SetRISCOSIcons( void ) 
{
	/* Refresh the icon store with the IDs assigned to us by NUTS */
	DWORD    FileTypes[ 13 ] = { 0xFFB, 0xFFD, 0xAFF, 0xFFE, 0xFF7, 0xC85, 0xFFA, 0xFEB, 0xFF9, 0xFFF, 0xFFC, 0xA00, 0xA01 };
	FileType IntTypes[  13 ] = { FT_BASIC, FT_Data, FT_Graphic, FT_Script, FT_Arbitrary, FT_Graphic, FT_Code, FT_Script, FT_MiscImage, FT_Text, FT_Code, FT_Directory, FT_Directory };
	std::string Names[  13 ] = { "BASIC Program", "Data", "Draw File", "Exec (Spool)", "Font", "JPEG Image", "Module", "Obey (Script)", "Sprite", "Text File", "Utility", "Directory", "Application" };

	for ( BYTE i=0; i<13; i++ )
	{
		RISCOSIcons::AddIconMaps(
			AcornDescriptor.FSDescriptors[ 8 ].IconIDs[ i ],
			FileTypes[ i ],
			IntTypes[ i ],
			Names[ i ],
			(HBITMAP) AcornDescriptor.FSDescriptors[ 8 ].Icons[ i ]
		);
	}
}

ACORNDLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
{
	FileSystem *pFS = nullptr;

	SetRISCOSIcons();

	switch ( PUID )
	{
	case FSID_DFS_40:
	case FSID_DFS_80:
		pFS = new AcornDFSFileSystem( pSource );
		break;

	case FSID_DFS_DSD:
		pFS = new AcornDFSDSD( pSource );
		break;

	case FSID_ADFS_S:
	case FSID_ADFS_M:
	case FSID_ADFS_H:
	case FSID_ADFS_D:
	case FSID_ADFS_L2:
		{
			ADFSFileSystem *pADFS = new ADFSFileSystem( pSource );

			if ( PUID == FSID_ADFS_D )
			{
				pADFS->SetDFormat();
			}

			pFS = pADFS;
		}

		break;

	case FSID_ADFS_E:
	case FSID_ADFS_F:
	case FSID_ADFS_HN:
		{
			pFS = new ADFSEFileSystem( pSource );
		}
		break;

	case FSID_ADFS_L:
		{
			ADFSFileSystem *pADFS = new ADFSFileSystem( pSource );

			pADFS->SetLFormat();

			pFS = pADFS;
		}
		break;

	case FSID_SPRITE:
		{
			SpriteFile *pSprite = new SpriteFile( pSource );

			pFS = pSprite;
		}
		break;
	};

	pFS->FSID = PUID;

	return pFS;
}

ACORNDLL_API void *CreateTranslator( DWORD TUID )
{
	void *pXlator = nullptr;

	if ( TUID == GRAPHIC_ACORN )
	{
		pXlator = (SCREENTranslator *) new AcornSCREENTranslator();
	}
	else if ( TUID == GRAPHIC_SPRITE )
	{
		pXlator = (SCREENTranslator *) new SpriteTranslator();
	}
	else if ( TUID == BBCBASIC )
	{
		pXlator = (TEXTTranslator *) new BBCBASICTranslator();
	}

	return pXlator;
}