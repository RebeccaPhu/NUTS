#include "StdAfx.h"
#include "BlockMap.h"
#include "NUTSFSTypes.h"
#include "NUTSConstants.h"

#define BlockWidth  12
#define BlockHeight 12

#define MapWidth  ( ( BlockWidth  * BlocksWide ) + 2 )
#define MapHeight ( ( BlockHeight * BlocksHigh ) + 2 )

BlockMap::BlockMap(void)
{
	hBlockDC     = NULL;
	hBlockBitmap = NULL;
	hO           = NULL;

	MapSizeW     = MapWidth;
	MapSizeH     = MapHeight;
}

BlockMap::~BlockMap(void)
{
	if ( hO != NULL )
	{
		SelectObject( hBlockDC, hO );

		DeleteObject( hBlockBitmap );
		DeleteDC( hBlockDC );
	}
}

void BlockMap::CreateBlockMap( HWND hWnd, BYTE *pMap )
{
	if ( pMap == nullptr )
	{
		return;
	}

	if ( hBlockDC == NULL )
	{
		HDC hDC  = GetDC( hWnd );
		hBlockDC = CreateCompatibleDC( hDC );

		hBlockBitmap = CreateCompatibleBitmap( hDC, MapWidth, MapHeight );

		hO = SelectObject( hBlockDC, hBlockBitmap );

		ReleaseDC( hWnd, hDC );

		LOGBRUSH brsh;
		brsh.lbStyle = BS_SOLID;
		brsh.lbColor = RGB( 192, 0, 192 );

		hFree = CreateBrushIndirect( &brsh );

		brsh.lbColor = RGB( 0, 0, 192 );

		hUsed = CreateBrushIndirect( &brsh );

		brsh.lbColor = RGB( 192, 0, 0 );

		hFixed = CreateBrushIndirect( &brsh );

		brsh.lbColor = GetSysColor( COLOR_WINDOW );

		hNone = CreateBrushIndirect( &brsh );
	}

	RECT EdgeRect;

	EdgeRect.left    = 0;
	EdgeRect.right   = EdgeRect.left + MapWidth;
	EdgeRect.top     = 0;
	EdgeRect.bottom  = EdgeRect.top + MapHeight;

	FillRect( hBlockDC, &EdgeRect, hNone );

	DrawEdge( hBlockDC, &EdgeRect, EDGE_SUNKEN, BF_RECT );

	BYTE xb = 0;
	BYTE yb = 0;

	for ( DWORD b=0; b<TotalBlocks; b++ )
	{
		BlockType BT = (BlockType) pMap[ b ];

		RECT BlockRect;

		BlockRect.left   = 1 + xb * BlockWidth;
		BlockRect.top    = 1 + yb * BlockHeight;
		BlockRect.right  = BlockRect.left + ( BlockWidth - 1 );
		BlockRect.bottom = BlockRect.top  + ( BlockHeight - 1 );

		if ( BT == BlockFree )
		{
			FillRect( hBlockDC, &BlockRect, hFree );
		}
		else if ( BT == BlockUsed )
		{
			FillRect( hBlockDC, &BlockRect, hUsed );
		}
		else if ( BT == BlockFixed )
		{
			FillRect( hBlockDC, &BlockRect, hFixed );
		}
		else
		{
			BlockRect.right++;
			BlockRect.bottom++;

			FillRect( hBlockDC, &BlockRect, hNone );
		}

		if ( BT != BlockAbsent )
		{
			FrameRect( hBlockDC, &BlockRect, (HBRUSH) GetStockObject( BLACK_BRUSH ) );
		}

		xb++;

		if ( xb >= BlocksWide ) { xb = 0; yb++; }
	}

//	SelectObject( hBlockDC, hO );
}