// SINCLAIRDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "SinclairDLL.h"

#include "SinclairDefs.h"
#include "../NUTS/Defs.h"
#include "resource.h"

#include "TAPFileSystem.h"
#include "SinclairTZXFileSystem.h"
#include "DOS3FileSystem.h"
#include "TRDFileSystem.h"
#include "../NUTS/TZXIcons.h"
#include "SpectrumSCREENTranslator.h"
#include "SpectrumBASICTranslator.h"
#include "../NUTS/NUTSError.h"

BYTE *pSinclairFont;

HMODULE hInstance;

DataSourceCollector *pCollector;

BYTE *NUTSSignature;

DWORD FT_SINCLAIR;
DWORD FT_SINCLAIR_DOS;
DWORD FT_SINCLAIR_TZX;
DWORD FT_SINCLAIR_TRD;
DWORD ENCODING_SINCLAIR;
DWORD GRAPHIC_SPECTRUM;
DWORD BASIC_SPECTRUM;

FSDescriptor SinclairFS[] = {
	{
		/* .FriendlyName = */ L"ZX Spectrum TAP Tape Image",
		/* .PUID         = */ FSID_SPECTRUM_TAP,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize | FSF_Reorderable,
		0, 0
	},
	{
		/* .FriendlyName = */ L"ZX Spectrum TZX Tape Image",
		/* .PUID         = */ FSID_SPECTRUM_TZX,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_DynamicSize | FSF_Reorderable,
		0, 0
	},
	{
		/* .FriendlyName = */ L"ZX Spectrum +3DOS",
		/* .PUID         = */ FSID_DOS3,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity | FSF_Uses_DSK,
		0,
	},
	{
		/* .FriendlyName = */ L"TR-DOS 80T Double Sided",
		/* .PUID         = */ FSID_TRD_80DS,
		/* .Flags        = */ FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		256, 256 * 16 * 80 * 2
	},
	{
		/* .FriendlyName = */ L"TR-DOS 40T Double Sided",
		/* .PUID         = */ FSID_TRD_40DS,
		/* .Flags        = */ FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		256, 256 * 16 * 40 * 2
	},
	{
		/* .FriendlyName = */ L"TR-DOS 80T Single Sided",
		/* .PUID         = */ FSID_TRD_80SS,
		/* .Flags        = */ FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		256, 256 * 16 * 80 * 1
	},
	{
		/* .FriendlyName = */ L"TR-DOS 40T Single Sided",
		/* .PUID         = */ FSID_TRD_40SS,
		/* .Flags        = */ FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		256, 256 * 16 * 40 * 1
	}
};

#define SINCLAIR_FSCOUNT ( sizeof(SinclairFS) / sizeof( FSDescriptor) )

std::wstring ImageExtensions[] = { L"DSK", L"TRD", L"TAP", L"TZX" };

#define IMAGE_EXT_COUNT ( sizeof(ImageExtensions) / sizeof( std::wstring ) )

DataTranslator Translators[] = {
	{ 0, L"ZX Spectrum BASIC", 0, TXTextTranslator },
	{ 0, L"ZC Spectrum",       0, TXGFXTranslator  }
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
	if ( pSinclairFont == nullptr )
	{
		HRSRC hResource  = FindResource(hInstance, MAKEINTRESOURCE( IDF_SINCLAIRFONT ), RT_RCDATA);
		HGLOBAL hMemory  = LoadResource(hInstance, hResource);
		DWORD dwSize     = SizeofResource(hInstance, hResource);
		LPVOID lpAddress = LockResource(hMemory);

		// Font data starts from character 32
		pSinclairFont = (BYTE *) malloc( (size_t) dwSize + ( 32 * 8 ) );

		memcpy( &pSinclairFont[ 32 * 8 ], lpAddress, dwSize );
	}
}

SINCLAIRDLL_API void *CreateFS( DWORD PUID, DataSource *pSource )
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

	switch ( PUID )
	{
	case FSID_SPECTRUM_TAP:
		pFS = (void *) new TAPFileSystem( pSource );
		break;
	case FSID_SPECTRUM_TZX:
		pFS = (void *) new SinclairTZXFileSystem( pSource );
		break;
	case FSID_DOS3:
		pFS = (void *) new DOS3FileSystem( pSource );
		break;

	case FSID_TRD_80SS:
	case FSID_TRD_40SS:
	case FSID_TRD_80DS:
	case FSID_TRD_40DS:
		{
			TRDFileSystem *pTRD = new TRDFileSystem( pSource );
			pFS = (void *) pTRD;

			pTRD->FSID = PUID;
		}
		break;
	};

	return pFS;
}

SINCLAIRDLL_API void *CreateTranslator( DWORD TUID )
{
	void *pXlator = nullptr;

	if ( TUID == GRAPHIC_SPECTRUM )
	{
		pXlator = (void *) new SpectrumSCREENTranslator();
	}

	if ( TUID == BASIC_SPECTRUM )
	{
		pXlator = (void *) new SpectrumBASICTranslator();
	}

	return pXlator;
}

NUTSProvider ProviderSinclair = { L"Sinclair", 0, 0 };

WCHAR *pSinclairFontName = L"BBC Micro";

SINCLAIRDLL_API int NUTSCommandHandler( PluginCommand *cmd )
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
			cmd->OutParams[ 0 ].pPtr = (void *) &ProviderSinclair;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;
		
	case PC_ReportFileSystems:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].Value = SINCLAIR_FSCOUNT;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_DescribeFileSystem:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &SinclairFS[ cmd->InParams[ 1 ].Value ];
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
		ENCODING_SINCLAIR  = cmd->InParams[ 0 ].Value + 0;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportFonts:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetFontPointer:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr  = (void *) pSinclairFont;
			cmd->OutParams[ 1 ].pPtr  = (void *) pSinclairFontName;
			cmd->OutParams[ 2 ].Value = ENCODING_SINCLAIR;
			cmd->OutParams[ 3 ].Value = NULL;

			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

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

	case PC_ReportFSFileTypeCount:
		cmd->OutParams[ 0 ].Value = 4;

		return NUTS_PLUGIN_SUCCESS;

	case PC_SetFSFileTypeBase:
		{
			DWORD Base = cmd->InParams[ 0 ].Value;

			FT_SINCLAIR     = Base + 0;
			FT_SINCLAIR_DOS = Base + 1;
			FT_SINCLAIR_TZX = Base + 2;
			FT_SINCLAIR_TRD = Base + 3;
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportTranslators:
		cmd->OutParams[ 0 ].Value = TRANSLATOR_COUNT;

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeTranslator:
		{
			BYTE tx = (BYTE) cmd->InParams[ 0 ].Value;

			Translators[ tx ].TUID = cmd->InParams[ 1 ].Value;

			cmd->OutParams[ 0 ].pPtr  = (void *) Translators[ tx ].FriendlyName.c_str();
			cmd->OutParams[ 1 ].Value = Translators[ tx ].Flags;
			cmd->OutParams[ 2 ].Value = Translators[ tx ].ProviderID;

			BASIC_SPECTRUM   = MAKEFSID( cmd->InParams[ 2 ].Value, 0, Translators[ 0 ].TUID );
			GRAPHIC_SPECTRUM = MAKEFSID( cmd->InParams[ 2 ].Value, 0, Translators[ 1 ].TUID );
		}

		return NUTS_PLUGIN_SUCCESS;

	case PC_CreateTranslator:
		cmd->OutParams[ 0 ].pPtr = CreateTranslator( cmd->InParams[ 0 ].Value );

		return NUTS_PLUGIN_SUCCESS;
	}

	return NUTS_PLUGIN_UNRECOGNISED;
}