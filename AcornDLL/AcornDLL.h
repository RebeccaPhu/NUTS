// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the ACORNDLL_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// ACORNDLL_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef ACORNDLL_EXPORTS
#define ACORNDLL_API __declspec(dllexport)
#else
#define ACORNDLL_API __declspec(dllimport)
#endif

#include "../NUTS/PluginDescriptor.h"
#include "../NUTS/DataSource.h"
#include "../NUTS/DataSourceCollector.h"
#include "../NUTS/NUTSError.h"

extern "C" ACORNDLL_API BYTE *NUTSSignature;
extern "C" ACORNDLL_API int NUTSCommandHandler( PluginCommand *cmd );

extern HMODULE hInstance;

