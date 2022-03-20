// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the TZXCONFIG_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// TZXCONFIG_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef TZXCONFIG_EXPORTS
#define TZXCONFIG_API __declspec(dllexport)
#else
#define TZXCONFIG_API __declspec(dllimport)
#endif

#include <vector>

#include "../NUTS/NativeFile.h"
#include "../NUTS/TempFile.h"

extern HINSTANCE hInstance;

TZXCONFIG_API BYTE TZXSelectNewBlock( HWND hParent );
TZXCONFIG_API int TZXConfigureBlock( HWND hParent, WORD BlockNum, BYTE BlockID, CTempFile *pStore, bool NewBlock, bool AllowReplaceData, std::vector<NativeFile> &Files );
TZXCONFIG_API int TZXSelectBlock( HWND hParent, std::vector<NativeFile> &Files );
TZXCONFIG_API int TZXExportOptions( HWND hParent, std::wstring RegPrefix );

TZXCONFIG_API int StringBox( HWND hWnd, std::wstring Prompt, std::wstring &Result );

TZXCONFIG_API int TZXSelectPath( HWND hWnd, std::vector<std::wstring> &Paths );