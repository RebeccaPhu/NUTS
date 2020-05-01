// AmigaDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AmigaDLL.h"

#include "AmigaFileSystem.h"
#include "../NUTS/PluginDescriptor.h"
#include "../NUTS/Defs.h"
#include "../NUTS/DataSource.h"
#include "Defs.h"
#include "resource.h"

HMODULE hInstance;

BYTE *pTopaz1 = nullptr;
BYTE *pTopaz2 = nullptr;

AMIGADLL_API DataSourceCollector *pExternCollector;
DataSourceCollector *pCollector;

FSDescriptor AmigaFS[2] = {
	{
		/* .FriendlyName = */ L"Commodore Amiga OFS Disk Image",
		/* .PUID         = */ FSID_AMIGAO,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_Supports_Spaces | FSF_ArbitrarySize | FSF_UseSectors,
		0, { }, { },
		1, { L"ADF" }, { FT_MiscImage }, { FT_DiskImage },
		512, 1700
	},
	{
		/* .FriendlyName = */ L"Commodore Amiga FFS Disk Image",
		/* .PUID         = */ FSID_AMIGAF,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_Supports_Spaces | FSF_ArbitrarySize | FSF_UseSectors,
		0, { }, { },
		1, { L"ADF" }, { FT_MiscImage }, { FT_DiskImage },
		512, 1700
	}
};

FontDescriptor AmigaFonts[2] = {
	{
		/* .FriendlyName  = */ L"Topaz 1",
		/* .PUID          = */ FONTID_TOPAZ1,
		/* .Flags         = */ 0,
		/* .MinChar       = */ 0,
		/* .MaxChar       = */ 127,
		/* .ProcessableControlCodes = */ { 0xFF },
		/* .pFontData     = */ NULL,
		/* .MatchingFSIDs = */ {
			ENCODING_AMIGA,
			0
		}
	},
	{
		/* .FriendlyName  = */ L"Topaz 2",
		/* .PUID          = */ FONTID_TOPAZ2,
		/* .Flags         = */ 0,
		/* .MinChar       = */ 0,
		/* .MaxChar       = */ 127,
		/* .ProcessableControlCodes = */ { 0xFF },
		/* .pFontData     = */ NULL,
		/* .MatchingFSIDs = */ {
			ENCODING_AMIGA,
			0
		}
	}
};

PluginDescriptor AmigaDescriptor = {
	/* .Provider = */ L"Amiga",
	/* .PUID     = */ PLUGINID_AMIGA,
	/* .NumFS    = */ 2,
	/* .NumFonts = */ 2,
	/* .BASXlats = */ 0,
	/* .GFXXlats = */ 0,

	/* .FSDescriptors  = */ AmigaFS,
	/* .FontDescriptor = */ AmigaFonts,
	/* .BASXlators     = */ nullptr,
	/* .GFXXlats       = */ nullptr
};

AMIGADLL_API PluginDescriptor *GetPluginDescriptor(void)
{
	if ( pTopaz1 == nullptr )
	{
		HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_TOPAZ1 ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hInstance, hResource);
		DWORD dwSize     = SizeofResource(hInstance, hResource);
		LPVOID lpAddress = LockResource(hMemory);

		pTopaz1 = (BYTE *) malloc( 256 * 8 );

		ZeroMemory( pTopaz1, 256 * 8 );
		memcpy( &pTopaz1[ 33 * 8 ], lpAddress, dwSize );
	}

	if ( pTopaz2 == nullptr )
	{
		HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_TOPAZ2 ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hInstance, hResource);
		DWORD dwSize     = SizeofResource(hInstance, hResource);
		LPVOID lpAddress = LockResource(hMemory);

		pTopaz2 = (BYTE *) malloc( 256 * 8 );

		ZeroMemory( pTopaz2, 256 * 8 );
		memcpy( &pTopaz2[ 33 * 8 ], lpAddress, dwSize );
	}

	AmigaDescriptor.FontDescriptors[0].pFontData = pTopaz1;
	AmigaDescriptor.FontDescriptors[1].pFontData = pTopaz2;

	return &AmigaDescriptor;
}

AMIGADLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
{
	/* Do this because the compiler is too stupid to do a no-op converstion without having it's hand held */
	pCollector = pExternCollector;

	void *pFS = NULL;

	switch ( PUID )
	{
	case FSID_AMIGAO:
	case FSID_AMIGAF:
		pFS = (void *) new AmigaFileSystem( pSource );
		break;
	}

	return pFS;
}