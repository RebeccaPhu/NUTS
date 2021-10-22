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

DWORD FILE_MACINTOSH;
DWORD ENCODING_MACINTOSH;
DWORD FT_AMSTRAD_TAPE;

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

APPLEDLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
{
	void *pFS = NULL;

	switch ( PUID )
	{
	case FSID_MFS:
	case FSID_MFS_HD:
		pFS = (void *) new MacintoshMFSFileSystem( pSource );

		break;
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

APPLEDLL_API int NUTSCommandHandler( PluginCommand *cmd )
{
	switch ( cmd->CommandID )
	{
	case PC_SetPluginConnectors:
		pCollector   = (DataSourceCollector *) cmd->InParams[ 0 ].pPtr;
		pGlobalError = (NUTSError *)           cmd->InParams[ 1 ].pPtr;
		
		LoadFonts();

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
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_SetEncodingBase:
		ENCODING_MACINTOSH = cmd->InParams[ 0 ].Value + 0;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportFonts:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetFontPointer:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr  = (void *) pChicago;
			cmd->OutParams[ 1 ].pPtr  = (void *) pChicagoName;
			cmd->OutParams[ 2 ].Value = ENCODING_MACINTOSH;
			cmd->OutParams[ 3 ].Value = NULL;

			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_ReportFSFileTypeCount:
		cmd->OutParams[ 0 ].Value = 2;

		return NUTS_PLUGIN_SUCCESS;

	case PC_SetFSFileTypeBase:
		{
			DWORD Base = cmd->InParams[ 0 ].Value;

			FILE_MACINTOSH   = Base + 0;
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportIconCount:
		cmd->OutParams[ 0 ].Value = 2;

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

			if ( Icon == 1 )
			{
				BLANK_ICON = (UINT) cmd->InParams[ 1 ].Value;

				HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_BLANK ) );

				cmd->OutParams[ 0 ].pPtr = (void *) bmp;
			}
		}

		return NUTS_PLUGIN_SUCCESS;

	}

	return NUTS_PLUGIN_UNRECOGNISED;
};