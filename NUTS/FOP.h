#ifndef FOP_H
#define FOP_H

#include "stdafx.h"
#include <string>

typedef enum _FOPDirection
{
	FOP_None        = 0,
	FOP_ReadEntry   = 1,
	FOP_WriteEntry  = 2,
	FOP_PostRead    = 3,
	FOP_PreWrite    = 4,
	FOP_ExtraAttrs  = 5,
	FOP_AttrChanges = 6,
	FOP_SetDirType  = 7,
} FOPDirection;

#define FOP_DATATYPE_ZIPATTR 0x071D071D
#define FOP_DATATYPE_CDISO   0x0CD00CD0

typedef struct _FOPReturn
{
	std::wstring Identifier;
	BYTE		 StatusString[ 128 ];
	BYTE         Descriptor[ 128 ];
	FSIdentifier ProposedFS;
} FOPReturn;

typedef struct _FOPData
{
	_FOPData()
	{
		pFile  = nullptr;
		pFS    = nullptr;
		pXAttr = nullptr;
		lXAttr = 0;

		Direction = FOP_None,
		DataType  = 0;
		
		ReturnData.Identifier        = L"";
		ReturnData.StatusString[ 0 ] = 0;
		ReturnData.Descriptor[ 0 ]   = 0;
		ReturnData.ProposedFS        = FSID_NONE;
	}

	/* out to plugin */
	void         *pFile;
	void         *pFS;
	FOPDirection Direction;
	BYTE         *pXAttr;
	WORD         lXAttr;
	DWORD        DataType;

	/* in from plugin */
	FOPReturn    ReturnData;
} FOPData;

typedef bool ( * FOPTranslateFunction ) ( FOPData * );
typedef void * ( * FOPLoadFSFunction ) ( FSIdentifier, void * );

#endif