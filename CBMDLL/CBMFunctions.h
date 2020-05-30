#pragma once

#include "../NUTS/Defs.h"
#include "BAM.h"

extern const BYTE spt[41];

DWORD SectorForLink( BYTE track, BYTE sector );
void SwapChars( BYTE *pString, WORD Len );
TSLink LinkForSector( DWORD Sector );
int MakeASCII( NativeFile *pFile );
void IncomingASCII( NativeFile *pFile );
