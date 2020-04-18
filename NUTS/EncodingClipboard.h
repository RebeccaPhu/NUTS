#pragma once

#include <time.h>

extern BYTE *pClipboard;
extern time_t ClipStamp;
extern time_t EClipStamp;
extern DWORD  EClipLen;

void SetEncodedClipboard( BYTE *pData, DWORD Length );