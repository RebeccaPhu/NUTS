#ifndef TAPEDEFS_H
#define TAPEDEFS_H

#include "stdafx.h"

#include <string>
#include <vector>

typedef struct _TapeCue {
	DWORD IndexPtr;
	std::wstring IndexName;
	std::wstring Extra;
	DWORD Flags;
} TapeCue;

typedef struct _TapeIndex {
	std::wstring Title;
	std::wstring Publisher;
	std::vector<TapeCue> Cues;
} TapeIndex;


#endif