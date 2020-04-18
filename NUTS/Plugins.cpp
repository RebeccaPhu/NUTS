#include "stdafx.h"

#include "Plugins.h"
#include "resource.h"

#include "IDE8Source.h"
#include "ImageDataSource.h"
#include "TextFileTranslator.h"

#include "BitmapCache.h"
#include "ExtensionRegistry.h"

#include <string>

CPlugins FSPlugins;

CPlugins::CPlugins()
{
	Plugins.clear();

	pPC437Font = nullptr;

	IconID = 0x400;
}

CPlugins::~CPlugins()
{
	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		FreeLibrary( iter->Handle );

		iter++;
	}

	Plugins.clear();

	if ( pPC437Font != nullptr )
	{
		free( pPC437Font );
	}
}

void CPlugins::LoadPlugins()
{
	// TODO: Look for plugins with FindFirstFile, etc.
	// For now, hard code this.


	EncodingFontMap[ ENCODING_ASCII ] = std::vector<DWORD>();
	EncodingFontMap[ ENCODING_ASCII ].push_back( FONTID_PC437 );

	EncodingFontSelectors[ 0 ][ ENCODING_ASCII ] = 0;
	EncodingFontSelectors[ 1 ][ ENCODING_ASCII ] = 0;

	LoadPlugin( "AcornDLL.dll" );
	LoadPlugin( "CBMDLL.dll" );
	LoadPlugin( "SinclairDLL.dll" );
}

void CPlugins::LoadPlugin( char *plugin )
{
	HMODULE hModule = LoadLibraryA( plugin );

	if ( hModule != NULL )
	{
		Plugin plugin;

		plugin.Handle          = hModule;
		plugin.DescriptorFunc  = (fnGetPluginDescriptor) GetProcAddress( hModule, "GetPluginDescriptor" );
		plugin.CreatorFunc     = (fnCreateFS)            GetProcAddress( hModule, "CreateFS" );
		plugin.XlatCreatorFunc = (fnCreateTranslator)    GetProcAddress( hModule, "CreateTranslator" );

		Plugins.push_back( plugin );

		PluginDescriptor *D = plugin.DescriptorFunc();

		BYTE i;

		for ( i=0; i<D->NumFonts; i++ )
		{
			DWORD FontID = D->FontDescriptors[i].PUID;

			for ( BYTE e=0; e<16; e++ )
			{
				DWORD Encoding = D->FontDescriptors[i].MatchingEncodings[e];

				if ( Encoding != 0U )
				{
					if ( EncodingFontMap.find( Encoding ) == EncodingFontMap.end() )
					{
						EncodingFontMap[ Encoding ] = std::vector<DWORD>();
					}

					EncodingFontMap[ Encoding ].push_back( FontID );

					EncodingFontSelectors[ 0 ][ Encoding ] = 0;
					EncodingFontSelectors[ 1 ][ Encoding ] = 0;
				}
			}
		}

		for ( i=0; i<D->NumFS; i++ )
		{
			BYTE Icon;

			for ( Icon=0; Icon < D->FSDescriptors[ i ].NumIcons; Icon++ )
			{
				D->FSDescriptors[ i ].IconIDs[ Icon ] = IconID;

				HBITMAP hBitmap = (HBITMAP) D->FSDescriptors[ i ].Icons[ Icon ];

				BitmapCache.AddBitmap( IconID, hBitmap );

				IconID++;
			}

			BYTE Ext;

			for ( Ext=0; Ext < D->FSDescriptors[ i ].NumExts; Ext++ )
			{
				ExtReg.RegisterExtension(
					std::wstring( (WCHAR *) D->FSDescriptors[ i ].Exts[ Ext ] ),
					(FileType) D->FSDescriptors[ i ].IconTypes[ Ext ],
					(FileType) D->FSDescriptors[ i ].IconMaps[  Ext ]
				);
			}
		}

		PluginDescriptors.push_back( *D );
	}
}

std::vector<PluginDescriptor> CPlugins::GetPluginList()
{
	return std::vector<PluginDescriptor>( PluginDescriptors );
}

ProviderList CPlugins::GetProviders( void )
{
	ProviderList Providers;

	PluginList::iterator iPlugin;

	for ( iPlugin = Plugins.begin(); iPlugin != Plugins.end(); iPlugin++ )
	{
		PluginDescriptor *D = iPlugin->DescriptorFunc();

		ProviderDesc Provider;

		Provider.Provider = D->Provider;
		Provider.PUID     = D->PUID;

		Providers.push_back( Provider );
	}

	return Providers;
}

FormatList CPlugins::GetFormats( DWORD PUID )
{
	FormatList Formats;

	PluginList::iterator iPlugin;

	for ( iPlugin = Plugins.begin(); iPlugin != Plugins.end(); iPlugin++ )
	{
		PluginDescriptor *D = iPlugin->DescriptorFunc();

		if ( D->PUID == PUID )
		{
			for ( BYTE i=0; i<D->NumFS; i++ )
			{
				FormatDesc Format;

				Format.Format     = D->FSDescriptors[ i ].FriendlyName;
				Format.FUID       = D->FSDescriptors[ i ].PUID;
				Format.Flags      = D->FSDescriptors[ i ].Flags;
				Format.MaxSize    = D->FSDescriptors[ i ].MaxSize;
				Format.SectorSize = D->FSDescriptors[ i ].SectorSize;

				Formats.push_back( Format );
			}
		}
	}

	return Formats;
}

TextTranslatorList CPlugins::GetTextTranslators( DWORD PUID )
{
	TextTranslatorList Xlators;

	PluginList::iterator iPlugin;

	for ( iPlugin = Plugins.begin(); iPlugin != Plugins.end(); iPlugin++ )
	{
		PluginDescriptor *D = iPlugin->DescriptorFunc();

		if ( ( D->PUID == PUID ) || ( PUID == NULL ) )
		{
			for ( BYTE i=0; i<D->NumTextXlators; i++ )
			{
				Xlators.push_back( D->TextXlators[ i ] );
			}
		}
	}

	return Xlators;
}

GraphicTranslatorList CPlugins::GetGraphicTranslators( DWORD PUID )
{
	GraphicTranslatorList Xlators;

	PluginList::iterator iPlugin;

	for ( iPlugin = Plugins.begin(); iPlugin != Plugins.end(); iPlugin++ )
	{
		PluginDescriptor *D = iPlugin->DescriptorFunc();

		if ( ( D->PUID == PUID ) || ( PUID == NULL ) )
		{
			for ( BYTE i=0; i<D->NumGraphicXlators; i++ )
			{
				Xlators.push_back( D->GraphicXlators[ i ] );
			}
		}
	}

	return Xlators;
}

FSHints CPlugins::FindFS( DataSource *pSource, NativeFile *pFile )
{
	std::vector<FSHint> hints;

	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		PluginDescriptor *D = iter->DescriptorFunc();

		BYTE i;

		for ( i=0; i<D->NumFS; i++ )
		{
			FileSystem *pFS = iter->CreatorFunc( D->FSDescriptors[i].PUID, pSource );

			if ( pFile == nullptr )
			{
				hints.push_back( pFS->Offer( nullptr ) );
			}
			else
			{
				hints.push_back( pFS->Offer( pFile->Extension ) );
			}

			delete pFS;
		}

		iter++;
	}

	return hints;
}

FileSystem *CPlugins::FindAndLoadFS( DataSource *pSource, NativeFile *pFile )
{
	FSHints hints = FindFS( pSource, pFile );

	WORD Confidence = 0;
	DWORD FSID      = FS_Null;
	
	FileSystem *newFS = nullptr;

	for ( FSHints::iterator iHint = hints.begin(); iHint != hints.end(); iHint++ )
	{
		if ( iHint->Confidence > Confidence )
		{
			Confidence = iHint->Confidence;
			FSID       = iHint->FSID;
		}
	}

	if ( FSID != FS_Null )
	{
		newFS = LoadFS( FSID, pSource, false );
	}

	return newFS;
}

FSHints CPlugins::FindFS( NativeFile *pImage )
{
	std::vector<FSHint> hints;

//	ImageDataSource src( tempFile );

//	return FindFS( &src );

	return hints;
}

FileSystem *CPlugins::LoadFS( DWORD FSID, DataSource *pSource, bool Initialise )
{
	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		PluginDescriptor *D = iter->DescriptorFunc();

		BYTE i;

		for ( i=0; i<D->NumFS; i++ )
		{
			if ( D->FSDescriptors[i].PUID == FSID )
			{
				FileSystem *pFS = iter->CreatorFunc( FSID, pSource );

				if ( Initialise )
				{
					pFS->Init();
				}

				return pFS;
			}
		}

		iter++;
	}

	return nullptr;
}

std::wstring CPlugins::FSName( DWORD FSID )
{
	std::wstring name = L"Unknwon File System";

	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		PluginDescriptor *D = iter->DescriptorFunc();

		BYTE i;

		for ( i=0; i<D->NumFS; i++ )
		{
			if ( D->FSDescriptors[i].PUID == FSID )
			{
				name = D->FSDescriptors[i].FriendlyName;

				return name;
			}
		}

		iter++;
	}

	return name;
}

WCHAR *CPlugins::FontName( DWORD FontID )
{
	static WCHAR *PC437Name = L"PC437";

	if ( FontID == FONTID_PC437 )
	{
		return PC437Name;
	}

	char *pFontName = nullptr;

	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		PluginDescriptor *D = iter->DescriptorFunc();

		BYTE i;

		for ( i=0; i<D->NumFonts; i++ )
		{
			if ( D->FontDescriptors[i].PUID == FontID )
			{
				return (WCHAR *) D->FontDescriptors[i].FriendlyName.c_str();
			}
		}

		iter++;
	}

	return PC437Name;
}

void *CPlugins::LoadFont( DWORD FontID )
{
	if ( FontID == FONTID_PC437 )
	{
		if ( pPC437Font == nullptr )
		{
			HMODULE hModule  = GetModuleHandle(NULL);
			HRSRC hResource  = FindResource(hModule, MAKEINTRESOURCE( IDF_PC437 ), RT_RCDATA);
			HGLOBAL hMemory  = LoadResource(hModule, hResource);
			DWORD dwSize     = SizeofResource(hModule, hResource);
			LPVOID lpAddress = LockResource(hMemory);

			pPC437Font = malloc( (size_t) dwSize );

			memcpy( pPC437Font, lpAddress, dwSize );
		}

		return pPC437Font;
	}

	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		PluginDescriptor *D = iter->DescriptorFunc();

		BYTE i;

		for ( i=0; i<D->NumFonts; i++ )
		{
			if ( D->FontDescriptors[i].PUID == FontID )
			{
				return D->FontDescriptors[i].pFontData;
			}
		}

		iter++;
	}

	return nullptr;
}

DWORD CPlugins::FindFont( DWORD Encoding, BYTE Index )
{
	if ( Encoding == ENCODING_ASCII )
	{
		return FONTID_PC437;
	}

	DWORD FontID = EncodingFontMap[ Encoding ][ EncodingFontSelectors[ Index ][ Encoding ] ];
	
	return FontID;
}

std::vector<DWORD> CPlugins::FontListForEncoding( DWORD Encoding )
{
	std::vector<DWORD> FontList;

	if ( EncodingFontMap.find( Encoding ) != EncodingFontMap.end() )
	{
		FontList = EncodingFontMap[ Encoding ];
	}

	return FontList;
}

void CPlugins::NextFont( DWORD Encoding, BYTE Index )
{
	EncodingFontSelectors[ Index ][ Encoding ] =
		( EncodingFontSelectors[ Index ][ Encoding ] + 1 ) %
		EncodingFontMap[ Encoding ].size();
}

std::vector<FSMenu> CPlugins::GetFSMenu()
{
	std::vector<FSMenu> menus;

	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		PluginDescriptor *D = iter->DescriptorFunc();

		FSMenu menu;

		menu.Provider = D->Provider;

		for ( BYTE i=0; i<D->NumFS; i++ )
		{
			FormatMenu format;

			format.FS = D->FSDescriptors[i].FriendlyName;
			format.ID = D->FSDescriptors[i].PUID;

			menu.FS.push_back( format );
		}

		iter++;

		menus.push_back( menu );
	}

	return menus;
}

void *CPlugins::LoadGraphicTranslator( DWORD TUID )
{
	PluginList::iterator iter = Plugins.begin();

	while ( iter != Plugins.end() )
	{
		PluginDescriptor *D = iter->DescriptorFunc();

		BYTE i;

		for ( i=0; i<D->NumGraphicXlators; i++ )
		{
			if ( D->GraphicXlators[i].TUID == TUID )
			{
				SCREENTranslator *pXlator = (SCREENTranslator *) iter->XlatCreatorFunc( TUID );
				
				return pXlator;
			}
		}

		iter++;
	}

	return nullptr;
}

void *CPlugins::LoadTextTranslator( DWORD TUID )
{
	TEXTTranslator *pXlator = nullptr;

	if ( TUID == TUID_TEXT )
	{
		pXlator = new TextFileTranslator();

		return pXlator;
	}
	else
	{
		PluginList::iterator iter = Plugins.begin();

		while ( iter != Plugins.end() )
		{
			PluginDescriptor *D = iter->DescriptorFunc();

			BYTE i;

			for ( i=0; i<D->NumGraphicXlators; i++ )
			{
				if ( D->TextXlators[i].TUID == TUID )
				{
					pXlator = (TEXTTranslator *) iter->XlatCreatorFunc( TUID );
				
					return pXlator;
				}
			}

			iter++;
		}
	}

	return nullptr;
}