// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#include "CBMDLL.h"
#include "OpenCBMPlugin.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	hInstance = hModule;

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		UnloadOpenCBM();
		break;
	}
	return TRUE;
}

