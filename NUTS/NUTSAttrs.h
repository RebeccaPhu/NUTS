#ifndef NUTSATTRS_H
#define NUTSATTRS_H

#include "stdafx.h"
#include <string>

typedef enum _AttrType {
	AttrVisible  = 0x00000001, /* Attribute is visible on the Properties page */
	AttrEnabled  = 0x00000002, /* Attribute can be edited */
	AttrSelect   = 0x00000004, /* Attribute is one item from a list */
	AttrBool     = 0x00000008, /* Attribute is a checkbox */
	AttrCombo    = 0x00000010, /* Attribute is one item from a list OR a free form text. */
	AttrNumeric  = 0x00000020, /* Attribute is a number type as free form text */
	AttrHex      = 0x00000040, /* Attribute is in hexadecimal */
	AttrOct      = 0x00000080, /* Attribute is in octal */
	AttrDec      = 0x00000100, /* Attribute is in decimal (BOOR-RING) */
	AttrWarning  = 0x00000200, /* Attribute should trigger a warning on application that changing it will have non-innocuous (but not dangerous) effects */
	AttrDanger   = 0x00000400, /* Attribute should trigger a warning on application that changing it will probably be a really stupid idea */
	AttrFile     = 0x00000800, /* Attribute applies to files in the filesystem */
    AttrDir      = 0x00001000, /* Attribute applies to directories in the filesystem */
	AttrNegative = 0x00002000, /* Attribute supports negative numbers */
	AttrString   = 0x00004000, /* Attribute is a free-form text string */
	AttrTime     = 0x00008000, /* Attribute is a time stamp */
} AttrType;

typedef struct _AttrOption {
	std::wstring Name;
	DWORD        EquivalentValue;
	BYTE         *EquivalentStringValue;
	bool         Dangerous;
} AttrOption;

typedef struct _AttrDesc {
	BYTE  Index;
	DWORD Type;
	BYTE  MaxStringLength;
	BYTE  *pStringVal;
	DWORD StartingValue;

	std::wstring Name;

	std::vector<AttrOption> Options;
} AttrDesc;

typedef std::vector<AttrDesc> AttrDescriptors;
typedef std::vector<AttrDesc>::iterator AttrDesc_iter;
typedef std::vector<AttrOption> AttrOptions;
typedef std::vector<AttrOption>::iterator AttrOpt_iter;

#endif