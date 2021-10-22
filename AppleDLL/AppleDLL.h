// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the APPLEDLL_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// APPLEDLL_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef APPLEDLL_EXPORTS
#define APPLEDLL_API __declspec(dllexport)
#else
#define APPLEDLL_API __declspec(dllimport)
#endif

#include "../NUTS/DataSource.h"
#include "../NUTS/PluginDescriptor.h"
#include "../NUTS/DataSourceCollector.h"
#include "../NUTS/NUTSError.h"

extern "C" APPLEDLL_API BYTE *NUTSSignature;
extern "C" APPLEDLL_API int NUTSCommandHandler( PluginCommand *cmd );

extern HMODULE hInstance;
extern UINT    APPL_ICON;
extern UINT    DOC_ICON;
extern UINT    BLANK_ICON;
