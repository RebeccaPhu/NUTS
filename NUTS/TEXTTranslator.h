#pragma once

#include "stdafx.h"

#include "NUTSXLAT.h"

#include "TempFile.h"

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

