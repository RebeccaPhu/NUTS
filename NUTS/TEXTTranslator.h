#pragma once

#include "TempFile.h"
#include "Defs.h"

class TEXTTranslator
{
public:
	TEXTTranslator(void)
	{
	}


	~TEXTTranslator(void)
	{
	}

public:
	virtual int TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts )
	{
		return 0;
	}
};

