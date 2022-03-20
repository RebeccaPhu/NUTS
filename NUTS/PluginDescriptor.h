#ifndef PLUGIN_DESCRIPTOR_
#define PLUGIN_DESCRIPTOR_

#include "BYTEString.h"

#include <string>
#include <list>

#include "NUTSTypes.h"


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
	PC_ReportFonts,
	PC_GetFontPointer,
	PC_ReportIconCount,
	PC_DescribeIcon,
	PC_ReportFSFileTypeCount,
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
	PluginIdentifier   PluginID;
	ProviderIdentifier ProviderID;
} NUTSProvider;

typedef struct _FileDescriptor {
	std::wstring FriendlyName;
	FSIdentifier FSID;
	DWORD Flags;
	DWORD SectorSize;
	QWORD MaxSize;
	BYTEString FavouredExtension;
} FSDescriptor;

typedef struct _DataTranslator {
	ProviderIdentifier ProviderID;
	std::wstring FriendlyName;
	TXIdentifier TUID;
	DWORD Flags;
} DataTranslator;

typedef struct _RootHook
{
	std::wstring FriendlyName;
	FSIdentifier HookFSID;
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
