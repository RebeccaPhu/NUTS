#ifndef RESOLVED_H
#define RESOLVED_H

#include "stdafx.h"

#include <map>

#include "NUTSTypes.h"

#ifndef ASPECT_TYPE
#define ASPECT_TYPE
typedef std::pair<WORD, WORD> AspectRatio;
#endif

typedef struct _IconDef {
	BITMAPINFOHEADER bmi;
	void *pImage;
	AspectRatio Aspect;
} IconDef;

typedef std::map<DWORD, IconDef> ResolvedIconList;
typedef ResolvedIconList::iterator ResolvedIcon_iter;

#endif