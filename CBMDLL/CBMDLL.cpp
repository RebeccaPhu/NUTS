// CBMDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "CBMDLL.h"

#include "D64FileSystem.h"
#include "IECATAFileSystem.h"

#include "resource.h"

#include "../NUTS/Defs.h"

#define FONTID_PETSCII1 0x0000CBC1
#define FONTID_PETSCII2 0x0000CBC2
#define PLUGINID_CBM    0x0000CBC0

BYTE *pPETSCII = nullptr;

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

PluginDescriptor CBMDescriptor = {
	/* .Provider = */ L"CBM",
	/* .PUID     = */ PLUGINID_CBM,
	/* .NumFS    = */ 2,
	/* .NumFonts = */ 2,
	/* .BASXlats = */ 0,
	/* .GFXXlats = */ 0,

	/* .FSDescriptors  = */ CBMFS,
	/* .FontDescriptor = */ CBMFonts,
	/* .BASXlators     = */ nullptr,
	/* .GFXXlats       = */ nullptr
};

CBMDLL_API PluginDescriptor *GetPluginDescriptor(void)
{
	if ( pPETSCII == nullptr )
	{
		HMODULE hModule  = GetModuleHandle( L"CBMDLL" );

		HRSRC hResource  = FindResource(hModule, MAKEINTRESOURCE( IDF_C64 ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hModule, hResource);
		DWORD dwSize     = SizeofResource(hModule, hResource);
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

	return &CBMDescriptor;
}

CBMDLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
{
	void *pFS = NULL;

	switch ( PUID )
	{
	case FSID_D64:
		pFS = (void *) new D64FileSystem( pSource );
		break;

	case FSID_IECATA:
		pFS = (void *) new IECATAFileSystem( pSource );
		break;
	};

	return pFS;
}