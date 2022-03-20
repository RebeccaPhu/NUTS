#ifndef MACICON_H
#define MACICON_H

#include "../NUTS/TempFile.h"
#include "../NUTS/Directory.h"

void ExtractIcon( NativeFile *pFile, CTempFile &fileobj, Directory *pDirectory );
void MakeIconFromData( BYTE *InBuffer, IconDef *pIcon );

#endif
