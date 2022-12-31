// OricDLL.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "OricDLL.h"

#include "../NUTS/NUTSTypes.h"

#include <string>

#include "OricDefs.h"

#include "../NUTS/NUTSError.h"
#include "../NUTS/Preference.h"

#include "resource.h"

#include "MFMDISKWrapper.h"
#include "ORICDSKSource.h"

HMODULE hInstance;

DataSourceCollector *pCollector;

BYTE *NUTSSignature;

const EncodingIdentifier ENCODING_ORIC = L"OricEncoding";
const FontIdentifier     FONT_ORIC     = L"OricFont";
const FTIdentifier       FT_ORIC       = L"OricFile";
const PluginIdentifier   PLUGIN_ORIC   = L"TangerinePlugin";
const ProviderIdentifier ORIC_PROVIDER = L"TangerineProvider";
const WrapperIdentifier  MFM_DSK       = L"MFM_DSK_Wrapper";
const WrapperIdentifier  ORIC_DSK      = L"ORIC_DSK_Wrapper";


BYTE *pOricFont = nullptr;

void LoadFonts()
{
	HRSRC   hResource;
	HGLOBAL hMemory;
	DWORD   dwSize;
	LPVOID  lpAddress;

	if ( pOricFont == nullptr )
	{
		hResource = FindResource(hInstance, MAKEINTRESOURCE( IDF_ORIC ), RT_RCDATA);
		hMemory   = LoadResource(hInstance, hResource);
		dwSize    = SizeofResource(hInstance, hResource);
		lpAddress = LockResource(hMemory);

		// Font data starts from character 32
		pOricFont = (BYTE *) malloc( 256 * 8 );

		ZeroMemory( pOricFont, 256 * 8 );

		memcpy( &pOricFont[ 32 * 8 ], lpAddress, dwSize );

		// Fill in the other blanks.

		for ( WORD i=0; i<256; i++ )
		{
			if ( ( i >= 32 ) &&( i <= 127 ) )
			{
				continue;
			}

			// Copy from the Question Mark.
			BYTE *pChar = &pOricFont[ i * 8 ];
			BYTE *pSrc  = &pOricFont[ ( 0x3f - 0x20 ) * 8 ];

			pChar[ 0 ] = 0xFF;
			pChar[ 1 ] = pSrc[ 1 ] | 0x81;
			pChar[ 2 ] = pSrc[ 2 ] | 0x81;
			pChar[ 3 ] = pSrc[ 3 ] | 0x81;
			pChar[ 4 ] = pSrc[ 4 ] | 0x81;
			pChar[ 5 ] = pSrc[ 5 ] | 0x81;
			pChar[ 6 ] = pSrc[ 6 ] | 0x81;
			pChar[ 7 ] = 0xFF;
		}
	}
}

WCHAR *pOricFontName = L"Oric-1/Oric Atmos";

NUTSProvider ProviderOric = { L"Tangerine", PLUGIN_ORIC, ORIC_PROVIDER };

std::wstring OricDSK = L"DSK";

static WCHAR *PluginAuthor = L"Rebecca Gellman";

ORICDLL_API int NUTSCommandHandler( PluginCommand *cmd )
{
	switch ( cmd->CommandID )
	{
	case PC_SetPluginConnectors:
		pCollector   = (DataSourceCollector *) cmd->InParams[ 0 ].pPtr;
		pGlobalError = (NUTSError *)           cmd->InParams[ 1 ].pPtr;
		
		LoadFonts();

		cmd->OutParams[ 0 ].Value = 0x00000001;
		cmd->OutParams[ 1 ].pPtr  = (void *) PLUGIN_ORIC.c_str();

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportProviders:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetProviderDescriptor:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &ProviderOric;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;
		
	case PC_ReportEncodingCount:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportFonts:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetFontPointer:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) pOricFont;
			cmd->OutParams[ 1 ].pPtr = (void *) pOricFontName;
			cmd->OutParams[ 2 ].pPtr = (void *) FONT_ORIC.c_str();
			cmd->OutParams[ 3 ].pPtr = (void *) ENCODING_ORIC.c_str();
			cmd->OutParams[ 4 ].pPtr = nullptr;

			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_ReportImageExtensions:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetImageExtension:
		cmd->OutParams[ 0 ].pPtr = (void *) OricDSK.c_str();
		cmd->OutParams[ 1 ].Value = FT_MiscImage;
		cmd->OutParams[ 2 ].Value = FT_DiskImage;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportFSFileTypeCount:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportPluginCreditStats:
		{
			std::wstring Provider = std::wstring( (WCHAR *) cmd->InParams[ 0 ].pPtr );

			cmd->OutParams[ 0 ].pPtr  = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_ORIC ) );
			cmd->OutParams[ 1 ].pPtr  = (void *) PluginAuthor;
			cmd->OutParams[ 2 ].Value = 0;
			cmd->OutParams[ 3 ].Value = 0;

			return NUTS_PLUGIN_SUCCESS;
		}

		break;

	case PC_GetWrapperCount:
		{
			cmd->OutParams[ 0 ].Value = 2U;
		}
		return NUTS_PLUGIN_SUCCESS;

	case PC_GetWrapperDescriptor:
		{
			static WrapperDescriptor descO;
			static WrapperDescriptor descN;

			descO.FriendlyName = L"Oric (Old) DSK Wrapper";
			descO.Identifier   = ORIC_DSK;

			descN.FriendlyName = L"Oric MFM (New) DSK Wrapper";
			descN.Identifier   = MFM_DSK;

			if ( cmd->InParams[ 0 ].Value == 0 )
			{
				cmd->OutParams[ 0 ].pPtr = &descO;
			}
			else if ( cmd->InParams[ 0 ].Value == 1 )
			{
				cmd->OutParams[ 0 ].pPtr = &descN;
			}
			else
			{
				return NUTS_PLUGIN_ERROR;
			}
		}
		return NUTS_PLUGIN_SUCCESS;

	case PC_LoadWrapper:
		{
			WrapperIdentifier WID = (WrapperIdentifier) (WCHAR *) cmd->InParams[ 0 ].pPtr;
			DataSource *pSource   = (DataSource *) cmd->InParams[ 1 ].pPtr;

			DataSource *pWrap = nullptr;

			if ( ( WID == ORIC_DSK ) && ( pSource != nullptr ) )
			{
				pWrap = new ORICDSKSource( pSource );
			}

			if ( ( WID == MFM_DSK ) && ( pSource != nullptr ) )
			{
				pWrap = new MFMDISKWrapper( pSource );
			}

			cmd->OutParams[ 0 ].pPtr = pWrap;
			
		}
		return NUTS_PLUGIN_SUCCESS;
    }

	return NUTS_PLUGIN_UNRECOGNISED;
}