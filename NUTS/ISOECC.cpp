#include "stdafx.h"

#include "ISOECC.h"

/* Based on UNECMIFY at https://github.com/qeedquan/ecm/blob/master/unecm.c */

/* This file is by requirement licensed under GPL. This does not affect the licensing of any other part of NUTS. */

/* LUTs used for computing ECC/EDC */
static BYTE  ecc_f_lut[256];
static BYTE  ecc_b_lut[256];
static DWORD edc_lut[256];

/* Init routine */
void eccedc_init( void )
{
	DWORD i, j, edc;

	for ( i = 0; i < 256; i++ )
	{
		j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);

		ecc_f_lut[i]     = j;
		ecc_b_lut[i ^ j] = i;
		edc = i;
		
		for ( j = 0; j < 8; j++ )
		{
			edc = (edc >> 1) ^ (edc & 1 ? 0xD8018001 : 0);
		}

		edc_lut[i] = edc;
	}
}

/* Compute EDC for a block */
DWORD edc_partial_computeblock( DWORD edc, const BYTE *src, WORD size )
{
	while ( size-- )
	{
		edc = (edc >> 8) ^ edc_lut[ (edc ^ (*src++)) & 0xFF ];
	}

	return edc;
}

void edc_computeblock( const BYTE *src, WORD size, BYTE *dest )
{
  DWORD edc = edc_partial_computeblock( 0, src, size );

  dest[0] = (edc >>  0) & 0xFF;
  dest[1] = (edc >>  8) & 0xFF;
  dest[2] = (edc >> 16) & 0xFF;
  dest[3] = (edc >> 24) & 0xFF;
}

/* Compute ECC for a block (can do either P or Q) */
static void ecc_computeblock( BYTE *src, DWORD major_count, DWORD minor_count, DWORD major_mult, DWORD minor_inc, BYTE *dest )
{
	DWORD size = major_count * minor_count;

	DWORD major, minor;

	for ( major = 0; major < major_count; major++ )
	{
		DWORD index = (major >> 1) * major_mult + (major & 1);

		BYTE ecc_a = 0;
		BYTE ecc_b = 0;

		for ( minor = 0; minor < minor_count; minor++ )
		{
			BYTE temp = src[index];

			index += minor_inc;

			if ( index >= size )
			{
				index -= size;
			}

			ecc_a ^= temp;
			ecc_b ^= temp;
			ecc_a = ecc_f_lut[ecc_a];
		}

		ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];

		dest[major              ] = ecc_a;
		dest[major + major_count] = ecc_a ^ ecc_b;
	}
}

/* Generate ECC P and Q codes for a block */
static void ecc_generate( BYTE *sector, bool zeroaddress )
{
	BYTE address[4], i;

	/* Save the address and zero it out */
	if ( zeroaddress )
	{
		for ( i = 0; i < 4; i++ )
		{
			address[i]     = sector[12 + i];
			sector[12 + i] = 0;
		}
	}

	/* Compute ECC P code */
	ecc_computeblock( sector + 0xC, 86, 24,  2, 86, sector + 0x81C );

	/* Compute ECC Q code */
	ecc_computeblock( sector + 0xC, 52, 43, 86, 88, sector + 0x8C8 );

	/* Restore the address */
	if ( zeroaddress )
	{
		for ( i = 0; i < 4; i++ )
		{
			sector[12 + i] = address[i];
		}
	}
}

/* Generate ECC/EDC information for a sector (must be 2352 = 0x930 bytes) */
void eccedc_generate( BYTE *sector, ISOSectorType type )
{
	DWORD i;

	switch ( type )
	{
		case ISOSECTOR_MODE1: /* Mode 1 */
			/* Compute EDC */
			edc_computeblock( sector + 0x00, 0x810, sector + 0x810 );

			/* Write out zero bytes */
			for ( i = 0; i < 8; i++ )
			{
				sector[0x814 + i] = 0;
			}

			/* Generate ECC P/Q codes */
			ecc_generate( sector, 0 );

			break;

		// NOTE: NO ECC/EDC for Mode 2 - the space is used for data.

		case ISOSECTOR_XA_M2F1: /* Mode 2 form 1 */
			/* Compute EDC */
			edc_computeblock( sector + 0x10, 0x808, sector + 0x818 );

			/* Generate ECC P/Q codes */
			ecc_generate(sector, 1);

			break;

		case ISOSECTOR_XA_M2F2: /* Mode 2 form 2 */
			/* Compute EDC */
			edc_computeblock(sector + 0x10, 0x91C, sector + 0x92C );

			// NO ECC here.

			break;
	}
}