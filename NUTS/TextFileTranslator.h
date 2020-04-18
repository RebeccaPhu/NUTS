#pragma once
#include "texttranslator.h"
class TextFileTranslator : public TEXTTranslator
{
public:
	TextFileTranslator(void);
	~TextFileTranslator(void);

public:
	int TranslateText( CTempFile &FileObj, TXTTranslateOptions *opts );
};
