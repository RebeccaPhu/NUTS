#ifndef PLUGIN_DESCRIPTOR_
#define PLUGIN_DESCRIPTOR_

#include "BYTEString.h"

#include <string>
#include <list>

#define MAKEFSID( plid, pid, fid ) ( (DWORD) ( plid << 16 ) | (DWORD) ( pid << 8 ) | (DWORD) fid )
#define MAKEPROVID( plid, pid ) ( (DWORD) ( plid << 16 ) | (DWORD) ( pid << 8 ) )
#define PLUGINID( cid ) ( (WORD) ( ( cid >> 16 ) & 0xFFFF ) )
#define PROVIDERID( cid ) ( (BYTE) ( ( cid >> 8 ) & 0xFF ) )
#define FSIDID( cid ) ( (BYTE) ( cid & 0xFF ) )
#define PROVID( cid ) ( (DWORD) ( (DWORD) cid & 0xFFFFFF00 ) )
#define MYFSID ( FSID & 0xFFFF )

typedef unsigned long long QWORD;

typedef enum _PluginCommandID {
	PC_SetPluginConnectors,
	PC_ReportProviders,
	PC_GetProviderDescriptor,
	PC_ReportFileSystems,
	PC_DescribeFileSystem,
	PC_GetOffsetLists,
	PC_CreateFileSystem,
	PC_ReportImageExtensions,
	PC_GetImageExtension,
	PC_ReportEncodingCount,
	PC_SetEncodingBase,
	PC_ReportFonts,
	PC_GetFontPointer,
	PC_ReportIconCount,
	PC_DescribeIcon,
	PC_ReportFSFileTypeCount,
	PC_SetFSFileTypeBase,
	PC_ReportTranslators,
	PC_DescribeTranslator,
	PC_CreateTranslator,
	PC_ReportRootHooks,
	PC_DescribeRootHook,
	PC_ReportRootCommands,
	PC_DescribeRootCommand,
	PC_PerformRootCommand,
	PC_TranslateFOPContent,
	PC_DescribeChar,
	PC_ReportPluginCreditStats,
	PC_GetIconLicensing,
	PC_GetPluginCredits,
	PC_GetFOPDirectoryTypes,
} PluginCommandID;

typedef union _PluginCommandParameter {
	void  *pPtr;
	DWORD Value;
} PluginCommandParameter;

typedef struct _PluginCommand {
	PluginCommandID CommandID;
	PluginCommandParameter InParams[ 8 ];
	PluginCommandParameter OutParams[ 8 ];
} PluginCommand;

#define NUTS_PLUGIN_SUCCESS      0
#define NUTS_PLUGIN_UNRECOGNISED 1
#define NUTS_PLUGIN_ERROR        0xFFFF

typedef struct _NUTSProvider {
	std::wstring FriendlyName;
	DWORD        PluginID;
	DWORD        ProviderID;
} NUTSProvider;

typedef struct _FileDescriptor {
	std::wstring FriendlyName;
	DWORD PUID;
	DWORD Flags;
	DWORD SectorSize;
	QWORD MaxSize;
	BYTEString FavouredExtension;
} FSDescriptor;

typedef struct _DataTranslator {
	DWORD ProviderID;
	std::wstring FriendlyName;
	DWORD TUID;
	DWORD Flags;
} DataTranslator;

typedef struct _RootHook
{
	std::wstring FriendlyName;
	DWORD        HookFSID;
	HBITMAP      HookIcon;
	BYTE         HookData[ 32 ];
	DWORD        Flags;
} RootHook;

typedef struct _FOPDirectoryType
{
	std::wstring FriendlyName;
	std::wstring Identifier;
} FOPDirectoryType;

#endif
