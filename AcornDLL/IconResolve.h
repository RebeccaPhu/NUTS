#ifndef ICONRESOLVE_H
#define ICONRESOLVE_H

#include "stdafx.h"

#include "../NUTS/FileSystem.h"

void ResolveAppIcon( FileSystem *pFS, NativeFile *iFile, bool AllowISOFilename = false );
int ResolveAppIcons( FileSystem *pFS );

#endif