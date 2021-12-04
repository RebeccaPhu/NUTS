#ifndef PLUGINS_H
#define PLUGINS_H

#include "stdafx.h"

#include "PluginDescriptor.h"
#include "FileSystem.h"
#include "DataSource.h"
#include <vector>
#include <map>
#include <string>

#include "SCREENTranslator.h"

#define FONTID_PC437 0x00000437

typedef int (*fnNUTSPluginFunction)(PluginCommand *);

typedef struct _NUTSPlugin {
	HMODULE Handle;
	fnNUTSPluginFunction   CommandHandler;
	DWORD                  PluginID;
} NUTSPlugin;

typedef std::vector<NUTSProvider> NUTSProviderList;
typedef std::vector<NUTSProvider>::iterator NUTSProvider_iter;
typedef std::vector<FSDescriptor> FSDescriptorList;
typedef std::vector<FSDescriptor>::iterator FSDescriptor_iter;
typedef std::map<DWORD, BYTE *> NUTSFontList;
typedef std::map<DWORD, BYTE *>::iterator FontList_iter;
typedef std::map<DWORD, std::wstring> NUTSFontNames;
typedef std::map<DWORD, std::wstring>::iterator FontName_iter;
typedef std::map<DWORD, DWORD> PluginFontMap;
typedef std::map<DWORD, DWORD> PluginFontMap_iter;
typedef std::map< DWORD, std::vector< QWORD > > FSImageOffsets;

typedef std::vector<NUTSPlugin> PluginList;
typedef std::vector<NUTSPlugin>::iterator Plugin_iter;
typedef std::vector<RootHook> RootHookList;

typedef std::vector<DataTranslator> TranslatorList;
typedef TranslatorList::iterator    TranslatorIterator;
typedef RootHookList::iterator      RootHookIterator;

class CPlugins
{
public:
	CPlugins();
	~CPlugins();

public:
	void LoadPlugins();
	FSHints FindFS( DataSource *pSource, NativeFile *pFile = nullptr );
	FileSystem *FindAndLoadFS( DataSource *pSource, NativeFile *pFile = nullptr );
	FileSystem *LoadFS( DWORD FSID, DataSource *pSource );
	std::wstring ProviderName( DWORD PRID );
	std::wstring FSName( DWORD FSID );
	std::vector<FSMenu> GetFSMenu();
	void *LoadTranslator( DWORD TUID );

	NUTSProviderList  GetProviders( void );
	FSDescriptorList  GetFilesystems( DWORD ProviderID );
	FormatList        GetFormats( DWORD PUID );
	TranslatorList    GetTranslators( DWORD PUID, DWORD Type );
	RootHookList      GetRootHooks();
	RootCommandSet    GetRootCommands();

	void  *LoadFont( DWORD ReqFontID );
	DWORD FindFont( DWORD Encoding, BYTE Index );
	void  NextFont( DWORD Encoding, BYTE Index );
	WCHAR *FontName( DWORD ReqFontID );

	std::vector<DWORD> FontListForEncoding( DWORD Encoding );
	NUTSFontNames      FullFontList( void );

	int PerformRootCommand( HWND hWnd, DWORD PUID, DWORD CmdIndex );

	bool ProcessFOP( FOPData *_FOPData );

	std::wstring GetCharacterDescription( DWORD FontID, BYTE Char );

	void UnloadPlugins();

	NUTSPlugin *GetPlugin( DWORD FSID );

	std::wstring GetSplash( )
	{
		std::wstring t;

		EnterCriticalSection( &SplashLock );

		t = PluginSplash;

		LeaveCriticalSection( &SplashLock );

		return t;
	}


private:
	NUTSProviderList  Providers;
	FSDescriptorList  FSDescriptors;
	NUTSFontList      FontList;
	NUTSFontNames     FontNames;
	PluginFontMap     FontMap;
	FSImageOffsets    ImageOffsets;
	PluginList        Plugins;
	TranslatorList    Translators;
	RootHookList      RootHooks;
	RootCommandSet    RootCommands;

	std::map<DWORD, std::vector<DWORD>> EncodingFontMap;
	std::map<DWORD, BYTE> EncodingFontSelectors[2];

private:
	void LoadPlugin( WCHAR *plugin );
	void GetFileSystemDetails( NUTSPlugin *plugin, BYTE pid, BYTE fsid );
	void LoadImageExtensions( NUTSPlugin *plugin );
	void LoadFonts( NUTSPlugin *plugin );
	void LoadIcons( NUTSPlugin *plugin );
	void LoadTranslators( NUTSPlugin *plugin );
	void LoadRootHooks( NUTSPlugin *plugin );
	void LoadRootCommands( NUTSPlugin *plugin );

	void *pPC437Font;

	DWORD IconID;
	DWORD PluginID;
	DWORD EncodingID;
	DWORD FontID;
	DWORD FSFTID;
	DWORD TXID;

	CRITICAL_SECTION SplashLock;

	std::wstring PluginSplash;

	void SetSplash( std::wstring t )
	{
		EnterCriticalSection( &SplashLock );

		PluginSplash = t;

		LeaveCriticalSection( &SplashLock );
	}
};

extern CPlugins FSPlugins;

#endif
