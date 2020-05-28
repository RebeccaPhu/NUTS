#pragma once

#include "DataSource.h"
#include "TempFile.h"

int ReplaceSourceContent( DataSource *pSource, CTempFile &FileObj );
int CopyContent( CTempFile &srcObj, CTempFile &destObj );