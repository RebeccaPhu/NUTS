#ifndef FOP_H
#define FOP_H

#include "stdafx.h"

typedef enum _FOPDirection
{
	FOP_ReadEntry  = 1,
	FOP_WriteEntry = 2,
} FOPDirection;

#define FOP_DATATYPE_ZIPATTR 0x071D071D
#define FOP_DATATYPE_CDISO   0x0CD00CD0

typedef struct _FOPData
{
	void         *pFile;
	void         *pFS;
	FOPDirection Direction;
	BYTE         *pXAttr;
	WORD         lXAttr;
	DWORD        DataType;
} FOPData;

typedef bool ( * FOPTranslateFunction ) ( FOPData * );

#endif