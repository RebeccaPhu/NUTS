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

#define FONTID_PC437 0x04370001

typedef PluginDescriptor *(*fnGetPluginDescriptor)(void);
typedef FileSystem *(*fnCreateFS)( DWORD, DataSource * );
typedef void *(*fnCreateTranslator)( DWORD );

typedef struct _Plugin {
	HMODULE Handle;
	fnGetPluginDescriptor DescriptorFunc;
	fnCreateFS            CreatorFunc;
	fnCreateTranslator    XlatCreatorFunc;
} Plugin;

typedef struct _FormatMenu {
	std::wstring FS;
	DWORD ID;
} FormatMenu;

typedef struct _FSMenu {
	std::wstring Provider;
	std::vector<FormatMenu> FS;
} FSMenu;

typedef std::vector<Plugin> PluginList;
typedef std::vector<RootHook> RootHookList;

typedef std::vector<TextTranslator>     TextTranslatorList;
typedef std::vector<GraphicTranslator>  GraphicTranslatorList;
typedef TextTranslatorList::iterator    TextTranslatorIterator;
typedef GraphicTranslatorList::iterator GraphicTranslatorIterator;
typedef RootHookList::iterator          RootHookIterator;

class CPlugins
{
public:
	CPlugins();
	~CPlugins();

public:
	void LoadPlugins();
	FSHints FindFS( DataSource *pSource, NativeFile *pFile = nullptr );
	FSHints FindFS( NativeFile *pImage );
	FileSystem *FindAndLoadFS( DataSource *pSource, NativeFile *pFile = nullptr );
	FileSystem *LoadFS( DWORD FSID, DataSource *pSource, bool Initialise = true );
	std::wstring FSName( DWORD FSID );
	std::vector<PluginDescriptor> GetPluginList();
	void *LoadFont( DWORD FontID );
	DWORD FindFont( DWORD Encoding, BYTE Index );
	void NextFont( DWORD Encoding, BYTE Index );
	WCHAR *FontName( DWORD FontID );
	std::vector<FSMenu> GetFSMenu();
	void *LoadGraphicTranslator( DWORD TUID );
	void *LoadTextTranslator( DWORD TUID );

	ProviderList GetProviders( void );
	FormatList GetFormats( DWORD PUID );
	TextTranslatorList GetTextTranslators( DWORD PUID );
	GraphicTranslatorList GetGraphicTranslators( DWORD PUID );
	RootHookList GetRootHooks();

	std::vector<DWORD> FontListForEncoding( DWORD Encoding );

private:
	std::vector<PluginDescriptor> PluginDescriptors;
	std::vector<FSDescriptor>     FSDescriptors;
	std::vector<FontDescriptor>   FontDescriptors;
	PluginList Plugins;

	std::map<DWORD, std::vector<DWORD>> EncodingFontMap;
	std::map<DWORD, BYTE> EncodingFontSelectors[2];

private:
	void LoadPlugin( char *plugin );

	void *pPC437Font;

	DWORD IconID;
};

extern CPlugins FSPlugins;

#endif
