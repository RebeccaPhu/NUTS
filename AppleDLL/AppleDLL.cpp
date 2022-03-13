#include "stdafx.h"

#include "..\NUTS\EncodingEdit.h"

#include "AppleDLL.h"

#include "../NUTS/PluginDescriptor.h"
#include "../NUTS/Defs.h"
#include "../NUTS/DataSource.h"
#include "../NUTS/NUTSError.h"
#include "../NUTS/TZXIcons.h"
#include "AppleDefs.h"
#include "resource.h"

#include "MacintoshMFSFileSystem.h"

HMODULE hInstance;

DataSourceCollector *pCollector;

BYTE *NUTSSignature;

const FTIdentifier       FILE_MACINTOSH     = L"MacintoshFileObject";
const EncodingIdentifier ENCODING_MACINTOSH = L"MacintoshEncoding";
const PluginIdentifier   APPLE_PLUGINID     = L"AppleNUTSPlugin";

BYTE *pChicago = nullptr;

FSDescriptor AppleFS[] = {
	{
		/* .FriendlyName = */ L"Macintosh MFS 400K",
		/* .PUID         = */ FSID_MFS,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_FixedSize | FSF_UseSectors,
		512, 80 * 10 * 512,
		(BYTE *) "IMG"
	},
	{
		/* .FriendlyName = */ L"Macintosh MFS Hard Disk",
		/* .PUID         = */ FSID_MFS,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_ArbitrarySize | FSF_UseSectors,
		512, 0,
		(BYTE *) "IMG"
	},
};

#define APPLEFS_COUNT ( sizeof(AppleFS) / sizeof(FSDescriptor) )

std::wstring ImageExtensions[] = { L"DSK", L"IMG" };

#define IMAGE_EXT_COUNT ( sizeof(ImageExtensions) / sizeof( std::wstring ) )


NUTSProvider ProviderAmstrad = { L"Apple", 0, 0 };

WCHAR *pChicagoName = L"Chicago";

void *CreateFS( FSIdentifier FSID, DataSource *pSource )
{
	void *pFS = NULL;

	if ( ( FSID == FSID_MFS ) || ( FSID == FSID_MFS_HD ) )
	{
		pFS = (void *) new MacintoshMFSFileSystem( pSource );
	}

	return pFS;
}

void LoadFonts()
{
	if ( pChicago == nullptr )
	{
		HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_CHICAGO ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hInstance, hResource);
		DWORD dwSize     = SizeofResource(hInstance, hResource);
		LPVOID lpAddress = LockResource(hMemory);

		pChicago = (BYTE *) malloc( 256 * 8 );

		ZeroMemory( pChicago, 256 * 8 );
		memcpy( pChicago, lpAddress, dwSize );
	}
}

UINT APPL_ICON;
UINT DOC_ICON;
UINT BLANK_ICON;

static WCHAR *PluginAuthor = L"Rebecca Gellman";

APPLEDLL_API int NUTSCommandHandler( PluginCommand *cmd )
{
	switch ( cmd->CommandID )
	{
	case PC_SetPluginConnectors:
		pCollector   = (DataSourceCollector *) cmd->InParams[ 0 ].pPtr;
		pGlobalError = (NUTSError *)           cmd->InParams[ 1 ].pPtr;
		
		LoadFonts();

		cmd->OutParams[ 0 ].Value = 0x00000001;
		cmd->OutParams[ 1 ].pPtr  = (void *) APPLE_PLUGINID.c_str();

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportProviders:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetProviderDescriptor:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &ProviderAmstrad;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;
		
	case PC_ReportFileSystems:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].Value = APPLEFS_COUNT;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_DescribeFileSystem:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &AppleFS[ cmd->InParams[ 1 ].Value ];
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

		return NUTS_PLUGIN_SUCCESS;

	case PC_CreateFileSystem:
		{
			DataSource *pSource = (DataSource *) cmd->InParams[ 2 ].pPtr;

			void *pFS = (void *) CreateFS( FSIdentifier( (WCHAR *) cmd->InParams[ 0 ].pPtr ), pSource );

			cmd->OutParams[ 0 ].pPtr = pFS;

			if ( pFS == nullptr )
			{
				return NUTS_PLUGIN_ERROR;
			}

			return NUTS_PLUGIN_SUCCESS;
		}

	case PC_ReportEncodingCount:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportFonts:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetFontPointer:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr  = (void *) pChicago;
			cmd->OutParams[ 1 ].pPtr  = (void *) pChicagoName;
			cmd->OutParams[ 2 ].pPtr  = (void *) ENCODING_MACINTOSH.c_str();
			cmd->OutParams[ 3 ].Value = NULL;

			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_ReportFSFileTypeCount:
		cmd->OutParams[ 0 ].Value = 2;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportIconCount:
		cmd->OutParams[ 0 ].Value = 3;

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeIcon:
		{
			BYTE Icon   = (BYTE) cmd->InParams[ 0 ].Value;

			if ( Icon == 0 )
			{
				APPL_ICON = (UINT) cmd->InParams[ 1 ].Value;

				HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_APPL ) );

				cmd->OutParams[ 0 ].pPtr = (void *) bmp;
			}

			if ( Icon == 1 )
			{
				DOC_ICON = (UINT) cmd->InParams[ 1 ].Value;

				HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_DOC ) );

				cmd->OutParams[ 0 ].pPtr = (void *) bmp;
			}

			if ( Icon == 2 )
			{
				BLANK_ICON = (UINT) cmd->InParams[ 1 ].Value;

				HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_BLANK ) );

				cmd->OutParams[ 0 ].pPtr = (void *) bmp;
			}
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportPluginCreditStats:
		cmd->OutParams[ 0 ].pPtr  = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_PLUGINBANNER ) );
		cmd->OutParams[ 1 ].pPtr  = (void *) PluginAuthor;
		cmd->OutParams[ 2 ].Value = 5;
		cmd->OutParams[ 3 ].Value = 0;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetIconLicensing:
		{
			static WCHAR *pIcons[5] =
			{
				L"Apple Logo", L"Macintosh Application Icon", L"Macintosh Document Icon", L"Macintosh File Icon", L"Repair Icon"
			};

			static int iIcons[ 5 ] = { IDB_APPLE, IDB_APPL, IDB_DOC, IDB_BLANK, IDB_REPAIR };
			
			static WCHAR *pIconset = L"Large Android Icons";
			static WCHAR *pAuthor  = L"Aha-Soft";
			static WCHAR *pLicense = L"CC Attribution 3.0 US";
			static WCHAR *pURL     = L"https://creativecommons.org/licenses/by/3.0/us/";
			static WCHAR *pEmpty   = L"";
			static WCHAR *pSet2    = L"Company Logo or System Icon";
			static WCHAR *pApple   = L"Apple Computer Inc.";
			static WCHAR *pFair    = L"Icons and logos used under fair use for familiar purposes";

			cmd->OutParams[ 0 ].pPtr = pIcons[ cmd->InParams[ 0 ].Value ];
			cmd->OutParams[ 1 ].pPtr = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( iIcons[ cmd->InParams[ 0 ].Value ] ) );
			
			if ( cmd->InParams[ 0 ].Value == 4 )
			{
				cmd->OutParams[ 2 ].pPtr = pIconset;
				cmd->OutParams[ 3 ].pPtr = pAuthor;
				cmd->OutParams[ 4 ].pPtr = pLicense;
				cmd->OutParams[ 5 ].pPtr = pURL;
			}
			else
			{
				cmd->OutParams[ 2 ].pPtr = pSet2;
				cmd->OutParams[ 3 ].pPtr = pApple;
				cmd->OutParams[ 4 ].pPtr = pFair;
				cmd->OutParams[ 5 ].pPtr = pEmpty;
			}
		}
		return NUTS_PLUGIN_SUCCESS;

	case PC_GetFOPDirectoryTypes:
		{
			static FOPDirectoryType FT;

			FT.FriendlyName = L"Apple";
			FT.Identifier   = L"APPLE";

			cmd->OutParams[ 0 ].pPtr = &FT;
			cmd->OutParams[ 1 ].pPtr = nullptr;
		}

		return NUTS_PLUGIN_SUCCESS;
	}

	return NUTS_PLUGIN_UNRECOGNISED;
};