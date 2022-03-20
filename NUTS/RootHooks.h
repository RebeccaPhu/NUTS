#ifndef ROOTHOOKS_H
#define ROOTHOOKS_H

#include "stdafx.h"

#include <vector>

#include "NUTSTypes.h"

typedef struct _RootCommand {
	PluginIdentifier PLID;
	DWORD CmdIndex;
	std::wstring Text;
} RootCommand;

typedef enum _RootCommandResult {
	GC_ResultNone        = 0,
	GC_ResultRefresh     = 1,
	GC_ResultRootRefresh = 2,
} RootCommandResult;

typedef std::vector<RootCommand> RootCommandSet;
typedef std::vector<RootCommand>::iterator RootCommandSetIterator;


#endif