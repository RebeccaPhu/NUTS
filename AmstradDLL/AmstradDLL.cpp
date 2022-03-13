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

DataSourceCollector *pCollector;

BYTE *NUTSSignature;

const FTIdentifier       FILE_AMSTRAD    = L"Amstrad_AMSDOS_Object";
const EncodingIdentifier ENCODING_CPC    = L"Amstrad_Text_Encoding";
const TXIdentifier       TUID_LOCO       = L"Amstrad_Locomotive_BASIC";
const FTIdentifier       FT_AMSTRAD_TAPE = L"Amstrad_Tape_Object";

FSDescriptor AmstradFS[] = {
	{
		/* .FriendlyName = */ L"Amstrad AMSDOS",
		/* .PUID         = */ FSID_AMSDOS,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Supports_Dirs | FSF_FixedSize | FSF_UseSectors | FSF_Uses_DSK | FSF_No_Quick_Format,
		512, 40 * 9 * 512,
		(BYTE *) "DSK"
	},
	{
		/* .FriendlyName = */ L"Amstrad CPC CDT Tape Image",
		/* .PUID         = */ FSID_AMSTRAD_CDT,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize,
		0, 0,
		(BYTE *) "CDT"
	}
};

#define AMSFS_COUNT ( sizeof(AmstradFS) / sizeof(FSDescriptor) )

std::wstring ImageExtensions[] = { L"DSK", L"CDT" };

#define IMAGE_EXT_COUNT ( sizeof(ImageExtensions) / sizeof( std::wstring ) )


DataTranslator Translators[] = {
	{ 0, L"Amstrad/Locomotive BASIC", 0, TXTextTranslator }
};

#define TRANSLATOR_COUNT ( sizeof(Translators) / sizeof(DataTranslator) )

UINT IconBitmapIDs[ 25 ] = {
	IDB_STDDATA, IDB_TURBODATA, IDB_PURETONE,  IDB_PULSESEQ,
	IDB_PDATA,   IDB_DIRECT,    IDB_CSW,       IDB_GENERAL,
	IDB_PAUSE,   IDB_GSTART,    IDB_GEND,      IDB_JUMP,
	IDB_LSTART,  IDB_LEND,      IDB_CALL,      IDB_RETURN,
	IDB_SELECT,  IDB_STOP48,    IDB_SETSIGNAL, IDB_DESC,
	IDB_MESSAGE, IDB_INFO,      IDB_HARDWARE,  IDB_CUSTOM,
	IDB_GLUE
};

DWORD IconIDs[ 25 ];

void LoadFonts()
{
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
}

void *CreateFS( FSIdentifier FSID, DataSource *pSource )
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
			IconMap[ BlockIDs[ i ] ] = IconIDs[ i ];
		}
	}

	if ( FSID == FSID_AMSDOS )
	{
		pFS = (void *) new AMSDOSFileSystem( pSource );
	}
	else if ( FSID == FSID_AMSTRAD_CDT )
	{
		pFS = (void *) new AmstradCDTFileSystem( pSource );
	}

	return pFS;
}

void *CreateTranslator( TXIdentifier TUID )
{
	void *pXlator = nullptr;

	if ( TUID == TUID_LOCO )
	{
		pXlator = (TEXTTranslator *) new LocomotiveBASICTranslator();
	}

	return pXlator;
}

NUTSProvider ProviderAmstrad = { L"Amstrad", 0, 0 };

WCHAR *pCPCFontName = L"Amstrad";

static WCHAR *PluginAuthor = L"Rebecca Gellman";

AMSTRADDLL_API int NUTSCommandHandler( PluginCommand *cmd )
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
			cmd->OutParams[ 0 ].Value = AMSFS_COUNT;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_DescribeFileSystem:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &AmstradFS[ cmd->InParams[ 1 ].Value ];
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
			cmd->OutParams[ 0 ].pPtr  = (void *) pCPCF;
			cmd->OutParams[ 1 ].pPtr  = (void *) pCPCFontName;
			cmd->OutParams[ 2 ].pPtr  = (void *) ENCODING_CPC.c_str();
			cmd->OutParams[ 3 ].Value = NULL;

			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_ReportFSFileTypeCount:
		cmd->OutParams[ 0 ].Value = 2;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportIconCount:
		cmd->OutParams[ 0 ].Value = 25;

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeIcon:
		{
			BYTE Icon   = (BYTE) cmd->InParams[ 0 ].Value;
			UINT IconID = (UINT) cmd->InParams[ 1 ].Value;

			HBITMAP bmp = LoadBitmap( hInstance, MAKEINTRESOURCE( IconBitmapIDs[ Icon ] ) );

			IconIDs[ Icon ] = IconID;

			cmd->OutParams[ 0 ].pPtr = (void *) bmp;
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportTranslators:
		cmd->OutParams[ 0 ].Value = TRANSLATOR_COUNT;

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeTranslator:
		{
			BYTE tx = (BYTE) cmd->InParams[ 0 ].Value;

			cmd->OutParams[ 0 ].pPtr  = (void *) Translators[ tx ].FriendlyName.c_str();
			cmd->OutParams[ 1 ].Value = Translators[ tx ].Flags;
			cmd->OutParams[ 2 ].Value = Translators[ tx ].ProviderID;
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_CreateTranslator:
		cmd->OutParams[ 0 ].pPtr = CreateTranslator( TXIdentifier( (WCHAR *) cmd->InParams[ 0 ].pPtr ) );

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportPluginCreditStats:
		cmd->OutParams[ 0 ].pPtr  = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_AMSTRAD ) );
		cmd->OutParams[ 1 ].pPtr  = (void *) PluginAuthor;
		cmd->OutParams[ 2 ].Value = 0;
		cmd->OutParams[ 3 ].Value = 0;

		return NUTS_PLUGIN_SUCCESS;

	}

	return NUTS_PLUGIN_UNRECOGNISED;
}