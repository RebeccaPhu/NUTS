#pragma once

#include "resource.h"

#include <map>

#include "Defs.h"

using namespace std;

class CBitmapCache
{
public:
	CBitmapCache(void);
	~CBitmapCache(void);

	void    LoadBitmaps();
	HBITMAP GetBitmap( const DWORD ID );
	void    AddBitmap( const DWORD ID, HBITMAP hBitmap );
	void    Unload( void );

	typedef struct _TypePair {
		FileType Type;
		int      IconResource;
	} TypePair;

private:
	std::map<DWORD, HBITMAP> bitmaps;
};

extern CBitmapCache BitmapCache;