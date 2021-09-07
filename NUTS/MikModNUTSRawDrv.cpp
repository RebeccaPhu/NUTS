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

#include "stdafx.h"

#include "MikModNUTSRawDrv.h"

MIKNUTSDRVInternals MODInternals = { NULL };

MIKNUTSDRVInternals *pMODInternals = &MODInternals;

#include <stdio.h>

#include <mikmod_internals.h>

#define BUFFERSIZE 32768

static	SBYTE *audiobuffer = NULL;

static void NUTSRAW_CommandLine(const CHAR *cmdline)
{
}

static BOOL NUTSRAW_IsThere(void)
{
	return 1;
}

static int NUTSRAW_Init(void)
{
	md_mode|=DMODE_SOFT_MUSIC|DMODE_SOFT_SNDFX;

	if ( !( audiobuffer = (SBYTE*) MikMod_malloc( BUFFERSIZE ) ) )
	{
		return 1;
	}

	if ( ( VC_Init() ) )
	{
		return 1;
	}

	if ( MODInternals.trgObj != NULL )
	{
		MODInternals.trgObj->Seek( 0 );
	}

	return 0;
}

static void NUTSRAW_Exit(void)
{
	VC_Exit();

	MikMod_free( audiobuffer );

	audiobuffer = NULL;
}

static void NUTSRAW_Update(void)
{
	if ( MODInternals.trgObj != NULL )
	{
		MODInternals.trgObj->Write( audiobuffer, VC_WriteBytes(audiobuffer,BUFFERSIZE) );
	}
}

static int NUTSRAW_Reset(void)
{
	if ( MODInternals.trgObj != NULL )
	{
		MODInternals.trgObj->Seek( 0 );
	}

	return 0;
}

MDRIVER drv_nuts_raw={
	NULL,
	"Disk writer (raw data)",
	"Raw disk writer (music.raw) v1.1",
	0,255,
	"raw",
	"file:t:music.raw:Output file name\n",
	NUTSRAW_CommandLine,
	NUTSRAW_IsThere,
	VC_SampleLoad,
	VC_SampleUnload,
	VC_SampleSpace,
	VC_SampleLength,
	NUTSRAW_Init,
	NUTSRAW_Exit,
	NUTSRAW_Reset,
	VC_SetNumVoices,
	VC_PlayStart,
	VC_PlayStop,
	NUTSRAW_Update,
	NULL,
	VC_VoiceSetVolume,
	VC_VoiceGetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceGetFrequency,
	VC_VoiceSetPanning,
	VC_VoiceGetPanning,
	VC_VoicePlay,
	VC_VoiceStop,
	VC_VoiceStopped,
	VC_VoiceGetPosition,
	VC_VoiceRealVolume
};
