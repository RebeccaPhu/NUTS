#pragma once

#include <string>
#include <vector>

#include "PluginDescriptor.h"
#include "FileSystem.h"
#include "DataSource.h"
#include "Plugins.h"

typedef struct _BuiltInTranslator
{
	TXIdentifier TUID;
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

	void *LoadTranslator( TXIdentifier TUID );

	BuiltInProviderList GetBuiltInProviders();
	FormatList          GetBuiltinFormatList( ProviderIdentifier PUID, bool ExcludeUnimageable = false );
	BuiltInMenuList     GetBuiltInMenuList();

	FSHints GetOffers( DataSource *pSource, NativeFile *pFile );

	FileSystem *LoadFS( FSIdentifier FSID, DataSource *pSource );

	std::wstring ProviderName( ProviderIdentifier PRID );
	std::wstring FSName( FSIdentifier FSID );
	std::wstring FSExt( FSIdentifier FSID );

	WrapperList GetWrappers();

	DataSource *GetWrapper( WrapperIdentifier wrapper, DataSource *pSource );
};

extern BuiltIns NUTSBuiltIns;

