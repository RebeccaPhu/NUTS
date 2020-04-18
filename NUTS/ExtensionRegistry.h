#pragma once

#include <string>
#include <map>

#include "Defs.h"

class ExtensionRegistry
{
public:
	ExtensionRegistry(void);
	~ExtensionRegistry(void);

private:
	std::map<std::wstring, ExtDef> Mappings;

public:
	void RegisterExtension( std::wstring Ext, FileType Type, FileType Icon );
	ExtDef GetTypes( std::wstring Ext );
};


extern ExtensionRegistry ExtReg;