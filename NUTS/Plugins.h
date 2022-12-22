#ifndef PLUGINS_H
#define PLUGINS_H

#include "stdafx.h"

#include "PluginDescriptor.h"
#include "FileSystem.h"
#include "DataSource.h"
#include "RootHooks.h"
#include <vector>
#include <map>
#include <string>

#include "SCREENTranslator.h"

typedef struct _FormatMenu {
	std::wstring FS;
	FSIdentifier ID;
} FormatMenu;

typedef struct _FSMenu {
	std::wstring Provider;
	std::vector<FormatMenu> FS;
} FSMenu;

typedef int (*fnNUTSPluginFunction)(PluginCommand *);

typedef struct _NUTSPlugin {
	HMODULE              Handle;
	fnNUTSPluginFunction CommandHandler;
	PluginIdentifier     PluginID;
	DWORD                MaxAPI;
} NUTSPlugin;

typedef std::vector<NUTSProvider> NUTSProviderList;
typedef std::vector<NUTSProvider>::iterator NUTSProvider_iter;
typedef std::vector<FSDescriptor> FSDescriptorList;
typedef std::vector<FSDescriptor>::iterator FSDescriptor_iter;
typedef std::vector<WrapperDescriptor> WrapperList;
typedef std::vector<WrapperDescriptor>::iterator Wrapper_iter;
typedef std::map<FontIdentifier, BYTE *> NUTSFontList;
typedef std::map<FontIdentifier, BYTE *>::iterator FontList_iter;
typedef std::map<FontIdentifier, std::wstring> NUTSFontNames;
typedef std::map<FontIdentifier, std::wstring>::iterator FontName_iter;
typedef std::map<FontIdentifier, PluginIdentifier> PluginFontMap;
typedef std::map<FontIdentifier, PluginIdentifier> PluginFontMap_iter;
typedef std::map<ProviderIdentifier, std::vector<FSIdentifier>> ProviderFSMap;
typedef std::map<PluginIdentifier, std::vector<FSIdentifier>> PluginFSMap;
typedef std::map<WrapperIdentifier, PluginIdentifier> WrapperMap;

typedef std::vector<NUTSPlugin> PluginList;
typedef std::vector<NUTSPlugin>::iterator Plugin_iter;
typedef std::vector<RootHook> RootHookList;

typedef std::vector<DataTranslator> TranslatorList;
typedef TranslatorList::iterator    TranslatorIterator;
typedef RootHookList::iterator      RootHookIterator;

typedef std::map<std::wstring,std::wstring> FOPDirectoryTypes;
typedef FOPDirectoryTypes::iterator         FOPDirectoryTypeIterator;

class CPlugins
{
public:
	CPlugins();
	~CPlugins();

public:
	void LoadPlugins();
	FSHints FindFS( DataSource *pSource, NativeFile *pFile = nullptr );
	FileSystem *FindAndLoadFS( DataSource *pSource, NativeFile *pFile = nullptr );
	FileSystem *LoadFS( FSIdentifier FSID, DataSource *pSource );
	FileSystem *LoadFSWithWrappers( FSIdentifier FSID, DataSource *pSource );
	DataSource *LoadHookDataSource( FSIdentifier FSID, DataSource *pSource );
	std::wstring ProviderName( ProviderIdentifier PRID );
	std::wstring FSName( FSIdentifier FSID );
	std::wstring FSExt( FSIdentifier FSID );
	std::vector<FSMenu> GetFSMenu();
	void *LoadTranslator( TXIdentifier TUID );
	DataSource *LoadWrapper( WrapperIdentifier wrapper, DataSource *pSource );

	FSDescriptor      GetFSDesc( FSIdentifier FSID );
	NUTSProviderList  GetProviders( void );
	FSDescriptorList  GetFilesystems( ProviderIdentifier ProviderID );
	FormatList        GetFormats( ProviderIdentifier ProviderID, bool ExcludeUnimageable = false );
	TranslatorList    GetTranslators( ProviderIdentifier PVID, DWORD Type );
	RootHookList      GetRootHooks();
	RootCommandSet    GetRootCommands();
	FOPDirectoryTypes GetDirectoryTypes();

	void  *LoadFont( FontIdentifier ReqFontID );
	FontIdentifier FindFont( EncodingIdentifier Encoding, BYTE Index );
	void  NextFont( EncodingIdentifier Encoding, BYTE Index );
	std::wstring FontName( FontIdentifer ReqFontID );

	std::vector<FontIdentifier> FontListForEncoding( EncodingIdentifier Encoding );
	NUTSFontNames      FullFontList( void );

	int PerformRootCommand( HWND hWnd, PluginIdentifier PUID, DWORD CmdIndex );

	bool ProcessFOP( FOPData *_FOPData );

	std::wstring GetCharacterDescription( FontIdentifier FontID, BYTE Char );

	void UnloadPlugins();

	NUTSPlugin *GetPlugin( FSIdentifier FSID );
	NUTSPlugin *GetTXPlugin( TXIdentifier TXID );
	NUTSPlugin *GetPluginByID( PluginIdentifier PLID );

	std::wstring GetProviderNameByID( ProviderIdentifier prid );

	std::vector<NUTSPortRequirement> GetPortRequirements( void );

	std::wstring GetSplash( )
	{
		std::wstring t;

		EnterCriticalSection( &SplashLock );

		t = PluginSplash;

		LeaveCriticalSection( &SplashLock );

		return t;
	}

	WrapperList GetWrappers();
	std::wstring GetWrapperName( WrapperIdentifier wrapper );

	void SetPortConfiguration( void );

	FOPTranslateFunction GetProcessFOP();
	FOPLoadFSFunction GetFOPLoadFS();

private:
	NUTSProviderList  Providers;
	FSDescriptorList  FSDescriptors;
	ProviderFSMap     ProviderFS;
	PluginFSMap       PluginFS;
	NUTSFontList      FontList;
	NUTSFontNames     FontNames;
	PluginFontMap     FontMap;
	PluginList        Plugins;
	TranslatorList    Translators;
	RootHookList      RootHooks;
	RootCommandSet    RootCommands;
	FOPDirectoryTypes DirectoryTypes;
	WrapperList       Wrappers;
	WrapperMap        WrapperPluginMap;

	std::map<EncodingIdentifier, std::vector<FontIdentifier>> EncodingFontMap;
	std::map<EncodingIdentifier, BYTE> EncodingFontSelectors[2];

private:
	void LoadPlugin( WCHAR *plugin );
	void GetFileSystemDetails( NUTSPlugin *plugin, BYTE pid, BYTE fsid, ProviderIdentifier PVID, PluginIdentifier PLID );
	void LoadImageExtensions( NUTSPlugin *plugin );
	void LoadFonts( NUTSPlugin *plugin );
	void LoadIcons( NUTSPlugin *plugin );
	void LoadTranslators( NUTSPlugin *plugin );
	void LoadRootHooks( NUTSPlugin *plugin );
	void LoadRootCommands( NUTSPlugin *plugin );
	void LoadFOPDirectoryTypes( NUTSPlugin *plugin );
	void LoadWrappers( NUTSPlugin *plugin );

	void *pPC437Font;

	CRITICAL_SECTION SplashLock;

	std::wstring PluginSplash;

	DWORD IconID;

	void SetSplash( std::wstring t )
	{
		EnterCriticalSection( &SplashLock );

		PluginSplash = t;

		LeaveCriticalSection( &SplashLock );
	}
};

extern CPlugins FSPlugins;

#endif
