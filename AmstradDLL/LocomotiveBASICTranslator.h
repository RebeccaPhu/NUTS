#pragma once

#include "../NUTS/TEXTTranslator.h"

class LocomotiveBASICTranslator :
	public TEXTTranslator
{
public:
	LocomotiveBASICTranslator(void);
	~LocomotiveBASICTranslator(void);

	int TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts );

	float FloatFromMantissa( DWORD Mantissa, BYTE Exponent );
};

