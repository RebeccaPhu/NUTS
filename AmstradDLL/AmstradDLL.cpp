// AmstradDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AmstradDLL.h"

#include "../NUTS/PluginDescriptor.h"
#include "../NUTS/Defs.h"
#include "../NUTS/DataSource.h"
#include "../NUTS/NUTSError.h"
#include "../NUTS/TZXIcons.h"
#include "Defs.h"
#include "resource.h"

#include "AMSDOSFileSystem.h"
#include "AmstradCDTFileSystem.h"
#include "LocomotiveBASICTranslator.h"

HMODULE hInstance;

BYTE *pCPCF = nullptr;

AMSTRADDLL_API DataSourceCollector *pExternCollector;
AMSTRADDLL_API NUTSError *pExternError;
DataSourceCollector *pCollector;

FSDescriptor AmstradFS[2] = {
	{
		/* .FriendlyName = */ L"Amstrad AMSDOS",
		/* .PUID         = */ FSID_AMSDOS,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_FixedSize | FSF_UseSectors | FSF_Uses_DSK | FSF_No_Quick_Format,
		0, { }, { },
		1, { L"DSK" }, { FT_MiscImage }, { FT_DiskImage },
		512, 40 * 9 * 512
	},
	{
		/* .FriendlyName = */ L"Amstrad CPC CDT Tape Image",
		/* .PUID         = */ FSID_AMSTRAD_CDT,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize,
		25, { 0 }, { 0 },
		1, { L"CDT" }, { FT_MiscImage }, { FT_TapeImage },
		0
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
	/* .NumFS    = */ 2,
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

	if ( AmstradFS[ 1 ].Icons[ 0 ] == 0 )
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

			AmstradFS[ 1 ].Icons[ i ] = (void *) bmp;
		}
	}

	AmstradDescriptor.FontDescriptors[0].pFontData = pCPCF;

	return &AmstradDescriptor;
}

AMSTRADDLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
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
			IconMap[ BlockIDs[ i ] ] = AmstradFS[ 1 ].IconIDs[ i ];
		}
	}

	switch ( PUID )
	{
	case FSID_AMSDOS:
		pFS = (void *) new AMSDOSFileSystem( pSource );

		break;

	case FSID_AMSTRAD_CDT:
		pFS = (void *) new AmstradCDTFileSystem( pSource );

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