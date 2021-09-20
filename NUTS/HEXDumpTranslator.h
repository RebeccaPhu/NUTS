#pragma once

#include "texttranslator.h"

class HEXDumpTranslator :
	public TEXTTranslator
{
public:
	HEXDumpTranslator(void);
	~HEXDumpTranslator(void);

public:
	int TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts );

private:
	void PlaceHex( DWORD hexVal, int digits, BYTE *pPlace );
};

