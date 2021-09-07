/*	
	GNU License for MikMod ONLY
	===========================

	MikMod sound library
	(c) 1998, 1999, 2000 Miodrag Vallat and others - see file AUTHORS for
	complete list.

	This library is free software; you can redistribute it and/or modify
	it under the terms of the GNU Library General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
	02111-1307, USA.

	##############################

	This Driver is a derived work from the libmikmod and as such is
	licensed under its GNU license.

	This file is also part of NUTS. NUTS (excluding this file) is
	subject to the author's own license ALONE and is NOT licensed
	under any GNU license.

	This file may be used in accordance with the terms of the GNU license,
	however it is unlikely to be of value to do so, especialy since it
	uses components from the remainder of NUTS which is not subject to the
	GNU license.
*/

/*  LibMikMod driver adapted from drv_raw */

#ifndef MIKMOD_NUTS_RAW_DRV

#include "TempFile.h"

typedef struct _MIKNUTSDRVInternals
{
	CTempFile *trgObj;
} MIKNUTSDRVInternals;

extern MIKNUTSDRVInternals *pMODInternals;

#include <mikmod_internals.h>

extern MDRIVER drv_nuts_raw;

#endif MIKMOD_NUTS_RAW_DRV