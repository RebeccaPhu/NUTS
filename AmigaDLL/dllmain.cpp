// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#include "AmigaDLL.h"

BYTE *sig = (BYTE *) "NUTS Plugin Signature";

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	hInstance = hModule;

	NUTSSignature = sig;

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

