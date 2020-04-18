#include "StdAfx.h"
#include "PiePercent.h"

#include <math.h>

PiePercent::PiePercent(void)
{
	hPieDC     = NULL;
	hPieBitmap = NULL;
	hPDarkBlue = NULL;
	hPDarkMag  = NULL;
	hPBlue     = NULL;
	hPMag      = NULL;
	hBG        = NULL;
	pPieBits   = nullptr;
}


PiePercent::~PiePercent(void)
{
	if ( ( hPieDC) && ( hOld ) ) { SelectObject( hPieDC, hOld ); }

	if ( hBG )        { DeleteObject( (HGDIOBJ) hBG ); }
	if ( hPMag )      { DeleteObject( (HGDIOBJ) hPMag ); }
	if ( hPBlue )     { DeleteObject( (HGDIOBJ) hPBlue ); }
	if ( hPDarkMag )  { DeleteObject( (HGDIOBJ) hPDarkMag ); }
	if ( hPDarkBlue ) { DeleteObject( (HGDIOBJ) hPDarkBlue ); }
	if ( hPieBitmap ) { DeleteObject( (HGDIOBJ) hPieBitmap ); }
	if ( hPieDC )     { DeleteDC( hPieDC ); }
}


void PiePercent::CreatePieChart( int Percent, HWND hPropsWnd )
{
	if ( hPieDC == NULL )
	{
		HDC hWndDC = GetDC( hPropsWnd );

		hPieDC = CreateCompatibleDC( hWndDC );

		hPieBitmap = CreateCompatibleBitmap( hWndDC, 120, 60 );

		hOld = SelectObject( hPieDC, hPieBitmap );

		hPDarkBlue = CreatePen( PS_SOLID, 0, RGB( 0, 0, 127 ) );
		hPDarkMag  = CreatePen( PS_SOLID, 0, RGB( 127,0,127 ) );

		hPBlue     = CreatePen( PS_SOLID, 0, RGB( 0, 0, 255 ) );
		hPMag      = CreatePen( PS_SOLID, 0, RGB( 255,0,255 ) );

		LOGBRUSH brsh;
		brsh.lbColor = GetSysColor(COLOR_WINDOW);
		brsh.lbStyle = BS_SOLID;

		hBG = CreateBrushIndirect( &brsh );

		ReleaseDC( hPropsWnd, hWndDC );
	}

	RECT r;

	r.left = 0;
	r.right = 120;
	r.top = 0;
	r.bottom = 60;

	SetBkMode( hPieDC, TRANSPARENT );

	FillRect( hPieDC, &r, hBG ); //  hBG );

	double p = (double) ( Percent % 25 );

	if ( ( Percent <= 25 ) || ( ( Percent > 50 ) && ( Percent <= 75 ) ) )
	{
		p = 25.0 - p;
	}

	double rads = ( ( 2.0 * 3.141592654 ) / 100.0 ) * p;

	HGDIOBJ hO = SelectObject( hPieDC, (HGDIOBJ) hPBlue );

	int blxc = 500;

	if ( ( Percent >= 25 ) && ( Percent < 75 ) )
	{
		blxc = (int) ( cos(rads) * 60 );

		if ( Percent >= 50 )
		{
			blxc = 0 - blxc;
		}
	}

	if ( Percent >= 75 )
	{
		blxc = -500;
	}
	
	/* Draw an ellipse, Pythagoras style */
	for ( int y=25; y>-25; y-- )
	{
		/* Determine min/max x for the given angle */
		double o2 = y * y;
		double h2 = 25 * 25;
		double a = sqrt( h2 - o2 );

		double yc = abs( y );

		if ( ( Percent <= 25) || ( Percent >= 75 ) )
		{
			yc = y;
		}

		double lxc;
		
		if ( ( Percent == 50 ) || ( Percent == 0 ) || ( Percent == 100 ) )
		{
			lxc = 60;
		}
		else if ( Percent < 50 )
		{
			lxc = 60 + (( yc / tan( rads ) ) * 2.4);
		}
		else
		{
			lxc = 60 - (( yc / tan( rads ) ) * 2.4);
		}

		int a_2 = (int) ( a * 2.4 );

		int minx = 60 - a_2;
		int maxx = 60 + a_2;

		if ( lxc > maxx ) { lxc = maxx; }
		if ( lxc < minx ) { lxc = minx; }

		if ( y >= 0 )
		{
			if ( ( Percent >= 25 ) && ( Percent <= 75 ) )
			{
				SelectObject( hPieDC, (HGDIOBJ) hPMag );

				MoveToEx( hPieDC, minx, 25 - y, NULL );
				LineTo( hPieDC, 60, 25 - y );

				SelectObject( hPieDC, (HGDIOBJ) hPBlue );

				MoveToEx( hPieDC, 60, 25 - y, NULL );
				LineTo( hPieDC, maxx, 25 - y );
			}
			else
			{
				if ( Percent >= 75 )
				{
					SelectObject( hPieDC, (HGDIOBJ) hPBlue );

					MoveToEx( hPieDC, minx, 25 - y, NULL );
					LineTo( hPieDC, (int) lxc, 25 - y );

					MoveToEx( hPieDC, 60, 25 - y, NULL );
					LineTo( hPieDC, maxx, 25 - y );

					SelectObject( hPieDC, (HGDIOBJ) hPMag );

					MoveToEx( hPieDC, (int) lxc, 25 - y, NULL );
					LineTo( hPieDC, 60, 25 - y );
				}
				else
				{
					SelectObject( hPieDC, (HGDIOBJ) hPMag );

					MoveToEx( hPieDC, minx, 25 - y, NULL );
					LineTo( hPieDC, 60, 25 - y );

					MoveToEx( hPieDC, (int) lxc, 25 - y, NULL );
					LineTo( hPieDC, maxx, 25 - y );

					SelectObject( hPieDC, (HGDIOBJ) hPBlue );

					MoveToEx( hPieDC, 60, 25 - y, NULL );
					LineTo( hPieDC, (int) lxc, 25 - y );
				}
			}
		}
		else
		{
			if ( ( Percent <= 25 ) || ( Percent >= 75 ) )
			{
				if ( Percent <= 25 )
				{
					SelectObject( hPieDC, (HGDIOBJ) hPMag );
				}
				else
				{
					SelectObject( hPieDC, (HGDIOBJ) hPBlue );
				}

				MoveToEx( hPieDC, minx, 25 - y, NULL );
				LineTo( hPieDC, maxx, 25 - y );
			}
			else
			{
				SelectObject( hPieDC, (HGDIOBJ) hPMag );

				MoveToEx( hPieDC, minx, 25 - y, NULL );
				LineTo( hPieDC, (int) lxc, 25 - y );

				SelectObject( hPieDC, (HGDIOBJ) hPBlue );

				MoveToEx( hPieDC, (int) lxc, 25 - y, NULL );
				LineTo( hPieDC, maxx, 25 - y );
			}
		}
	}

	/* Draw the 3D part */
	if ( Percent >= 75 )
	{
		blxc = -60;
	}
	else if ( Percent <= 25 )
	{
		blxc = 60;
	}
	else if ( Percent == 50 )
	{
		blxc = 0;
	}

	for ( int x=-60; x<60; x++ )
	{
		if ( ( x == -60 ) || ( x == 60 ) || ( x == blxc ) )
			SelectObject( hPieDC, GetStockObject( BLACK_PEN ) );
		else if ( x > blxc )
			SelectObject( hPieDC, (HGDIOBJ) hPDarkBlue );
		else
			SelectObject( hPieDC, (HGDIOBJ) hPDarkMag );

		double h2 = 60 * 60;
		double a2 = x * x;
		double o  = sqrt( h2 - a2 );

		int o_2 = (int) ( o / 2.4 );

		MoveToEx( hPieDC, 60 + x, 25 + o_2, NULL );
		LineTo( hPieDC, 60 + x, 35 + o_2 );
	}

	/* Draw the outline */
	SelectObject( hPieDC, GetStockObject( BLACK_PEN ) );

	MoveToEx( hPieDC, 0, 35, NULL );

	for ( int x=-60; x<60; x++ )
	{
		double h2 = 60 * 60;
		double a2 = x * x;
		double o  = sqrt( h2 - a2 );

		int o_2 = (int) ( o / 2.4 );

		LineTo( hPieDC, 60 + x, 35 + o_2 );
	}

	MoveToEx( hPieDC, 0, 25, NULL );

	for ( int x=-60; x<60; x++ )
	{
		double h2 = 60 * 60;
		double a2 = x * x;
		double o  = sqrt( h2 - a2 );

		int o_2 = (int) ( o / 2.4 );

		LineTo( hPieDC, 60 + x, 25 + o_2 );
	}

	MoveToEx( hPieDC, 0, 25, NULL );

	for ( int x=-60; x<60; x++ )
	{
		double h2 = 60 * 60;
		double a2 = x * x;
		double o  = sqrt( h2 - a2 );

		int o_2 = (int) ( o / 2.4 );

		LineTo( hPieDC, 60 + x, 25 - o_2 );
	}

	/* Draw the axes */
	MoveToEx( hPieDC, 60, 25, NULL );
	LineTo( hPieDC, 60, 0 );

	MoveToEx( hPieDC, 60, 25, NULL );

	double rrads = ( ( 2.0 * 3.141592654 ) / 100.0 ) * (double) Percent;

	LineTo( hPieDC, 60 + (int) ( sin( rrads ) * 60 ), 25 - (int) ( cos( rrads ) * 25 ) );

//	SelectObject( hPieDC, hO );
}
