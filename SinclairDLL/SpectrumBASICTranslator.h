#pragma once

#include "../NUTS/TEXTTranslator.h"

class SpectrumBASICTranslator :
	public TEXTTranslator
{
public:
	SpectrumBASICTranslator(void);
	~SpectrumBASICTranslator(void);

public:
	int TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts );
};

