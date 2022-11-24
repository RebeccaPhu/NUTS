// AmigaDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AmigaDLL.h"

#include "AmigaFileSystem.h"
#include "DMS.h"
#include "ILBMTranslator.h"
#include "../NUTS/PluginDescriptor.h"
#include "../NUTS/DataSource.h"
#include "../NUTS/NUTSError.h"
#include "../NUTS/NestedImageSource.h"
#include "Defs.h"
#include "resource.h"

HMODULE hInstance;

BYTE *pTopaz1 = nullptr;
BYTE *pTopaz2 = nullptr;

DataSourceCollector *pCollector;

const FTIdentifier       FILE_AMIGA     = L"Amiga_File_Object";
const EncodingIdentifier ENCODING_AMIGA = L"Amiga_Encoding";
const TXIdentifier       TUID_ILBM      = L"ILBM_Translator";
const PluginIdentifier   AMIGA_PLUGINID = L"AmigaNUTSPlugin";
const ProviderIdentifier AMIGA_PROVIDER = L"Amiga_Provider";

const FontIdentifier     TOPAZ1         = L"Topaz1_Font";
const FontIdentifier     TOPAZ2         = L"Topaz2_Font";

BYTE *NUTSSignature;

FSDescriptor AmigaFS[] = {
	{
		/* .FriendlyName = */ L"Amiga OFS Disk Image",
		/* .PUID         = */ FSID_AMIGAO,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_Supports_Spaces | FSF_ArbitrarySize | FSF_UseSectors | FSF_Imaging,
		512, 1700,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"Amiga FFS Disk Image",
		/* .PUID         = */ FSID_AMIGAF,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_Supports_Spaces | FSF_ArbitrarySize | FSF_UseSectors | FSF_Imaging,
		512, 1700,
		(BYTE *) "ADF"
	},
	{
		/* .FriendlyName = */ L"Amiga DMS Disk Archive",
		/* .PUID         = */ FSID_AMIGADMS,
		/* .Flags        = */ FSF_Supports_Spaces | FSF_Supports_Dirs,
		0, 0
	}
};

#define AMIGAFS_COUNT ( sizeof(AmigaFS) / sizeof(FSDescriptor) )

std::wstring ImageExtensions[] = { L"ADF", L"DMS" };

#define IMAGE_EXT_COUNT ( sizeof(ImageExtensions) / sizeof( std::wstring ) )

void *CreateFS( FSIdentifier FSID, DataSource *pSource )
{
	FileSystem *pFS = NULL;

	if ( ( FSID == FSID_AMIGAO ) || ( FSID == FSID_AMIGAF ) )
	{
		pFS = new AmigaFileSystem( pSource );
	}
	else if ( FSID == FSID_AMIGADMS )
	{
		CTempFile nestObj;

		nestObj.Keep();

		BYTE sig[4];

		if ( pSource->ReadRaw( 0, 4, sig ) != DS_SUCCESS )
		{
			return nullptr;
		}

		if ( !rstrncmp( sig, (BYTE *) "DMS!", 4 ) )
		{
			return nullptr;
		}

		if ( ExtractDMS( pSource, nestObj ) == NUTS_SUCCESS )
		{
			NestedImageSource *pNest = new NestedImageSource( nullptr, nullptr, nestObj.Name() );

			pFS = new AmigaFileSystem( pNest );
		}
	}

	if ( pFS != nullptr )
	{
		pFS->FSID = FSID;
	}

	return (void *) pFS;
}

NUTSProvider ProviderAmiga = { L"Amiga", AMIGA_PLUGINID, AMIGA_PROVIDER };

WCHAR *pTopaz1FontName = L"Topaz 1";
WCHAR *pTopaz2FontName = L"Topaz 2";

void LoadFonts()
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
}

DataTranslator Translators[] = {
	{ AMIGA_PROVIDER, L"Interleaved Bitmap", TUID_ILBM, TXGFXTranslator },
};

#define TRANSLATOR_COUNT ( sizeof(Translators) / sizeof(DataTranslator) )

void *CreateTranslator( TXIdentifier TUID )
{
	void *pXlator = nullptr;

	if ( TUID == TUID_ILBM )
	{
		pXlator = (void *) new ILBMTranslator();
	}

	return pXlator;
}

static WCHAR *PluginAuthor = L"Rebecca Gellman";

AMIGADLL_API int NUTSCommandHandler( PluginCommand *cmd )
{
	switch ( cmd->CommandID )
	{
	case PC_SetPluginConnectors:
		pCollector   = (DataSourceCollector *) cmd->InParams[ 0 ].pPtr;
		pGlobalError = (NUTSError *)           cmd->InParams[ 1 ].pPtr;
		
		LoadFonts();

		cmd->OutParams[ 0 ].Value = 0x00000001;
		cmd->OutParams[ 1 ].pPtr  = (void *) AMIGA_PLUGINID.c_str();

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportProviders:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetProviderDescriptor:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &ProviderAmiga;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;
		
	case PC_ReportFileSystems:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].Value = AMIGAFS_COUNT;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_DescribeFileSystem:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &AmigaFS[ cmd->InParams[ 1 ].Value ];
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
		cmd->OutParams[ 0 ].Value = 2;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetFontPointer:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) pTopaz1;
			cmd->OutParams[ 1 ].pPtr = (void *) pTopaz1FontName;
			cmd->OutParams[ 2 ].pPtr = (void *) TOPAZ1.c_str();
			cmd->OutParams[ 3 ].pPtr = (void *) ENCODING_AMIGA.c_str();
			cmd->OutParams[ 4 ].pPtr = nullptr;

			return NUTS_PLUGIN_SUCCESS;
		}
		if ( cmd->InParams[ 0 ].Value == 1 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) pTopaz2;
			cmd->OutParams[ 1 ].pPtr = (void *) pTopaz2FontName;
			cmd->OutParams[ 2 ].pPtr = (void *) TOPAZ2.c_str();
			cmd->OutParams[ 3 ].pPtr = (void *) ENCODING_AMIGA.c_str();
			cmd->OutParams[ 4 ].pPtr = nullptr;

			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_ReportFSFileTypeCount:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportTranslators:
		cmd->OutParams[ 0 ].Value = TRANSLATOR_COUNT;

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeTranslator:
		{
			BYTE tx = (BYTE) cmd->InParams[ 0 ].Value;

			cmd->OutParams[ 0 ].pPtr  = (void *) Translators[ tx ].FriendlyName.c_str();
			cmd->OutParams[ 1 ].Value = Translators[ tx ].Flags;
			cmd->OutParams[ 2 ].pPtr  = (void *) Translators[ tx ].ProviderID.c_str();
			cmd->OutParams[ 3 ].pPtr  = (void *) Translators[ tx ].TUID.c_str();
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_CreateTranslator:
		cmd->OutParams[ 0 ].pPtr = CreateTranslator( TXIdentifier( (WCHAR *) cmd->InParams[ 0 ].pPtr ) );

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportPluginCreditStats:
		cmd->OutParams[ 0 ].pPtr  = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_AMIGA ) );
		cmd->OutParams[ 1 ].pPtr  = (void *) PluginAuthor;
		cmd->OutParams[ 2 ].Value = 0;
		cmd->OutParams[ 3 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetPluginCredits:
		{
			static WCHAR *pGrzegorz = L"Grzegorz Kraszewski: Amiga Sprite and info file format. See http://krashan.ppa.pl/articles/amigaicons/";

			cmd->OutParams[ 0 ].pPtr = (void *) pGrzegorz;

			return NUTS_PLUGIN_SUCCESS;
		}
		break;

	}

	return NUTS_PLUGIN_UNRECOGNISED;
}
