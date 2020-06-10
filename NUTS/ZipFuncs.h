#pragma once

#include "archive.h"

WCHAR *ZIPError( archive *a );
bool IsCPath( BYTE *path, BYTE *name );
BYTE *ZIPSubPath( BYTE *path, BYTE *name );
BYTE *ZIPSubName( BYTE *path, BYTE *name );