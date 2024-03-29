// CBMDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "CBMDLL.h"

#include "D64FileSystem.h"
#include "T64FileSystem.h"
#include "IECATAFileSystem.h"
#include "OpenCBMSource.h"
#include "OpenCBMPlugin.h"

#include "resource.h"

#include "../NUTS/Preference.h"
#include "../NUTS/NUTSError.h"
#include "../NUTS/RootHooks.h"

#include <ShlObj.h>

#include "CBMDefs.h"

/* Don't ask. */
BYTE *pPETSCII = nullptr;
BYTE *pPETSCIIs[2] = { nullptr, nullptr };

HMODULE hInstance;

DataSourceCollector *pCollector;

BYTE *NUTSSignature;

const FTIdentifier FT_C64      = L"C64_File_Object";
const FTIdentifier FT_CBM_TAPE = L"C64_Tape_Object";

const EncodingIdentifier ENCODING_PETSCII = L"PETSCII";

const PluginIdentifier CBM_PLUGINID = L"CommodoreNUTSPlugin";

const ProviderIdentifier CBM_PROVIDER = L"CommodoreProvider";

const FontIdentifier PETSCII1 = L"PETSCII_1";
const FontIdentifier PETSCII2 = L"PETSCII_2";

FSDescriptor CBMFS[] = {
	{
		/* .FriendlyName = */ L"D64 Commodore Disk Image",
		/* .PUID         = */ FSID_D64,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_Imaging,
		256, 1700,
		(BYTE *) "D64"
	},
	{
		/* .FriendlyName = */ L"T64 Commodore Tape Image",
		/* .PUID         = */ FSID_T64,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Supports_Spaces | FSF_DynamicSize | FSF_Reorderable | FSF_Prohibit_Nesting | FSF_Imaging,
		0, 0,
		(BYTE *) "T64"
	},
	{
		/* .FriendlyName = */ L"IEC-ATA Hard Disk",
		/* .PUID         = */ FSID_IECATA,
		/* .Flags        = */ FSF_Creates_Image | FSF_Formats_Image | FSF_Formats_Raw | FSF_Supports_Dirs | FSF_Supports_Spaces | FSF_ArbitrarySize | FSF_UseSectors | FSF_Imaging,
		512, 0,
		(BYTE *) "IMG"
	}
};

#define CBM_FSCOUNT ( sizeof(CBMFS) / sizeof( FSDescriptor) )

std::wstring ImageExtensions[] = { L"D64", L"T64" };

#define IMAGE_EXT_COUNT ( sizeof(ImageExtensions) / sizeof( std::wstring ) )

RootHook RootHooks[ 4 ];

DWORD FontID1 = 0xFFFFFFFF;
DWORD FontID2 = 0xFFFFFFFF;

void LoadFonts()
{
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
			pPETSCIIs[c] = (BYTE *) malloc( 256 * 8 );

			memset( pPETSCIIs[c], 0, 256 * 8 );

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
					&pPETSCIIs[c][ shifts[s][2] * 8 ],
					&pPETSCII[ ( 0x800 * c ) + ( shifts[s][0] * 8 ) ],
					shifts[s][1] * 8
				);

				s++;
			}
		}
	}
}

DWORD NumHooks;

RootHook Hooks[ 4 ];

void LoadHooks( void )
{
	if ( !OpenCBMLoaded )
	{
		LoadOpenCBM( );
	}

	     NumHooks = 0;
	BYTE HookID   = 0;

	if ( OpenCBMLoaded )
	{
		for ( BYTE device=8; device<=11; device++ )
		{
			if ( CBMDeviceBits & ( 1 << device ) )
			{
				Hooks[ HookID ].FriendlyName  = L"OpenCBM/" + std::to_wstring( (long double) device );
				Hooks[ HookID ].HookIcon      = LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_OPENCBM ) );

				RootHookInvocation Invoker;

				Invoker.FriendlyName  = L"Raw Source"; // Enter As
				Invoker.Flags         = RHF_CreatesDataSource;
				Invoker.HookData[ 0 ] = device;

				Hooks[ HookID ].Invocations.push_back( Invoker );

				Invoker.FriendlyName  = L"Raw Disk";
				Invoker.Flags         = RHF_CreatesFileSystem;
				Invoker.HookData[ 0 ] = device;
				Invoker.HookFSID      = FSID_OPENCBM;

				Hooks[ HookID ].Invocations.push_back( Invoker );
				HookID++;

				NumHooks++;
			}
		}
	}
}

/* Hook Icon Creds */
/* Serial Port: Port Icon, FatCow Hosting Icons, FatCow, CC Attribution 3.0 United States */


void *CreateFS( FSIdentifier FSID, DataSource *pSource )
{
	void *pFS = NULL;

	if ( FSID == FSID_D64 )
	{
		pFS = (void *) new D64FileSystem( pSource );
	}
	else if ( FSID == FSID_T64 )
	{
		pFS = (void *) new T64FileSystem( pSource );
	}
	else if ( FSID == FSID_OPENCBM )
	{
		BYTE DriveNum = 0;

		pSource->ReadRaw( 0, 1, &DriveNum );

		OpenCBMSource *pOpenCBMSource = new OpenCBMSource( DriveNum );

		D64FileSystem *newFS = new D64FileSystem( pOpenCBMSource );

		newFS->pDir->NoLengthChase = true;
		newFS->Drive               = DriveNum;
		newFS->pBAM->Drive         = DriveNum;
		newFS->pDir->Drive         = DriveNum;
		newFS->IsOpenCBM           = true;
		newFS->pBAM->IsOpenCBM     = true;
		newFS->pDir->IsOpenCBM     = true;

		pFS = (void *) newFS;

		DS_RELEASE( pOpenCBMSource );

	}
	else if ( FSID == FSID_IECATA )
	{
		pFS = (void *) new IECATAFileSystem( pSource );
	}

	return pFS;
}

int PerformRootCommand( HWND hWnd, DWORD CmdIndex )
{
	WCHAR Path[ MAX_PATH + 1 ] = { 0 };

	BROWSEINFO bi;

	bi.hwndOwner      = hWnd;
	bi.pidlRoot       = NULL;
	bi.pszDisplayName = Path;
	bi.lpszTitle      = L"Select location of OpenCBM plugin (opencbm.dll)";
	bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_EDITBOX | BIF_NONEWFOLDERBUTTON;
	bi.lpfn           = NULL;
	bi.lParam         = NULL;
	bi.iImage         = 0;

	PIDLIST_ABSOLUTE  pPIDL = SHBrowseForFolder( &bi );

	if ( pPIDL != NULL )
	{
		SHGetPathFromIDList( pPIDL, Path );

		Preference( L"OpenCBMPath" ) = std::wstring( Path );

		LoadOpenCBM();

		CoTaskMemFree( pPIDL ); 
	}

	return (int) GC_ResultRootRefresh;
}

WCHAR *DescribeChar( BYTE Char, DWORD FontID )
{
	/* This is complicated because of which font is in use */
	static WCHAR desc[ 256 ];

	std::wstring cd;

	if ( FontID == FontID1 )
	{
		if ( ( Char >= 0 ) && ( Char < 0x20 ) )
		{
			cd = L"Control character " + std::to_wstring( (long double) Char );
		}
		else if ( Char == 0x5C )
		{
			cd = L"Pound sign";
		}
		else if ( ( Char >= 0x20 ) && ( Char <= 0x5D ) )
		{
			WCHAR x[2] = { (WCHAR) Char, 0 };

			cd = L"PETSCII Character '" + std::wstring( x ) + L"'";
		}
		else
		{
			cd = L"PETSCII Extended Character " + std::to_wstring( (long double) Char );
		}
	}
	else
	{
		/* PETSCII 2 has lower case chars where ASCII puts upper case, and vice-versa.
		   It also duplicates the upper case chars at 0xC1 to 0xDA. */
		if ( ( Char >= 0 ) && ( Char < 0x20 ) )
		{
			cd = L"Control character " + std::to_wstring( (long double) Char );
		}
		else if ( Char == 0x5C )
		{
			cd = L"Pound sign";
		}
		else if ( ( Char >= 0x20 ) && ( Char <= 0x40 ) )
		{
			WCHAR x[2] = { (WCHAR) Char, 0 };

			cd = L"PETSCII Character '" + std::wstring( x ) + L"'";
		}
		else if ( ( Char >= 0x41 ) && ( Char <= 0x5D ) )
		{
			WCHAR x[2] = { (WCHAR) Char + 0x20, 0 };

			cd = L"PETSCII Character '" + std::wstring( x ) + L"'";
		}
		else if ( ( Char >= 0x61 ) && ( Char <= 0x7A ) )
		{
			WCHAR x[2] = { (WCHAR) Char - 0x20, 0 };

			cd = L"PETSCII Character '" + std::wstring( x ) + L"'";
		}
		else if ( ( Char >= 0xC1 ) && ( Char <= 0xDA ) )
		{
			WCHAR x[2] = { (WCHAR) Char - 0x80, 0 };

			cd = L"PETSCII Character (high range) '" + std::wstring( x ) + L"'";
		}
		else
		{
			cd = L"PETSCII Extended Character " + std::to_wstring( (long double) Char );
		}
	}

	wcscpy_s( desc, 256, cd.c_str() );

	return desc;
}

NUTSProvider ProviderCBM = { L"Commodore", CBM_PLUGINID, CBM_PROVIDER };

WCHAR *pPETSCII1FontName = L"PETSCII 1";
WCHAR *pPETSCII2FontName = L"PETSCII 2";

WCHAR *OpenCBMPathSet = L"Set OpenCBM Path";

static WCHAR *PluginAuthor = L"Rebecca Gellman";

CBMDLL_API int NUTSCommandHandler( PluginCommand *cmd )
{
	switch ( cmd->CommandID )
	{
	case PC_SetPluginConnectors:
		pCollector   = (DataSourceCollector *) cmd->InParams[ 0 ].pPtr;
		pGlobalError = (NUTSError *)           cmd->InParams[ 1 ].pPtr;
		
		LoadFonts();
		LoadHooks();

		cmd->OutParams[ 0 ].Value = 0x00000001;
		cmd->OutParams[ 1 ].pPtr  = (void *) CBM_PLUGINID.c_str();

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportProviders:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_GetProviderDescriptor:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &ProviderCBM;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;
		
	case PC_ReportFileSystems:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].Value = CBM_FSCOUNT;
			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_DescribeFileSystem:
		if ( cmd->InParams[ 0 ].Value == 0 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) &CBMFS[ cmd->InParams[ 1 ].Value ];
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
			cmd->OutParams[ 0 ].pPtr = (void *) pPETSCIIs[ 0 ];
			cmd->OutParams[ 1 ].pPtr = (void *) pPETSCII1FontName;
			cmd->OutParams[ 2 ].pPtr = (void *) PETSCII1.c_str();
			cmd->OutParams[ 3 ].pPtr = (void *) ENCODING_PETSCII.c_str();
			cmd->OutParams[ 4 ].pPtr = nullptr;

			FontID1 = cmd->InParams[ 1 ].Value;

			return NUTS_PLUGIN_SUCCESS;
		}
		if ( cmd->InParams[ 0 ].Value == 1 )
		{
			cmd->OutParams[ 0 ].pPtr = (void *) pPETSCIIs[ 1 ];
			cmd->OutParams[ 1 ].pPtr = (void *) pPETSCII2FontName;
			cmd->OutParams[ 2 ].pPtr = (void *) PETSCII2.c_str();
			cmd->OutParams[ 2 ].pPtr = (void *) ENCODING_PETSCII.c_str();
			cmd->OutParams[ 3 ].pPtr = nullptr;

			FontID2 = cmd->InParams[ 1 ].Value;

			return NUTS_PLUGIN_SUCCESS;
		}

		return NUTS_PLUGIN_ERROR;

	case PC_ReportFSFileTypeCount:
		cmd->OutParams[ 0 ].Value = 2;

		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportRootHooks:
		cmd->OutParams[ 0 ].Value = NumHooks;

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeRootHook:
		{
			BYTE HookID = (BYTE) cmd->InParams[ 0 ].Value;

			cmd->OutParams[ 0 ].pPtr = &Hooks[ HookID ];
		}
		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportRootCommands:
		cmd->OutParams[ 0 ].Value = 1;

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeRootCommand:
		cmd->OutParams[ 0 ].pPtr = OpenCBMPathSet;

		return NUTS_PLUGIN_SUCCESS;

	case PC_PerformRootCommand:
		cmd->OutParams[ 0 ].Value =	(DWORD) PerformRootCommand(
			(HWND) cmd->InParams[ 0 ].pPtr,
			cmd->InParams[ 1 ].Value
		);

		return NUTS_PLUGIN_SUCCESS;

	case PC_DescribeChar:
		{
			BYTE  Char   = (BYTE) cmd->InParams[ 1 ].Value;
			DWORD FontID =        cmd->InParams[ 0 ].Value;

			cmd->OutParams[ 0 ].pPtr = DescribeChar( Char, FontID );
		}
		return NUTS_PLUGIN_SUCCESS;

	case PC_ReportPluginCreditStats:
		cmd->OutParams[ 0 ].pPtr  = (void *) LoadBitmap( hInstance, MAKEINTRESOURCE( IDB_CBM ) );
		cmd->OutParams[ 1 ].pPtr  = (void *) PluginAuthor;
		cmd->OutParams[ 2 ].Value = 0;
		cmd->OutParams[ 3 ].Value = 0;

		return NUTS_PLUGIN_SUCCESS;

	}

	return NUTS_PLUGIN_UNRECOGNISED;
}