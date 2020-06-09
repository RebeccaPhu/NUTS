#pragma once

#include "../NUTS/Defs.h"

class ADFSDirectoryCommon
{
public:
	ADFSDirectoryCommon(void);
	~ADFSDirectoryCommon(void);

public:
	void TranslateType( NativeFile *file );
};

