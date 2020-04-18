#include "StdAfx.h"
#include "ExtensionRegistry.h"

ExtensionRegistry ExtReg;

ExtensionRegistry::ExtensionRegistry(void)
{
	Mappings.clear();
}


ExtensionRegistry::~ExtensionRegistry(void)
{
}

void ExtensionRegistry::RegisterExtension( std::wstring Ext, FileType Type, FileType Icon )
{
	ExtDef Def;

	Def.Type = Type;
	Def.Icon = Icon;

	Mappings[ Ext ] = Def;
}

ExtDef ExtensionRegistry::GetTypes( std::wstring Ext )
{
	static ExtDef NullDef = { FT_Arbitrary, FT_Arbitrary };

	if ( Mappings.find( Ext ) == Mappings.end() )
	{
		return NullDef;
	}

	return Mappings[ Ext ];
}
