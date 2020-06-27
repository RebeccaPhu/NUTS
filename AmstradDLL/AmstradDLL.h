// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the AMIGADLL_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// AMIGADLL_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef AMSTRADDLL_EXPORTS
#define AMSTRADDLL_API __declspec(dllexport)
#else
#define AMSTRADDLL_API __declspec(dllimport)
#endif

#include "../NUTS/DataSource.h"
#include "../NUTS/PluginDescriptor.h"
#include "../NUTS/DataSourceCollector.h"
#include "../NUTS/NUTSError.h"

extern "C" AMSTRADDLL_API PluginDescriptor *GetPluginDescriptor(void);
extern "C" AMSTRADDLL_API void *CreateFS( DWORD PUID, DataSource *pSource );

extern "C" AMSTRADDLL_API DataSourceCollector *pExternCollector;
extern "C" AMSTRADDLL_API NUTSError *pExternError;

extern HMODULE hInstance;
