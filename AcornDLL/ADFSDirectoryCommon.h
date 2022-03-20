#pragma once

#include "../NUTS/DataSource.h"
#include "TranslatedSector.h"

#include "../NUTS/NativeFile.h"

class ADFSDirectoryCommon : public TranslatedSector
{
public:
	ADFSDirectoryCommon(void);
	~ADFSDirectoryCommon(void);

public:
	void TranslateType( NativeFile *file );

};

