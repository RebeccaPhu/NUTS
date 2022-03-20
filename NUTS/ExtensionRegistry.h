#pragma once

#include <string>
#include <map>

#include "NativeFile.h"

typedef struct _ExtDef {
	FileType Type;
	FileType Icon;
} ExtDef;

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
	void UnloadExtensions(void);
};


extern ExtensionRegistry ExtReg;