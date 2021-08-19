#include "stdafx.h"
#include "ILBMTranslator.h"
#include "../NUTS/libfuncs.h"
#include "../NUTS/NUTSError.h"

#include <MMSystem.h>

ILBMTranslator::ILBMTranslator(void)
{
}


ILBMTranslator::~ILBMTranslator(void)
{
}

int ILBMTranslator::TranslateGraphics( GFXTranslateOptions &Options, CTempFile &FileObj )
{
	QWORD  lContent   = FileObj.Ext();
	DWORD *pixels     = nullptr;
	long  TotalMemory = (long) lContent;
	BYTE  *pContent   = (BYTE *) malloc( TotalMemory );

	ZeroMemory( pContent, TotalMemory );

	ReadIFFHeader( &FileObj );

	if ( !ImageHeader.Valid )
	{
		return NUTSError( 0x401, L"Not a valid ILBM file" );
	}

	FileObj.Seek( 0);
	FileObj.Read( pContent, lContent );

	Options.bmi->bmiHeader.biBitCount		= 32;
	Options.bmi->bmiHeader.biClrImportant	= 0;
	Options.bmi->bmiHeader.biClrUsed		= 0;
	Options.bmi->bmiHeader.biCompression	= BI_RGB;
	Options.bmi->bmiHeader.biHeight			= ImageHeader.Height;
	Options.bmi->bmiHeader.biPlanes			= 1;
	Options.bmi->bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	Options.bmi->bmiHeader.biSizeImage		= ImageHeader.Height * ImageHeader.Width * 4;
	Options.bmi->bmiHeader.biWidth			= ImageHeader.Width;
	Options.bmi->bmiHeader.biYPelsPerMeter	= 0;
	Options.bmi->bmiHeader.biXPelsPerMeter	= 0;

	pixels = (DWORD *) malloc( Options.bmi->bmiHeader.biSizeImage );

	*Options.pGraphics = (void *) pixels;

	// Preset all the unplanar values to 0
	ZeroMemory( pixels, Options.bmi->bmiHeader.biSizeImage ); 

	BYTE *srcPtr = &pContent[ ImageHeader.DataOffset ];

	// We'll need a bitplane row
	DWORD RowSize = ( ( ImageHeader.Width + 15 ) / 16 ) * 2;

	BYTE *PlanarRow = (BYTE *) malloc( RowSize );

	for ( DWORD y=0; y<ImageHeader.Height; y++)
	{
		for (BYTE plane=0; plane<ImageHeader.Planes; plane++)
		{
			if ( ImageHeader.Compression == 0 )
			{
				memcpy( PlanarRow, srcPtr, RowSize );

				srcPtr += RowSize;
			}
			else
			{
				// Modified-RLE compression
				BYTE  v;
				WORD  vv;
				DWORD ptr = 0;

				while ( ( ptr < RowSize ) && ( srcPtr < ( pContent + lContent ) ) )
				{
					v = *srcPtr; srcPtr++;

					if ( v > 128 )
					{
						vv = 257 - ( WORD ) v;

						v = *srcPtr; srcPtr++;

						for ( WORD cp=0; cp<vv; cp++ )
						{
							if ( ptr < RowSize )
							{
								PlanarRow[ ptr++ ] = v;
							}
						}
					}
					else if ( v < 128 )
					{
						vv = v; vv++;

						for ( WORD cp=0; cp<vv; cp++ )
						{
							v = *srcPtr; srcPtr++;

							if ( ptr < RowSize )
							{
								PlanarRow[ ptr++ ] = v;
							}
						}
					}
					else
					{
						break;
					}
				}
			}

			DWORD *pRow = &pixels[ ((ImageHeader.Height - 1) - y) * ImageHeader.Width ];

			UnpackPlanar( PlanarRow, pRow, plane, ImageHeader.Width );
		}
	}

	// The pixels array is full of colour indexes. Translate them to the RGB values
	for ( DWORD y = 0; y < ImageHeader.Height; y++ )
	{
		DWORD *pRow = &pixels[ y * ImageHeader.Width ];

		for ( DWORD x = 0; x < ImageHeader.Width; x++ )
		{
			DWORD pix = pRow[ x ];
			DWORD col = 0;

			if ( Palette.find( pix ) != Palette.end() )
			{
				col = Palette[ pix ];
			}

			pRow[ x ] = ( ( col & 0xFF0000 ) >> 16 ) | ( col & 0xFF00 ) | ( ( col & 0xFF ) << 16 );;
		}
	}

	return 0;
}

void ILBMTranslator::UnpackPlanar( BYTE *PlanarRow, DWORD *pRow, BYTE plane, DWORD Width )
{
	for ( DWORD pb=0; pb<Width; pb++ )
	{
		DWORD byteOffset = pb / 8;
		DWORD bitOffset  = pb % 8;

		BYTE bit = PlanarRow[ byteOffset ];
		
		bit >>= ( 7 - bitOffset );
		bit &= 1;

		pRow[ pb ] |= ( bit << plane );
	}
}

PhysColours ILBMTranslator::GetPhysicalColours( void )
{
	return PColours;
}

PhysPalette ILBMTranslator::GetPhysicalPalette( void )
{
	return Palette;
}

inline DWORD ILBMTranslator::GetPhysicalColour( BYTE TXCol, GFXTranslateOptions *opts )
{
	DWORD pix;

	pix = opts->pPhysicalPalette->at( TXCol );

	return ( ( pix & 0xFF0000 ) >> 16 ) | ( pix & 0xFF00 ) | ( ( pix & 0xFF ) << 16 );
}

AspectRatio ILBMTranslator::GetAspect( void )
{
	return std::make_pair<BYTE,BYTE>( ImageHeader.AspectX, ImageHeader.AspectY );
}

void ILBMTranslator::ReadIFFHeader( CTempFile *obj )
{
	ImageHeader.Valid = false;

	obj->Seek( 0 );

	DWORD ChunkID;
	BYTE  ChunkLenBytes[ 4 ];
	DWORD ChunkLen;

	obj->Read( &ChunkID, 4 );

	if ( ChunkID != MAKEFOURCC( 'F', 'O', 'R', 'M' ) )
	{
		return;
	}

	obj->Read( ChunkLenBytes, 4 ); // Discard

	obj->Read( &ChunkID, 4 );

	if ( ChunkID != MAKEFOURCC( 'I', 'L', 'B', 'M' ) )
	{
		if ( ChunkID != MAKEFOURCC( 'P', 'B', 'M', ' ' ) )
		{
			return;
		}
	}

	// The game is afoot. *summons torpedo*
	QWORD p = 12;
	QWORD e = obj->Ext();

	while ( p < e )
	{
		obj->Read( &ChunkID, 4 );
		obj->Read( &ChunkLenBytes, 4 );

		ChunkLen = BEDWORD( ChunkLenBytes );

		p += 8;

		if ( ( ChunkID == MAKEFOURCC( 'B', 'M', 'H', 'D' ) ) && ( ( e - p ) >= 20 ) )
		{
			BYTE bmhd[ 20 ];

			obj->Read( bmhd, 20 ); p += 20;

			ImageHeader.Width       = BEWORD( &bmhd[ 0 ] );
			ImageHeader.Height      = BEWORD( &bmhd[ 2 ] );
			ImageHeader.Planes      = bmhd[ 8 ];
			ImageHeader.Compression = bmhd[ 10 ];
			ImageHeader.AspectX     = bmhd[ 14 ];
			ImageHeader.AspectY     = bmhd[ 15 ];
			ImageHeader.Mask        = false;

			if ( ChunkLen > 20 )
			{
				/* Extra crap!? */
				obj->Seek( p + ( ChunkLen - 20 ) );

				p += ChunkLen - 20;
			}
		}
		else if ( ChunkID == MAKEFOURCC( 'C', 'M', 'A', 'P' ) )
		{
			DWORD NumColours = ChunkLen / 3;

			for ( DWORD c=0; c<NumColours; c++ )
			{
				BYTE rgb[ 4 ];

				obj->Read( rgb, 3 );

				PColours[ c ] = std::make_pair( c, c );
				Palette[ c ]  = (DWORD) RGB( rgb[0], rgb[1], rgb[2] );
			}

			if ( NumColours & 1 )
			{
				ChunkLen++;
			}

			p += ChunkLen;

			obj->Seek( p );
		}
		else if ( ChunkID == MAKEFOURCC( 'B', 'O', 'D', 'Y' ) )
		{
			ImageHeader.DataOffset = p;

			p += ChunkLen;

			obj->Seek( p );
		}
		else
		{
			/* Unknwown chunk. Skip */
			p += ChunkLen;

			obj->Seek( p );
		}
	}

	ImageHeader.Valid = true;
}