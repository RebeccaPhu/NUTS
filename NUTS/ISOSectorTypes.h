#ifndef ISOSECTORTYPES_H
#define ISOSECTORTYPES_H

#include "stdafx.h"

typedef enum _ISOSectorType
{
	ISOSECTOR_AUTO    = 0,    // Auto detect sector type from raw data
	ISOSECTOR_MODE1   = 1,    // Raw form with Mode 1 storage
	ISOSECTOR_MODE2   = 2,    // Raw form with Mode 2 (Non-XA) storage
	ISOSECTOR_XA_M2F1 = 3,    // Raw form with XA Mode 2 Form 1 storage
	ISOSECTOR_XA_M2F2 = 4,    // Raw form with XA Mode 2 Form 2 storage
	ISOSECTOR_PURE1   = 0xFE, // Pure form (no raw sectors) with Mode 1 storage
	ISOSECTOR_UNKNOWN = 0xFF, // Unknown sector type
} ISOSectorType;

#endif
