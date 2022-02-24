#ifndef ISOECC_H
#define ISOECC_H

/* Based on UNECMIFY at https://github.com/qeedquan/ecm/blob/master/unecm.c */

/* This file is by requirement licensed under GPL. This does not affect the licensing of any other part of NUTS. */

#include "stdafx.h"

#include "ISOSectorTypes.h"

void eccedc_init( void );
void eccedc_generate( BYTE *sector, ISOSectorType type );

#endif
