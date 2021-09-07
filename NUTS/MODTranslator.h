#pragma once

#include "audiotranslator.h"
#include "Defs.h"

#include <mikmod.h>

class MODTranslator :
	public AUDIOTranslator
{
public:
	MODTranslator(void);
	~MODTranslator(void);

	int Translate( CTempFile &obj, CTempFile &Output, AudioTranslateOptions *tx );


private:
	static bool Inited;
	static bool HaveLock;

	CRITICAL_SECTION MikModLock;
};

