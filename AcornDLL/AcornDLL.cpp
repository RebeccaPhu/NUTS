// AcornDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

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
#include "../NUTS/IDE8Source.h"
#include "../NUTS/OffsetDataSource.h"
#include "ADFSDirectoryCommon.h"
#include "../NUTS/NUTSError.h"
#include "IconResolve.h"

#include "resource.h"

BYTE *pAcornFont;
BYTE *pTeletextFont;
BYTE *pRiscOSFont;

DWORD ENCODING_ACORN;
DWORD ENCODING_RISCOS;
DWORD FT_ACORN;
DWORD FT_SPRITE;
DWORD FT_ACORNX;
DWORD GRAPHIC_ACORN;
DWORD GRAPHIC_SPRITE;
DWORD BBCBASIC;

HMODULE hInstance;

DataSourceCollector *pCollector;

BYTE *NUTSSignature;

FSDescriptor BBCMicroFS[] = {
	{
		/* .FriendlyName = */ L"Acorn DFS (40T)",
		/* .PUID         = */ FSID_DFS_40,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_FixedSize,
		256, 256U * 10U * 40U,
		(BYTE *) "SSD"
	},
	{
		/* .FriendlyName = */ L"Acorn DFS (80T)",
		/* .PUID         = */ FSID_DFS_80,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_FixedSize,
		256, 256U * 10U * 80U,
		(BYTE *) "SSD"
	},
	{
		/* .FriendlyName = */ L"Acorn DFS (Double Sided Image)",
		/* .PUID         = */ FSID_DFS_DSD,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize,
		0, 0,
		(BYTE *) "DSD"
	},
	{
		/* .FriendlyName = */ L"Acorn ADFS 160K 40T SS (S)",
		/* .PUID         = */ FSID_ADFS_S,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		256, 256U * 16U * 40U,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"Acorn ADFS 320K 80T SS (M)",
		/* .PUID         = */ FSID_ADFS_M,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		256, 256U * 16U * 80U,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"Acorn ADFS 640K 80T DS (L)",
		/* .PUID         = */ FSID_ADFS_L,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		256, 256U * 16U * 160U,
		(BYTE *) "ADL"
	},
	{
		/* .FriendlyName = */ L"Acorn ADFS Hard Disk (Old Map)",
		/* .PUID         = */ FSID_ADFS_H,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_ArbitrarySize | FSF_UseSectors,
		256, 0,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"Acorn ADFS Hard Disk (8 Bit IDE)",
		/* .PUID         = */ FSID_ADFS_H8,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_ArbitrarySize | FSF_UseSectors,
		256, 0,
		(BYTE *) "ADF"
	}
};

#define BBCMICRO_FSCOUNT ( sizeof(BBCMicroFS) / sizeof( FSDescriptor) )

FSDescriptor RISCOSFS[] = {
	{
		/* .FriendlyName = */ L"RISC OS ADFS 640K (L)",
		/* .PUID         = */ FSID_ADFS_L2,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		1024, 1024U * 5U * 160U,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS 800K (D)",
		/* .PUID         = */ FSID_ADFS_D,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		1024, 1024U * 5U * 160U,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS 800K (E)",
		/* .PUID         = */ FSID_ADFS_E,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		1024, 1024U * 5U * 160U,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS 800K (E+)",
		/* .PUID         = */ FSID_ADFS_EP,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		1024, 1024U * 5U * 160U,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS 1.6M (F)",
		/* .PUID         = */ FSID_ADFS_F,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		1024, 1024U * 10U * 160U,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS 1.6M (F+)",
		/* .PUID         = */ FSID_ADFS_FP,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		1024, 1024U * 10U * 160U,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS 3.2M (G)",
		/* .PUID         = */ FSID_ADFS_G,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_FixedSize,
		1024, 1024U * 20U * 160U,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS Hard Disk (Old Map)",
		/* .PUID         = */ FSID_ADFS_HO,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_ArbitrarySize | FSF_UseSectors,
		256, 0,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS Hard Disk (New Map)",
		/* .PUID         = */ FSID_ADFS_HN,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_ArbitrarySize | FSF_UseSectors,
		256, 0,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"RISC OS ADFS Hard Disk (Long Names)",
		/* .PUID         = */ FSID_ADFS_HP,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Dirs | FSF_ArbitrarySize | FSF_UseSectors,
		256, 0,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"RISC OS Sprite File",
		/* .PUID         = */ FSID_SPRITE,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize,
		0, 0,
		(BYTE *) "SPR"
	}
};

#define RISCOS_FSCOUNT ( sizeof(RISCOSFS) / sizeof( FSDescriptor) )

std::wstring ImageExtensions[] = { L"SSD", L"IMG", L"DSD", L"ADF", L"ADS", L"ADM", L"ADL", L"HDF" };

#define IMAGE_EXT_COUNT ( sizeof(ImageExtensions) / sizeof( std::wstring ) )

DataTranslator Translators[] = {
	{ 0, L"BBC BASIC I - V",   0, TXTextTranslator },
	{ 0, L"Acorn Modes 0 - 7", 0, TXGFXTranslator | GFXLogicalPalette | GFXMultipleModes },
	{ 1, L"RISC OS Sprite",    0, TXGFXTranslator | GFXMultipleModes }
};

#define TRANSLATOR_COUNT ( sizeof(Translators) / sizeof(DataTranslator) )


typedef struct _RISCOSIcon
{
	DWORD RISCOSType;
	FileType NUTSType;
	std::string Name;
	UINT Bitmap;
} RISCOSIcon;

UINT IconBitmapIDs[ 13 ] = { IDB_BASIC, IDB_DATA, IDB_DRAWFILE, IDB_EXEC, IDB_FONT, IDB_JPEG, IDB_MODULE, IDB_OBEY, IDB_SPRITE, IDB_TEXT, IDB_UTIL, IDB_FOLDER, IDB_APP };

RISCOSIcon AcornIcons[] = {
	{ 0xFFB, FT_BASIC,     "BASIC Program", IDB_BASIC    }, 
	{ 0xFFD, FT_Data,      "Data",          IDB_DATA     },
	{ 0xAFF, FT_Graphic,   "Draw File",     IDB_DRAWFILE },
	{ 0xFFE, FT_Script,    "Exec (Spool)",  IDB_EXEC     },
	{ 0xFF7, FT_Arbitrary, "Font",          IDB_FONT     },
	{ 0xC85, FT_Graphic,   "JPEG Image",    IDB_JPEG     },
	{ 0xFFA, FT_Code,      "Module",        IDB_MODULE   },
	{ 0xFEB, FT_Script,    "Obey (Script)", IDB_OBEY     },
	{ 0xFF9, FT_MiscImage, "Sprite",        IDB_SPRITE   },
	{ 0xFFF, FT_Text,      "Text File",     IDB_TEXT     },
	{ 0xFFC, FT_Code,      "Utility",       IDB_UTIL     },
	{ 0xA00, FT_Directory, "Directory",     IDB_FOLDER   },
	{ 0xA01, FT_Directory, "Application",   IDB_APP      }
};

#define NumIcons ( sizeof(AcornIcons) / sizeof(RISCOSIcon) )

ACORNDLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
{
	FileSystem *pFS = nullptr;

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
	case FSID_ADFS_H8:
	case FSID_ADFS_HO:
	case FSID_ADFS_D:
	case FSID_ADFS_L2:
		{
			if ( PUID == FSID_ADFS_HO )
			{
				ADFSFileSystem *pADFS = new ADFSFileSystem( pSource );

				pFS = pADFS;
			}
			else if ( PUID == FSID_ADFS_H8 )
			{
				IDE8Source *pIDE8 = new IDE8Source( pSource );

				ADFSFileSystem *pADFS = new ADFSFileSystem( pIDE8 );

				pFS = pADFS;

				DS_RELEASE( pIDE8 );
			}
			else
			{
				ADFSFileSystem *pADFS = new ADFSFileSystem( pSource );

				pFS = pADFS;
			}
		}

		break;

	case FSID_ADFS_E:
	case FSID_ADFS_F:
	case FSID_ADFS_G:
	case FSID_ADFS_EP:
	case FSID_ADFS_FP:
	case FSID_ADFS_HN:
	case FSID_ADFS_HP:
		{
			pFS = new ADFSEFileSystem( pSource );
		}
		break;

	case FSID_ADFS_L:
		{
			ADFSFileSystem *pADFS = new ADFSFileSystem( pSource );

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

void *CreateTranslator( DWORD TUID )
{
	void *pXlator = nullptr;

	if ( TUID == GRAPHIC_ACORN )
	{
		pXlator = (void *) new AcornSCREENTranslator();
	}
	else if ( TUID == GRAPHIC_SPRITE )
	{
		pXlator = (void *) new SpriteTranslator();
	}
	else if ( TUID == BBCBASIC )
	{
		pXlator = (void *) new BBCBASICTranslator();
	}

	return pXlator;
}

bool TranslateZIPContent( FOPData *fop )
{
	NativeFile *File = (NativeFile *) fop->pFile;

	BYTE *pData = fop->pXAttr;

	/* Translate the extra data in the ZIP (max 64 bytes) if it is present */
	if ( fop->Direction == FOP_ReadEntry )
	{
		/* This is source data */
		WORD Offset = 0;

		while ( Offset < 60 ) // Need at least 4 bytes
		{
			WORD ExID = * (WORD *) &pData[ Offset + 0 ];
			WORD ExLn = * (WORD *) &pData[ Offset + 2 ];

			Offset += 4;

			if ( Offset + ExLn > 64 ) { ExLn = 64 - Offset; }

			/* Need at least 16 bytes */
			if ( ExLn < 16 )
			{
				break;
			}

			if ( ExID == 0x4341 ) /* Got it - "AC" */
			{
				/* Extract the attributes - we are interested in load addr, exec addr and attribute DWORD.
				   Not sure what the 1st DWORD is - seems to always be "ARC0" */

				if ( rstrnicmp( &pData[ Offset ], (BYTE *) "ARC0", 4 ) )
				{
					/* Load + Exec - which are actually forming Timestamp and File type */
					File->LoadAddr = * (DWORD *) &pData[ Offset + 4 ];
					File->ExecAddr = * (DWORD *) &pData[ Offset + 8 ];

					/* Attribute DWORD - Only the first 6 bits are used */
					DWORD Attrs = * (DWORD *) &pData[ Offset + 12 ];

					if ( ( Attrs & 1 ) | ( Attrs & 16 ) ) { File->AttrRead   = 0xFFFFFFFF; } else { File->AttrRead   = 0x00000000; }
					if ( ( Attrs & 2 ) | ( Attrs & 32 ) ) { File->AttrWrite  = 0xFFFFFFFF; } else { File->AttrWrite  = 0x00000000; }
					if   ( Attrs & 4 )                    { File->AttrLocked = 0xFFFFFFFF; } else { File->AttrLocked = 0x00000000; }

					/* Give it icons too */
					ADFSDirectoryCommon adir;

					adir.TranslateType( File );

					/* Change the type for copying purposes */
					File->FSFileType = FT_ACORNX;
					File->EncodingID = ENCODING_RISCOS;

					ResolveAppIcon( (FileSystem *) fop->pFS, (NativeFile *) fop->pFile );

					return true;
				}
			}
		}
	}
	else if ( ( File->FSFileType == FT_ACORN ) || ( File->FSFileType == FT_ACORNX ) )
	{
		ZeroMemory( pData, fop->lXAttr );

		if ( fop->lXAttr >= 0x18 )
		{
			pData[ 0 ] = 0x41;
			pData[ 1 ] = 0x43;
			pData[ 2 ] = 0x18;
			pData[ 3 ] = 0x00;

			rstrncpy( &pData[ 0x04 ], (BYTE *) "ARC0", 4 );

			if ( File->FSFileType == FT_ACORN )
			{
				* (DWORD *) &pData[ 0x08 ] = 0;
				* (DWORD *) &pData[ 0x0C ] = 0;
			}
			else
			{
				* (DWORD *) &pData[ 0x08 ] = File->LoadAddr;
				* (DWORD *) &pData[ 0x0C ] = File->ExecAddr;
			}

			DWORD Attrs = 0;

			if ( File->AttrRead )   { Attrs |= ( 1 | 16 ); }
			if ( File->AttrWrite )  { Attrs |= ( 2 | 32 ); }
			if ( File->AttrLocked ) { Attrs |= 4; }

			* (DWORD *) &pData[ 0x010 ] = Attrs;

			File->FSFileType = FT_ZIP;
			File->Flags     |= FF_AvoidSidecar;

			fop->lXAttr = 0x18;

			return true;
		}
	}

	return false;
}

void LoadFonts()
{
	HRSRC   hResource;
	HGLOBAL hMemory;
	DWORD   dwSize;
	LPVOID  lpAddress;

	if ( pAcornFont == nullptr )
	{
		hResource = FindResource(hInstance, MAKEINTRESOURCE( IDF_ACORN ), RT_RCDATA);
		hMemory   = LoadResource(hInstance, hResource);
		dwSize    = SizeofResource(hInstance, hResource);
		lpAddress = LockResource(hMemory);

		// Font data starts from character 32
		pAcornFont = (BYTE *) malloc( 256 * 8 );

		ZeroMemory( pAcornFont, 256 * 8 );

		memcpy( &pAcornFont[ 32 * 8 ], lpAddress, dwSize );
	}

	if ( pTeletextFont == nullptr )
	{
		hResource = FindResource(hInstance, MAKEINTRESOURCE( IDF_TELETEXT ), RT_RCDATA);
		hMemory   = LoadResource(hInstance, hResource);
		dwSize    = SizeofResource(hInstance, hResource);
		lpAddress = LockResource(hMemory);

		// Font data starts from character 32
		pTeletextFont = (BYTE *) malloc( 256 * 8 );

		ZeroMemory( pTeletextFont, 256 * 8 );

		memcpy( &pTeletextFont[ 0 * 8   ], lpAddress, dwSize / 2 );
		memcpy( &pTeletextFont[ 128 * 8 ], lpAddress, dwSize / 2 );

		for ( BYTE c=0x00; c!=0x20; c++ )
		{
			BYTE *pChar  = &pTeletextFont[ c * 8 ];
			BYTE *pTChar = &pTeletextFont[ ( c + 0x80 ) * 8 ];
			BYTE *pSrc   = &pTeletextFont[ '?' * 8 ];

			for (BYTE r=0; r<8; r++ )
			{
				BYTE cc = pSrc[ r ];

				if ( r == 0 ) { cc |= 0x55; }
				else if ( r == 7 ) { cc |= 0xAA; }
				else if ( r & 1 ) { cc |= 0x80; }
				else { cc |= 0x01; }

				pChar[ r ] = cc;
				pTChar[ r ] = cc;
			}
		}
	}

	if ( pRiscOSFont == nullptr )
	{
		hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_RISCOS ), RT_RCDATA);
		hMemory    = LoadResource(hInstance, hResource);
		dwSize     = SizeofResource(hInstance, hResource);
		lpAddress  = LockResource(hMemory);

		// Font data starts from character 32
		pRiscOSFont = (BYTE *) malloc( 256 * 8 );

		BYTE *pFontSrc = ( BYTE * ) lpAddress;

		ZeroMemory( pRiscOSFont, 256 * 8 );

		memcpy( &pRiscOSFont[ 32 * 8  ], &pFontSrc[ 0 ], ( 256 - 32 ) * 8 );
	}

	AcornSCREENTranslator::pTeletextFont = pTeletextFont;
}

DWORD FontID1 = 0xFFFFFFFF;
DWORD FontID2 = 0xFFFFFFFF;
DWORD FontID3 = 0xFFFFFFFF;

WCHAR *DescribeChar( BYTE Char, DWORD FontID )
{
	/* This is complicated because of which font is in use */
	static WCHAR desc[ 256 ];

	std::wstring cd;

	if ( ( FontID == FontID1 ) && ( Char == 0x60 ) )
	{
		cd = L"Pound sign";
	}
	else if ( FontID == FontID3 )
	{
		/* Teletext */
		BYTE DChar = Char & 0x7F;

		cd = L"";

		if ( Char & 0x80 ) { cd = L"(Top bit set) "; }

		/* There's a few weird characters that look different in telext */
		if ( DChar == 0x60 ) { cd += L"Pound Sign"; }
		if ( DChar == 0x5F ) { cd += L"Long Hyphen"; }
		if ( DChar == 0x5E ) { cd += L"Up Arrow"; }
		if ( DChar == 0x5D ) { cd += L"Right Arrow"; }
		if ( DChar == 0x5C ) { cd += L"Half Fraction"; }
		if ( DChar == 0x5B ) { cd += L"Left Arrow"; }
		if ( DChar == 0x7F ) { cd += L"Hard Space"; }
		if ( DChar == 0x7E ) { cd += L"Division Symbol"; }
		if ( DChar == 0x7D ) { cd += L"Three Quarter Fraction"; }
		if ( DChar == 0x7C ) { cd += L"Double Bar"; }
		if ( DChar == 0x7B ) { cd += L"One Quarter Fraction"; }

		if ( 
			( ( DChar >= 0x20 ) && ( DChar <= 0x5A ) ) ||
			( ( DChar >= 0x61 ) && ( DChar <= 0x7A ) ) 
			)
		{
			WCHAR x[2] = { (WCHAR) DChar, 0 };

			cd += L"ASSCII Character '" + std::wstring( x ) + L"'";
		}

		if ( ( DChar >= 0 ) && ( DChar < 0x20 ) )
		{
			static WCHAR *codes[] = {
				L"No Effect", L"Red Alpha", L"Green Alpha", L"Yellow Alpha", L"Blue Alpha", L"Magenta Alpha", L"Cyan Alpha", L"White Alpha", L"FlashL", L"Steady",
				L"No Effect", L"No Effect", L"Normal Height", L"Double Height", L"No Effect", L"No Effect", L"No Effect",
				L"Red Graphics", L"Green Graphics", L"Yellow Graphics", L"Blue Graphics", L"Magenta Graphics", L"Cyan Graphics", L"White Graphics",
				L"Conceal Display", L"Contiguous Graphics", L"Separated Graphics", L"No Effect", L"Black Background", L"New Background", L"Hold Graphics",
				L"Release Graphics"
			};

			cd += L"Control Code: " + std::wstring( codes[ DChar ] );
		}
	}
	else
	{
		if ( ( Char >= 0 ) && ( Char < 0x20 ) )
		{
			cd = L"Control character " + std::to_wstring( (long double) Char );
		}
		else if ( ( Char >= 0x20 ) && ( Char <= 0x7E ) )
		{
			WCHAR x[2] = { (WCHAR) Char, 0 };

			cd = L"ASSCII Character '" + std::wstring( x ) + L"'";
		}
		else if ( Char == 0x7F )
		{
			cd = L"Delete";
		}
		else
		{
			cd = L"Extended Character " + std::to_wstring( (long double) Char );
		}
	}

	wcscpy( desc, cd.c_str() );

	return desc;
}

NUTSProvider ProviderBBCMicro = { L"BBC Micro", 0, 0 };
NUTSProvider ProviderRISCOS   = { L"RISC OS", 0, 0 };

WCHAR *pBBCMicroFontName = L"BBC Micro";
WCHAR *pRiscOSFontName   = L"Risc OS";
WCHAR *pTTXFontName      = L"Teletext";

static WCHAR *PluginAuthor = L"Rebecca Gellman";

ACORNDLL_API int NUTSCommandHandler( PluginCommand *cmd )
{
	switch ( cmd->CommandID )
	{
	case PC_SetPluginConnectors:
		pCollector   = (DataSourceCollector *) cmd->InParams[ 0 ].pPtr;
		pGlobalError = (NUTSError *)           cmd->InParams[ 1 ].pPtr;
		
		LoadFonts();

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportProviders:
		cmd->OutParams[ 0 ].Value = 2;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetProviderDescriptor:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &ProviderBBCMicro;
			return NUTS_PLUGIN_SUCCESS;
		}
		if ( cmd->InParams[ 0 ].Value == 1 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &ProviderRISCOS;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;
		
	case PC_GetOffsetLists:
		{
			cmd->OutParams[ 0 ].Value = 0;

			BYTE pid = (BYTE) cmd->InParams[ 0 ].Value;

			if ( pid == 1 )
			{
				BYTE fsid = (BYTE) cmd->InParams[ 1 ].Value;

				if ( ( fsid == 7 ) || ( fsid == 8 ) || ( fsid == 9 ) )
				{
					cmd->OutParams[ 0 ].Value = 2;
					cmd->OutParams[ 1 ].Value = 0;
					cmd->OutParams[ 2 ].Value = 0x200;
				}
			}
		}
		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportFileSystems:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].Value = BBCMICRO_FSCOUNT;
			return NUTS_PLUGIN_SUCCESS;
		}
		if ( cmd->InParams[ 0 ].Value == 1 )
		{
			cmd->OutParams[ 0 ].Value = RISCOS_FSCOUNT;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_DescribeFileSystem:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &BBCMicroFS[ cmd->InParams[ 1 ].Value ];
			return NUTS_PLUGIN_SUCCESS;
		}
		if ( cmd->InParams[ 0 ].Value == 1 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &RISCOSFS[ cmd->InParams[ 1 ].Value ];
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_ReportImageExtensions:
		cmd->OutParams[ 0 ].Value = IMAGE_EXT_COUNT;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetImageExtension:
		cmd->OutParams[ 0 ].pPtr = (void *) ImageExtensions[ cmd->InParams[ 0 ].Value ].c_str();
		cmd->OutParams[ 1 ].Value = FT_MiscImage;
		cmd->OutParams[ 2 ].Value = FT_DiskImage;

		if ( cmd->InParams[ 0 ].Value == IMAGE_EXT_COUNT - 1 )
		{
			cmd->OutParams[ 2 ].Value = FT_HardImage;
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_CreateFileSystem:
		{
			DataSource *pSource = (DataSource *) cmd->InParams[ 2 ].pPtr;

			DWORD ProviderID = cmd->InParams[ 0 ].Value;
			DWORD FSID       = cmd->InParams[ 1 ].Value;

			DWORD FullFSID = MAKEFSID( 0, ProviderID, FSID );

			void *pFS = (void *) CreateFS( FullFSID, pSource );

			cmd->OutParams[ 0 ].pPtr = pFS;

			if ( pFS == nullptr )
			{
				return NUTS_PLUGIN_ERROR;
			}

			return NUTS_PLUGIN_SUCCESS;
		}

	case PC_ReportEncodingCount:
		cmd->OutParams[ 0 ].Value = 2;

		return NUTS_PLUGIN_SUCCESS;

	case PC_SetEncodingBase:
		ENCODING_ACORN  = cmd->InParams[ 0 ].Value + 0;
		ENCODING_RISCOS = cmd->InParams[ 0 ].Value + 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportFonts:
		cmd->OutParams[ 0 ].Value = 3;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetFontPointer:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr  = (void *) pAcornFont;
			cmd->OutParams[ 1 ].pPtr  = (void *) pBBCMicroFontName;
			cmd->OutParams[ 2 ].Value = ENCODING_ACORN;
			cmd->OutParams[ 3 ].Value = NULL;

			FontID1 = cmd->InParams[ 1 ].Value;

			return NUTS_PLUGIN_SUCCESS;
		}
		if ( cmd->InParams[ 0 ].Value == 1 )
		{
			cmd->OutParams[ 0 ].pPtr  = (void *) pRiscOSFont;
			cmd->OutParams[ 1 ].pPtr  = (void *) pRiscOSFontName;
			cmd->OutParams[ 2 ].Value = ENCODING_RISCOS;
			cmd->OutParams[ 3 ].Value = NULL;

			FontID2 = cmd->InParams[ 1 ].Value;

			return NUTS_PLUGIN_SUCCESS;
		}

		if ( cmd->InParams[ 0 ].Value == 2 )
		{
			cmd->OutParams[ 0 ].pPtr  = (void *) pTeletextFont;
			cmd->OutParams[ 1 ].pPtr  = (void *) pTTXFontName;
			cmd->OutParams[ 2 ].Value = ENCODING_ACORN;
			cmd->OutParams[ 3 ].Value = ENCODING_RISCOS;
			cmd->OutParams[ 4 ].Value = NULL;

			FontID3 = cmd->InParams[ 1 ].Value;

			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_ReportIconCount:
		cmd->OutParams[ 0 ].Value = NumIcons;

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeIcon:
		{
			BYTE Icon   = (BYTE) cmd->InParams[ 0 ].Value;
			UINT IconID = (UINT) cmd->InParams[ 1 ].Value;

			HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( AcornIcons[ Icon ].Bitmap ) );

			RISCOSIcons::AddIconMaps(
				IconID,
				AcornIcons[ Icon ].RISCOSType,
				AcornIcons[ Icon ].NUTSType,
				AcornIcons[ Icon ].Name,
				bmp
			);

			cmd->OutParams[ 0 ].pPtr = (void *) bmp;
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportFSFileTypeCount:
		cmd->OutParams[ 0 ].Value = 3;

		return NUTS_PLUGIN_SUCCESS;

	case PC_SetFSFileTypeBase:
		{
			DWORD Base = cmd->InParams[ 0 ].Value;

			FT_ACORN  = Base + 0;
			FT_SPRITE = Base + 1;
			FT_ACORNX = Base + 2;
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportTranslators:
		cmd->OutParams[ 0 ].Value = TRANSLATOR_COUNT;

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeTranslator:
		{
			BYTE tx = (BYTE) cmd->InParams[ 0 ].Value;

			Translators[ tx ].TUID = cmd->InParams[ 1 ].Value;

			cmd->OutParams[ 0 ].pPtr  = (void *) Translators[ tx ].FriendlyName.c_str();
			cmd->OutParams[ 1 ].Value = Translators[ tx ].Flags;
			cmd->OutParams[ 2 ].Value = Translators[ tx ].ProviderID;

			BBCBASIC       = MAKEFSID( cmd->InParams[ 2 ].Value, 0, Translators[ 0 ].TUID );
			GRAPHIC_ACORN  = MAKEFSID( cmd->InParams[ 2 ].Value, 0, Translators[ 1 ].TUID );
			GRAPHIC_SPRITE = MAKEFSID( cmd->InParams[ 2 ].Value, 1, Translators[ 2 ].TUID );
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_CreateTranslator:
		cmd->OutParams[ 0 ].pPtr = CreateTranslator( cmd->InParams[ 0 ].Value );

		return NUTS_PLUGIN_SUCCESS;

	case PC_TranslateFOPContent:
		{
			FOPData *fop = (FOPData *) cmd->InParams[ 0 ].pPtr;

			if ( fop->DataType == FOP_DATATYPE_ZIPATTR )
			{
				bool r = TranslateZIPContent( fop );

				if ( r ) { cmd->OutParams[ 0 ].Value = 0xFFFFFFFF; } else { cmd->OutParams[ 0 ].Value = 0x00000000; }

				return NUTS_PLUGIN_SUCCESS;
			}
		}
		
		break;

	case PC_DescribeChar:
		{
			BYTE  Char   = (BYTE) cmd->InParams[ 1 ].Value;
			DWORD FontID =        cmd->InParams[ 0 ].Value;

			cmd->OutParams[ 0 ].pPtr = DescribeChar( Char, FontID );
		}
		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportPluginCreditStats:
		if ( PROVIDERID( cmd->InParams[ 0 ].Value ) == 0 )
		{
			cmd->OutParams[ 0 ].pPtr  = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_BBCMICRO ) );
			cmd->OutParams[ 1 ].pPtr  = (void *) PluginAuthor;
			cmd->OutParams[ 2 ].Value = 0;
			cmd->OutParams[ 3 ].Value = 0;

			return NUTS_PLUGIN_SUCCESS;
		}

		if ( PROVIDERID( cmd->InParams[ 0 ].Value ) == 1 )
		{
			cmd->OutParams[ 0 ].pPtr  = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_RISCOS ) );
			cmd->OutParams[ 1 ].pPtr  = (void *) PluginAuthor;
			cmd->OutParams[ 2 ].Value = 15;
			cmd->OutParams[ 3 ].Value = 1;

			return NUTS_PLUGIN_SUCCESS;
		}

		break;

	case PC_GetPluginCredits:
		if ( PROVIDERID( cmd->InParams[ 0 ].Value ) ==  1 )
		{
			static WCHAR *pGerald = L"Gerald Holdsworth: RiscOS Sprite and ADFS New Map/Big Directory descriptions. See http://www.geraldholdsworth.co.uk";

			cmd->OutParams[ 0 ].pPtr = (void *) pGerald;

			return NUTS_PLUGIN_SUCCESS;
		}
		break;

	case PC_GetIconLicensing:
		{
			static WCHAR *pIcons[] =
			{
				L"Application icon", L"BASIC Program Icon", L"Data File Icon", L"Draw File Icon", L"Exec File Icon", L"Directory Icon", L"Font Icon", L"JPEG Icon",
				L"Module Icon", L"Obey File Icon", L"Sprite File Icon", L"Text File Icon", L"Utility File Icon",

				L"Repair Icon", L"Moving and Packing 02 Brown Icon"
			};
			
			static int iIcons[] = {
				IDB_APP, IDB_BASIC, IDB_DATA, IDB_DRAWFILE, IDB_EXEC, IDB_FOLDER, IDB_FONT, IDB_JPEG, IDB_MODULE, IDB_OBEY, IDB_SPRITE, IDB_TEXT, IDB_UTIL,

				IDB_REPAIR, IDB_COMPACT
			};
			
			static WCHAR *pIconset = L"Risc OS Icons";
			static WCHAR *pAuthor  = L"Acorn Computers";
			static WCHAR *pFair    = L"Used under fair use for familiarity purposes";
			static WCHAR *pEmpty   = L"";

			static WCHAR *pIconset2 = L"Large Android Icons";
			static WCHAR *pIconset3 = L"Moving and Packing Icon Set";

			static WCHAR *pAuthor2  = L"Aha-Soft";
			static WCHAR *pAuthor3  = L"My Moving Reviews";

			static WCHAR *pLicense2 = L"CC Attribution 3.0 US";
			static WCHAR *pLicense3 = L"Linkware";

			static WCHAR *pURL2     = L"https://creativecommons.org/licenses/by/3.0/us/";
			static WCHAR *pURL3     = L"http://www.mymovingreviews.com/move/webmasters/moving-company-icon-sets";

			cmd->OutParams[ 0 ].pPtr = pIcons[ cmd->InParams[ 0 ].Value ];
			cmd->OutParams[ 1 ].pPtr = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( iIcons[ cmd->InParams[ 0 ].Value ] ) );
			
			if ( cmd->InParams[ 0 ].Value == 13 )
			{
				cmd->OutParams[ 2 ].pPtr = pIconset2;
				cmd->OutParams[ 3 ].pPtr = pAuthor2;
				cmd->OutParams[ 4 ].pPtr = pLicense2;
				cmd->OutParams[ 5 ].pPtr = pURL2;
			}
			else if ( cmd->InParams[ 0 ].Value == 14 )
			{
				cmd->OutParams[ 2 ].pPtr = pIconset3;
				cmd->OutParams[ 3 ].pPtr = pAuthor3;
				cmd->OutParams[ 4 ].pPtr = pLicense3;
				cmd->OutParams[ 5 ].pPtr = pURL3;
			}
			else
			{
				cmd->OutParams[ 2 ].pPtr = pIconset;
				cmd->OutParams[ 3 ].pPtr = pAuthor;
				cmd->OutParams[ 4 ].pPtr = pFair;
				cmd->OutParams[ 5 ].pPtr = pEmpty;
			}
		}
		return NUTS_PLUGIN_SUCCESS;
	}

	return NUTS_PLUGIN_UNRECOGNISED;
}