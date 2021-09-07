#include "StdAfx.h"
#include "BuiltIns.h"

#include "TextFileTranslator.h"
#include "MODTranslator.h"

#include "Defs.h"

BuiltIns::BuiltIns(void)
{
}


BuiltIns::~BuiltIns(void)
{
}

BuiltInTranslators BuiltIns::GetBuiltInTranslators( void )
{
	BuiltInTranslators tx;

	BuiltInTranslator t;

	t.TUID         = TUID_TEXT;
	t.TXFlags      = TXTextTranslator;
	t.FriendlyName = L"Text File";

	tx.push_back( t );

	t.TUID         = TUID_MOD_MUSIC;
	t.TXFlags      = TXAUDTranslator;
	t.FriendlyName = L"MOD Music File";

	tx.push_back( t );

	return tx;
}

void *BuiltIns::LoadTranslator( DWORD TUID )
{
	void *pXlator = nullptr;

	if ( TUID == TUID_TEXT )
	{
		pXlator = new TextFileTranslator();
	}

	if ( TUID == TUID_MOD_MUSIC )
	{
		pXlator = new MODTranslator();
	}

	return pXlator;
}

BuiltIns NUTSBuiltIns;