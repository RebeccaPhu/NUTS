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
		/* .Flags        = */ FSF_Formats_Raw | FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		256, 256 * 16 * 80 * 2
	},
	{
		/* .FriendlyName = */ L"TR-DOS 40T Double Sided",
		/* .PUID         = */ FSID_TRD_40DS,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		256, 256 * 16 * 40 * 2
	},
	{
		/* .FriendlyName = */ L"TR-DOS 80T Single Sided",
		/* .PUID         = */ FSID_TRD_80SS,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		256, 256 * 16 * 80 * 1
	},
	{
		/* .FriendlyName = */ L"TR-DOS 40T Single Sided",
		/* .PUID         = */ FSID_TRD_40SS,
		/* .Flags        = */ FSF_Formats_Raw | FSF_Formats_Image | FSF_Creates_Image | FSF_UseSectors | FSF_DynamicSize | FSF_SupportFreeSpace | FSF_SupportBlocks | FSF_Size | FSF_Capacity,
		256, 256 * 16 * 40 * 1
	}
};

#define SINCLAIR_FSCOUNT ( sizeof(SinclairFS) / sizeof( FSDescriptor) )

std::wstring ImageExtensions[] = { L"DSK", L"TRD", L"TAP", L"TZX" };

#define IMAGE_EXT_COUNT ( sizeof(ImageExtensions) / sizeof( std::wstring ) )

DataTranslator Translators[] = {
	{ 0, L"ZX Spectrum BASIC", 0, TXTextTranslator },
	{ 0, L"ZX Spectrum",       0, TXGFXTranslator  }
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
		pSinclairFont = (BYTE *) malloc( 256 * 8 );

		ZeroMemory( pSinclairFont, 256 * 8 );

		memcpy( &pSinclairFont[ 32 * 8 ], lpAddress, dwSize );

		// Now generate some special chars

		// Block Graphics
		for ( BYTE c=0x80; c<0x90; c++ )
		{
			BYTE *pChar = &pSinclairFont[ c * 8 ];

			for (BYTE r=0; r<8; r++ )
			{
				BYTE bg = 0;

				if ( ( c & 1 ) && ( r < 4 ) ) { bg |= 0x0F; }
				if ( ( c & 2 ) && ( r < 4 ) ) { bg |= 0xF0; }
				if ( ( c & 4 ) && ( r > 3 ) ) { bg |= 0x0F; }
				if ( ( c & 8 ) && ( r > 3 ) ) { bg |= 0xF0; }

				pChar[ r ] = bg;
			}
		}

		// UDGs
		for ( BYTE c=0x90; c<0xA5; c++ )
		{
			BYTE *pChar = &pSinclairFont[ c * 8 ];
			BYTE *pSrc  = &pSinclairFont[ ( c - 0x4F ) * 8 ];

			for (BYTE r=0; r<8; r++ )
			{
				BYTE udg = pSrc[ r ];

				if ( r == 0 ) { udg |= 0x55; }
				else if ( r == 7 ) { udg |= 0xAA; }
				else if ( r & 1 ) { udg |= 0x80; }
				else { udg |= 0x01; }

				pChar[ r ] = udg;
			}
		}

		// Keywords
		for ( BYTE c=0xA5; c!=0x00; c++ )
		{
			BYTE *pChar = &pSinclairFont[ c * 8 ];
			BYTE *pSrc  = &pSinclairFont[ 'K' * 8 ];

			for (BYTE r=0; r<8; r++ )
			{
				if ( r == 0 ) { pChar[ 0 ] = 0xFF; }
				else if ( r == 7 ) { pChar[ 7 ] = 0xFF; }
				else { pChar[ r ] = pSrc[ r ] | 0x81; }
			}
		}

		// Finally, control chars
		for ( BYTE c=0x00; c!=0x20; c++ )
		{
			BYTE *pChar = &pSinclairFont[ c * 8 ];
			BYTE *pSrc  = &pSinclairFont[ '?' * 8 ];

			for (BYTE r=0; r<8; r++ )
			{
				BYTE cc = pSrc[ r ];

				if ( r == 0 ) { cc |= 0x55; }
				else if ( r == 7 ) { cc |= 0xAA; }
				else if ( r & 1 ) { cc |= 0x80; }
				else { cc |= 0x01; }

				pChar[ r ] = cc;
			}
		}
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

WCHAR *DescribeChar( BYTE Char )
{
	static WCHAR desc[ 256 ];

	std::wstring sd;

	if ( ( Char >= 0 ) && ( Char < 0x20 ) )
	{
		static WCHAR *controls[] = {
			L"None", L"None", L"None", L"None", L"True Video", L"Inverse Video", L"PRINT comma", L"EDIT", L"Cursor Left", L"Cursor Right",
			L"Cursor down", L"Cursor Up", L"DELETE", L"ENTER", L"number", L"GRAPHICS MODE", L"INK control", L"PAPER control", L"FLASH control",
			L"BRIGHT control", L"INVERSE control", L"OVER control", L"AT control", L"TAB control", L"None", L"None", L"None", L"None", L"None",
			L"None", L"None", L"None"
		};

		sd = L"Control Code: " + std::wstring( controls[ Char ] );
	}
	else if ( Char == 0x60 )
	{
		sd = L"Pound sign";
	}
	else if ( ( Char >= 0x20 ) && ( Char <= 0x7E ) )
	{
		WCHAR x[2] = { (WCHAR) Char, 0 };

		sd = L"ASCII Character '" + std::wstring( x ) + L"'";
	}
	else if ( Char == 0x7F )
	{
		sd = L"Copyright Symbol";
	}
	else if ( ( Char >= 0x80 ) && ( Char <= 0x8F ) )
	{
		sd = L"Block graphics character " + std::to_wstring( (long double) Char - 0x80 );
	}
	else if ( ( Char >= 0x90 ) && ( Char <= 0xA4 ) )
	{
		WCHAR x[2] = { (WCHAR) Char - 0x4F, 0 };

		sd = L"User defined graphics character '" + std::wstring( x ) + L"'";
	}
	else
	{
		static WCHAR *keywords[] = {
			L"RND", L"INKEY$", L"PI", L"FN", L"POINT", L"SCREEN$", L"ATTR", L"AT", L"TAB", L"VAL$", L"CODE",
			L"VAL", L"LEN", L"SIN", L"COS", L"TAN", L"ASN", L"ACS", L"ATN", L"LN", L"EXP", L"INT", L"SQR", L"SGN", L"ABS", L"PEEK", L"IN",
			L"USR", L"STR$", L"CHR$", L"NOT", L"BIN", L"OR", L"AND", L"<=", L">=", L"<>", L"LINE", L"THEN", L"TO", L"STEP", L"DEF FN", L"CAT",
			L"FORMAT", L"MOVE", L"ERASE", L"OPEN #", L"CLOSE #", L"MERGE", L"VERIFY", L"BEEP", L"CIRCLE", L"INK", L"PAPER", L"FLASH", L"BRIGHT", L"INVERSE", L"OVER", L"OUT",
			L"LPRINT", L"LLIST", L"STOP", L"READ", L"DATA", L"RESTORE", L"NEW", L"BORDER", L"CONTINUE", L"DIM", L"REM", L"FOR", L"GO TO", L"GO SUB", L"INPUT", L"LOAD",
			L"LIST", L"LET", L"PAUSE", L"NEXT", L"POKE", L"PRINT", L"PLOT", L"RUN", L"SAVE", L"RANDOMIZE", L"IF", L"CLS", L"DRAW", L"CLEAR", L"RETURN", L"COPY"
		};
	
		sd = L"ZX Spectrum BASIC Keyword '" + std::wstring( keywords[ Char - 0xA5 ])  + L"'";
	}
	
	wcscpy( desc, sd.c_str() );

	return desc;
}

NUTSProvider ProviderSinclair = { L"Sinclair", 0, 0 };

WCHAR *pSinclairFontName = L"Sinclair";

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
		cmd->OutParams[ 2 ].Value = ( cmd->InParams[ 0 ].Value > 1 )?FT_TapeImage:FT_DiskImage;

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

	case PC_DescribeChar:
		{
			BYTE Char = (BYTE) cmd->InParams[ 1 ].Value;

			cmd->OutParams[ 0 ].pPtr = DescribeChar( Char );
		}
		return NUTS_PLUGIN_SUCCESS;
	}

	return NUTS_PLUGIN_UNRECOGNISED;
}