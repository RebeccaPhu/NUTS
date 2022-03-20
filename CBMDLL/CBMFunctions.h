#pragma once

#include "../NUTS/NativeFile.h"
#include "BAM.h"

extern const BYTE spt[41];

DWORD SectorForLink( BYTE track, BYTE sector );
void TToString( BYTE *pString, WORD Len );
void TFromString( BYTE *pString, WORD Len );
void SwapChars( BYTE *pString, WORD Len );
TSLink LinkForSector( DWORD Sector );
int MakeASCII( NativeFile *pFile );
void IncomingASCII( NativeFile *pFile );
