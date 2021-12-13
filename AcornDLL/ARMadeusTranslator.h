#pragma once

#include "../NUTS/audiotranslator.h"
#include "Defs.h"

class ARMadeusTranslator :
	public AUDIOTranslator
{
public:
	ARMadeusTranslator(void);
	~ARMadeusTranslator(void);

public:
	int Translate( CTempFile &obj, CTempFile &Output, AudioTranslateOptions *tx );
};

