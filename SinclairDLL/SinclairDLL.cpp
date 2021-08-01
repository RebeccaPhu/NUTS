// SINCLAIRDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "SinclairDLL.h"

#include "SinclairDefs.h"
#include "../NUTS/Defs.h"
#include "resource.h"

#include "TAPFileSystem.h"
#include "SinclairTZXFileSystem.h"
#include "DOS3FileSystem.h"
#include "TRDFileSystem.h"
#include "../NUTS/TZXIcons.h"
#include "SpectrumSCREENTranslator.h"
#include "SpectrumBASICTranslator.h"
#include "../NUTS/NUTSError.h"

BYTE *pSinclairFont;
BYTE *pTeletextFont;

HMODULE hInstance;

SINCLAIRDLL_API DataSourceCollector *pExternCollector;
SINCLAIRDLL_API NUTSError *pExternError;
DataSourceCollector *pCollector;

FSDescriptor SinclairFS[7] = {
	{
		/* .FriendlyName = */ L"ZX Spectrum TAP Tape Image",
		/* .PUID         = */ FSID_SPECTRUM_TAP,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize | FSF_Reorderable,
		0, { }, { },
		1, { L"TAP" }, { FT_MiscImage }, { FT_TapeImage },
		0, 0
	},
	{
		/* .FriendlyName = */ L"ZX Spectrum TZX Tape Image",
		/* .PUID         = */ FSID_SPECTRUM_TZX,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize | FSF_Reorderable,
		25, { 0 }, { 0 },
		1, { L"TZX" }, { FT_MiscImage }, { FT_TapeImage },
		0, 0
	},
	{
		/* .FriendlyName = */ L"ZX Spectrum +3DOS",
		/* .PUID         = */ FSID_DOS3,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Uses_DSK,
		0, { }, { },
		1, { L"DSK" }, { FT_MiscImage }, { FT_DiskImage },
		0,
	},
	{
		/* .FriendlyName = */ L"TR-DOS 80T Double Sided",
		/* .PUID         = */ FSID_TRD_80DS,
		/* .Flags        = */ FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		0, { }, { },
		1, { L"TRD" }, { FT_MiscImage }, { FT_DiskImage },
		256, 256 * 16 * 80 * 2
	},
	{
		/* .FriendlyName = */ L"TR-DOS 40T Double Sided",
		/* .PUID         = */ FSID_TRD_40DS,
		/* .Flags        = */ FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		0, { }, { },
		1, { L"TRD" }, { FT_MiscImage }, { FT_DiskImage },
		256, 256 * 16 * 40 * 2
	},
	{
		/* .FriendlyName = */ L"TR-DOS 80T Single Sided",
		/* .PUID         = */ FSID_TRD_80SS,
		/* .Flags        = */ FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		0, { }, { },
		1, { L"TRD" }, { FT_MiscImage }, { FT_DiskImage },
		256, 256 * 16 * 80 * 1
	},
	{
		/* .FriendlyName = */ L"TR-DOS 40T Single Sided",
		/* .PUID         = */ FSID_TRD_40SS,
		/* .Flags        = */ FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		0, { }, { },
		1, { L"TRD" }, { FT_MiscImage }, { FT_DiskImage },
		256, 256 * 16 * 40 * 1
	}
};

FontDescriptor SinclairFonts[2] =
	{
		{
		/* .FriendlyName  = */ L"Sinclair",
		/* .PUID          = */ FONTID_SINCLAIR,
		/* .Flags         = */ 0,
		/* .MinChar       = */ 32,
		/* .MaxChar       = */ 255,
		/* .ProcessableControlCodes = */ { 21, 6, 0xFF },
		/* .pFontData     = */ NULL,
		/* .MatchingFSIDs = */ {
			ENCODING_SINCLAIR,
			0,
			},
		}
};

TextTranslator SpectrumBASIC[] = {
	{
		L"ZX Spectrum BASIC",
		BASIC_SPECTRUM,
		0
	}
};


GraphicTranslator SpectrumGFX[] = {
	{
		L"ZX Spectrum",
		GRAPHIC_SPECTRUM,
		0
	}
};

PluginDescriptor SinclairDescriptor = {
	/* .Provider = */ L"Sinclair",
	/* .PUID     = */ PLUGINID_SINCLAIR,
	/* .NumFS    = */ 7,
	/* .NumFonts = */ 1,
	/* .BASXlats = */ 1,
	/* .GFXXlats = */ 1,
	/* .NumHooks = */ 0,
	/* .Commands = */ 0,

	/* .FSDescriptors  = */ SinclairFS,
	/* .FontDescriptor = */ SinclairFonts,
	/* .BASICXlators   = */ SpectrumBASIC,
	/* .GFXXlators     = */ SpectrumGFX,
	/* .RootHooks      = */ nullptr,
	/* .Commands       = */ { },
};

SINCLAIRDLL_API PluginDescriptor *GetPluginDescriptor(void)
{
	/* Do this because the compiler is too stupid to do a no-op converstion without having it's hand held */
	pCollector   = pExternCollector;
	pGlobalError = pExternError;

	if ( pSinclairFont == nullptr )
	{
		HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_SINCLAIRFONT ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hInstance, hResource);
		DWORD dwSize     = SizeofResource(hInstance, hResource);
		LPVOID lpAddress = LockResource(hMemory);

		// Font data starts from character 32
		pSinclairFont = (BYTE *) malloc( (size_t) dwSize + ( 32 * 8 ) );

		memcpy( &pSinclairFont[ 32 * 8 ], lpAddress, dwSize );

		SinclairFonts[0].pFontData = pSinclairFont;
	}

	if ( SinclairFS[ 1 ].Icons[ 0 ] == 0 )
	{
		UINT IconBitmapIDs[ 25 ] = {
			IDB_STDDATA, IDB_TURBODATA, IDB_PURETONE,  IDB_PULSESEQ,
			IDB_PDATA,   IDB_DIRECT,    IDB_CSW,       IDB_GENERAL,
			IDB_PAUSE,   IDB_GSTART,    IDB_GEND,      IDB_JUMP,
			IDB_LSTART,  IDB_LEND,      IDB_CALL,      IDB_RETURN,
			IDB_SELECT,  IDB_STOP48,    IDB_SETSIGNAL, IDB_DESC,
			IDB_MESSAGE, IDB_INFO,      IDB_HARDWARE,  IDB_CUSTOM,
			IDB_GLUE
		};

		for ( BYTE i=0; i<25; i++ )
		{
			HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( IconBitmapIDs[ i ] ) );

			SinclairFS[ 1 ].Icons[ i ] = (void *) bmp;
		}
	}

	return &SinclairDescriptor;
}

SINCLAIRDLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
{
	void *pFS = NULL;

	if ( IconMap.size() == 0 )
	{
		BYTE BlockIDs[25] = {
			0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x18, 0x19,
			0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
			0x28, 0x2A, 0x2B, 0x30, 0x31, 0x32, 0x33, 0x35,
			0x5A
		};

		for ( BYTE i=0; i<25; i++ )
		{
			IconMap[ BlockIDs[ i ] ] = SinclairFS[ 1 ].IconIDs[ i ];
		}
	}

	switch ( PUID )
	{
	case FSID_SPECTRUM_TAP:
		pFS = (void *) new TAPFileSystem( pSource );
		break;
	case FSID_SPECTRUM_TZX:
		pFS = (void *) new SinclairTZXFileSystem( pSource );
		break;
	case FSID_DOS3:
		pFS = (void *) new DOS3FileSystem( pSource );
		break;

	case FSID_TRD_80SS:
	case FSID_TRD_40SS:
	case FSID_TRD_80DS:
	case FSID_TRD_40DS:
		{
			TRDFileSystem *pTRD = new TRDFileSystem( pSource );
			pFS = (void *) pTRD;

			pTRD->FSID = PUID;
		}
		break;
	};

	return pFS;
}

SINCLAIRDLL_API void *CreateTranslator( DWORD TUID )
{
	void *pXlator = nullptr;

	if ( TUID == GRAPHIC_SPECTRUM )
	{
		pXlator = (SCREENTranslator *) new SpectrumSCREENTranslator();
	}

	if ( TUID == BASIC_SPECTRUM )
	{
		pXlator = (SCREENTranslator *) new SpectrumBASICTranslator();
	}

	return pXlator;
}