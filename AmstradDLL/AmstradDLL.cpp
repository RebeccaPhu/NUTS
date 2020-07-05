// AmstradDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AmstradDLL.h"

#include "../NUTS/PluginDescriptor.h"
#include "../NUTS/Defs.h"
#include "../NUTS/DataSource.h"
#include "../NUTS/NUTSError.h"
#include "Defs.h"
#include "resource.h"

#include "AMSDOSFileSystem.h"
#include "LocomotiveBASICTranslator.h"

HMODULE hInstance;

BYTE *pCPCF = nullptr;

AMSTRADDLL_API DataSourceCollector *pExternCollector;
AMSTRADDLL_API NUTSError *pExternError;
DataSourceCollector *pCollector;

FSDescriptor AmstradFS[1] = {
	{
		/* .FriendlyName = */ L"Amstrad AMSDOS",
		/* .PUID         = */ FSID_AMSDOS,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_FixedSize | FSF_UseSectors | FSF_Uses_DSK | FSF_No_Quick_Format,
		0, { }, { },
		1, { L"DSK" }, { FT_MiscImage }, { FT_DiskImage },
		512, 40 * 9 * 512
	}
};

FontDescriptor AmstradFonts[1] = {
	{
		/* .FriendlyName  = */ L"CPC",
		/* .PUID          = */ FONTID_CPC,
		/* .Flags         = */ 0,
		/* .MinChar       = */ 0,
		/* .MaxChar       = */ 127,
		/* .ProcessableControlCodes = */ { 0xFF },
		/* .pFontData     = */ NULL,
		/* .MatchingFSIDs = */ {
			ENCODING_CPC,
			0
		}
	}
};

TextTranslator LocoBASIC[] = {
	{
		L"Amstrad/Locomotive BASIC",
		TUID_LOCO,
		0
	}
};

PluginDescriptor AmstradDescriptor = {
	/* .Provider = */ L"Amstrad",
	/* .PUID     = */ PLUGINID_AMSTRAD,
	/* .NumFS    = */ 1,
	/* .NumFonts = */ 1,
	/* .BASXlats = */ 1,
	/* .GFXXlats = */ 0,
	/* .NumHooks = */ 0,
	/* .Commands = */ 0,

	/* .FSDescriptors  = */ AmstradFS,
	/* .FontDescriptor = */ AmstradFonts,
	/* .BASXlators     = */ LocoBASIC,
	/* .GFXXlats       = */ nullptr,
	/* .RootHooks      = */ nullptr,
	/* .Commands       = */ { },
};

AMSTRADDLL_API PluginDescriptor *GetPluginDescriptor(void)
{
	/* Do this because the compiler is too stupid to do a no-op converstion without having it's hand held */
	pCollector   = pExternCollector;
	pGlobalError = pExternError;

	if ( pCPCF == nullptr )
	{
		HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_CPC ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hInstance, hResource);
		DWORD dwSize     = SizeofResource(hInstance, hResource);
		LPVOID lpAddress = LockResource(hMemory);

		pCPCF = (BYTE *) malloc( 256 * 8 );

		ZeroMemory( pCPCF, 256 * 8 );
		memcpy( pCPCF, lpAddress, dwSize );
	}

	AmstradDescriptor.FontDescriptors[0].pFontData = pCPCF;

	return &AmstradDescriptor;
}

AMSTRADDLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
{
	void *pFS = NULL;

	switch ( PUID )
	{
	case FSID_AMSDOS:
		pFS = (void *) new AMSDOSFileSystem( pSource );

		break;
	}

	return pFS;
}

AMSTRADDLL_API void *CreateTranslator( DWORD TUID )
{
	void *pXlator = nullptr;

	if ( TUID == TUID_LOCO )
	{
		pXlator = (TEXTTranslator *) new LocomotiveBASICTranslator();
	}

	return pXlator;
}