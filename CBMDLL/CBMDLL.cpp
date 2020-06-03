// CBMDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "CBMDLL.h"

#include "D64FileSystem.h"
#include "IECATAFileSystem.h"
#include "OpenCBMSource.h"
#include "OpenCBMPlugin.h"

#include "resource.h"

#include "../NUTS/Defs.h"

#define FONTID_PETSCII1 0x0000CBC1
#define FONTID_PETSCII2 0x0000CBC2
#define PLUGINID_CBM    0x0000CBC0
#define FSID_OPENCBM    0x0000CBC3

BYTE *pPETSCII = nullptr;

HMODULE hInstance;

CBMDLL_API DataSourceCollector *pExternCollector;
DataSourceCollector *pCollector;

FSDescriptor CBMFS[2] = {
	{
		/* .FriendlyName = */ L"D64 Commore Disk Image",
		/* .PUID         = */ FSID_D64,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces,
		0, { }, { },
		1, { L"D64" }, { FT_MiscImage }, { FT_DiskImage },
		256, 1700
	},
	{
		/* .FriendlyName = */ L"IEC-ATA Hard Disk",
		/* .PUID         = */ FSID_IECATA,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Formats_Raw | FSF_Supports_Dirs | FSF_Supports_Spaces | FSF_ArbitrarySize | FSF_UseSectors,
		0, { }, { },
		0, { }, { }, { },
		512, 0
	}
};

FontDescriptor CBMFonts[2] = {
	{
		/* .FriendlyName  = */ L"PETSCII 1",
		/* .PUID          = */ FONTID_PETSCII1,
		/* .Flags         = */ 0,
		/* .MinChar       = */ 0,
		/* .MaxChar       = */ 127,
		/* .ProcessableControlCodes = */ { 0xFF },
		/* .pFontData     = */ NULL,
		/* .MatchingFSIDs = */ {
			ENCODING_PETSCII,
			0
		}
	},
	{
		/* .FriendlyName  = */ L"PETSCII 2",
		/* .PUID          = */ FONTID_PETSCII2,
		/* .Flags         = */ 0,
		/* .MinChar       = */ 0,
		/* .MaxChar       = */ 127,
		/* .ProcessableControlCodes = */ { 0xFF },
		/* .pFontData     = */ NULL,
		/* .MatchingFSIDs = */ {
			ENCODING_PETSCII,
			0
		}
	}
};

RootHook RootHooks[ 4 ] = {
	{
		L"OpenCBM/8",
		FSID_OPENCBM,
		NULL,
		{ 8 }
	},
	{
		L"OpenCBM/9",
		FSID_OPENCBM,
		NULL,
		{ 9 }
	},
	{
		L"OpenCBM/10",
		FSID_OPENCBM,
		NULL,
		{ 10 }
	},
	{
		L"OpenCBM/11",
		FSID_OPENCBM,
		NULL,
		{ 11 }
	}
};

PluginDescriptor CBMDescriptor = {
	/* .Provider = */ L"CBM",
	/* .PUID     = */ PLUGINID_CBM,
	/* .NumFS    = */ 2,
	/* .NumFonts = */ 2,
	/* .BASXlats = */ 0,
	/* .GFXXlats = */ 0,
	/* .HumHooks = */ 4,

	/* .FSDescriptors  = */ CBMFS,
	/* .FontDescriptor = */ CBMFonts,
	/* .BASXlators     = */ nullptr,
	/* .GFXXlats       = */ nullptr,
	/* .RootHooks      = */ RootHooks
};

CBMDLL_API PluginDescriptor *GetPluginDescriptor(void)
{
	if ( !OpenCBMLoaded )
	{
		LoadOpenCBM( );
	}

	if ( pPETSCII == nullptr )
	{
		HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_C64 ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hInstance, hResource);
		DWORD dwSize     = SizeofResource(hInstance, hResource);
		LPVOID lpAddress = LockResource(hMemory);

		pPETSCII = (BYTE *) malloc( (size_t) dwSize );

		memcpy( pPETSCII, lpAddress, dwSize );

		/* The CHAR ROM as extracted from a C64 doesn't actually store chars in the
		   order in which the character set references them. We must shuffle. */

		for ( BYTE c=0; c<2; c++ )
		{
			CBMFonts[c].pFontData = (BYTE *) malloc( 256 * 8 );

			memset( CBMFonts[c].pFontData, 0, 256 * 8 );

			WORD shifts[][3] = {
				{ 0x20, 0x20, 0x20 },
				{ 0x00, 0x20, 0x40 },
				{ 0x40, 0x40, 0x60 },
				{ 0x40, 0x40, 0xC0 },
				{ 0x00, 0x00, 0x00 }
			};

			BYTE s = 0;

			while ( ( shifts[s][0] != 0 ) || ( shifts[s][1] != 0 ) )
			{
				memcpy(
					&CBMFonts[c].pFontData[ shifts[s][2] * 8 ],
					&pPETSCII[ ( 0x800 * c ) + ( shifts[s][0] * 8 ) ],
					shifts[s][1] * 8
				);

				s++;
			}
		}
	}

	CBMDescriptor.RootHooks[ 0 ].HookIcon = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_OPENCBM ) );
	CBMDescriptor.RootHooks[ 1 ].HookIcon = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_OPENCBM ) );
	CBMDescriptor.RootHooks[ 2 ].HookIcon = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_OPENCBM ) );
	CBMDescriptor.RootHooks[ 3 ].HookIcon = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_OPENCBM ) );

	return &CBMDescriptor;
}

/* Hook Icon Creds */
/* Serial Port: Port Icon, FatCow Hosting Icons, FatCow, CC Attribution 3.0 United States */


CBMDLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
{
	/* Do this because the compiler is too stupid to do a no-op converstion without having it's hand held */
	pCollector = pExternCollector;

	void *pFS = NULL;

	switch ( PUID )
	{
	case FSID_D64:
		pFS = (void *) new D64FileSystem( pSource );
		break;

	case FSID_OPENCBM:
		{
			BYTE DriveNum = 0;

			pSource->ReadRaw( 0, 1, &DriveNum );

			OpenCBMSource *pOpenCBMSource = new OpenCBMSource( DriveNum );

			pFS = (void *) new D64FileSystem( pOpenCBMSource );

			pOpenCBMSource->Release();
		}
		break;

	case FSID_IECATA:
		pFS = (void *) new IECATAFileSystem( pSource );
		break;
	};

	return pFS;
}