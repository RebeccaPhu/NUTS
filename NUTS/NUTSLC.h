#ifndef NUTSLC_H
#define NUTSLC_H

#include "stdafx.h"

#include <vector>
#include <string>

typedef enum _LCFlags {
	LC_IsChecked   = 0x00000001, /* Menu entry has a check mark */
	LC_IsSeparator = 0x00000002, /* Menu entry is a separator line */
	LC_ApplyNone   = 0x00000004, /* Menu entry applies when NO FS items are selected */
	LC_ApplyOne    = 0x00000008, /* Menu entry applies when ONE FS item is selected */
	LC_ApplyMany   = 0x00000010, /* Menu entry applies when MORE THAN ONE FS item are selected */
	LC_Disabled    = 0x00000020, /* Menu entry is disabled */
    LC_Always      = 0x0000001C, /* Menu entry applies REGARDLESS of how many FS items are selected */
	LC_OneOrMore   = 0x00000018, /* Menu entry applies when ONE OR MORE FS items are selected */
} LCFlags;

typedef struct _LocalCommand {
	DWORD Flags;
	std::wstring Name;
} LocalCommand;

typedef struct _LocalCommands {
	bool HasCommandSet;
	std::wstring Root;
	std::vector<LocalCommand> CommandSet;
} LocalCommands;

#endif