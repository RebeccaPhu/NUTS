// SINCLAIRDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "SinclairDLL.h"

#include "SinclairDefs.h"
#include "../NUTS/Defs.h"
#include "resource.h"

#include "TAPFileSystem.h"
#include "DOS3FileSystem.h"
#include "SpectrumSCREENTranslator.h"
#include "SpectrumBASICTranslator.h"
#include "../NUTS/NUTSError.h"

BYTE *pSinclairFont;
BYTE *pTeletextFont;

HMODULE hInstance;

SINCLAIRDLL_API DataSourceCollector *pExternCollector;
SINCLAIRDLL_API NUTSError *pExternError;
DataSourceCollector *pCollector;

FSDescriptor SinclairFS[2] = {
	{
		/* .FriendlyName = */ L"ZX Spectrum TAP Tape Image",
		/* .PUID         = */ FSID_SPECTRUM_TAP,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize,
		0, { }, { },
		1, { L"TAP" }, { FT_MiscImage }, { FT_TapeImage },
		0
	},
	{
		/* .FriendlyName = */ L"ZX Spectrum +3DOS",
		/* .PUID         = */ FSID_DOS3,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Uses_DSK,
		0, { }, { },
		1, { L"DSK" }, { FT_MiscImage }, { FT_DiskImage },
		0
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
	/* .NumFS    = */ 2,
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

	return &SinclairDescriptor;
}

SINCLAIRDLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
{
	void *pFS = NULL;

	switch ( PUID )
	{
	case FSID_SPECTRUM_TAP:
		pFS = (void *) new TAPFileSystem( pSource );
		break;
	case FSID_DOS3:
		pFS = (void *) new DOS3FileSystem( pSource );
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