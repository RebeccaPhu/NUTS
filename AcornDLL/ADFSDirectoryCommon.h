#pragma once

#include "../NUTS/Defs.h"
#include "../NUTS/DataSource.h"
#include "TranslatedSector.h"

class ADFSDirectoryCommon : public TranslatedSector
{
public:
	ADFSDirectoryCommon(void);
	~ADFSDirectoryCommon(void);

public:
	void TranslateType( NativeFile *file );

};

