#pragma once

#include "../NUTS/TEXTTranslator.h"

class BBCBASICTranslator :
	public TEXTTranslator
{
public:
	BBCBASICTranslator(void);
	~BBCBASICTranslator(void);

public:
	int TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts );
};

