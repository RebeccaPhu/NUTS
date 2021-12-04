#pragma once

#include <string>
#include <vector>

#include "PluginDescriptor.h"
#include "FileSystem.h"
#include "DataSource.h"
#include "Defs.h"

typedef struct _BuiltInTranslator
{
	DWORD TUID;
	DWORD TXFlags;
	std::wstring FriendlyName;
} BuiltInTranslator;

typedef std::vector<BuiltInTranslator> BuiltInTranslators;
typedef std::vector<BuiltInTranslator>::iterator BuiltInTranslator_iter;

typedef std::vector<NUTSProvider> BuiltInProviderList;
typedef std::vector<FSMenu>       BuiltInMenuList;

class BuiltIns
{
public:
	BuiltIns(void);
	~BuiltIns(void);

	BuiltInTranslators GetBuiltInTranslators( void );

	void *LoadTranslator( DWORD TUID );

	BuiltInProviderList GetBuiltInProviders();
	FormatList          GetBuiltinFormatList( DWORD PUID );
	BuiltInMenuList     GetBuiltInMenuList();

	FSHints GetOffers( DataSource *pSource, NativeFile *pFile );

	FileSystem *LoadFS( DWORD FSID, DataSource *pSource );

	std::wstring ProviderName( DWORD PRID );
	std::wstring FSName( DWORD FSID );
};

extern BuiltIns NUTSBuiltIns;

