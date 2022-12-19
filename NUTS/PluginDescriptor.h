#ifndef PLUGIN_DESCRIPTOR_
#define PLUGIN_DESCRIPTOR_

#include "BYTEString.h"

#include <vector>
#include <string>
#include <list>

#include "NUTSTypes.h"


typedef enum _PluginCommandID {
	PC_SetPluginConnectors     = 0x00000000,
	PC_ReportProviders         = 0x00000001,
	PC_GetProviderDescriptor   = 0x00000002,
	PC_ReportFileSystems       = 0x00000003,
	PC_DescribeFileSystem      = 0x00000004,
	PC_CreateFileSystem        = 0x00000005,
	PC_CreateHookDataSource    = 0x00000006,
	PC_ReportImageExtensions   = 0x00000007,
	PC_GetImageExtension       = 0x00000008,
	PC_ReportEncodingCount     = 0x00000009,
	PC_ReportFonts             = 0x0000000A,
	PC_GetFontPointer          = 0x0000000B,
	PC_ReportIconCount         = 0x0000000C,
	PC_DescribeIcon            = 0x0000000D,
	PC_ReportFSFileTypeCount   = 0x0000000E,
	PC_ReportTranslators       = 0x0000000F,
	PC_DescribeTranslator      = 0x00000010,
	PC_CreateTranslator        = 0x00000011,
	PC_ReportRootHooks         = 0x00000012,
	PC_DescribeRootHook        = 0x00000013,
	PC_ReportRootCommands      = 0x00000014,
	PC_DescribeRootCommand     = 0x00000015,
	PC_PerformRootCommand      = 0x00000016,
	PC_TranslateFOPContent     = 0x00000017,
	PC_DescribeChar            = 0x00000018,
	PC_ReportPluginCreditStats = 0x00000019,
	PC_GetIconLicensing        = 0x0000001A,
	PC_GetPluginCredits        = 0x0000001B,
	PC_GetFOPDirectoryTypes    = 0x0000001C,
	PC_GetPortProviders        = 0x0000001D,
	PC_GetPortCounts           = 0x0000001E,
	PC_SetPortAssignments      = 0x0000001F,
	PC_GetWrapperCount         = 0x00000020,
	PC_GetWrapperDescriptor    = 0x00000021,
	PC_LoadWrapper             = 0x00000022,
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

typedef struct _NUTSPortRequirement
{
	ProviderIdentifier ProviderID;
	BYTE               PortCount;
} NUTSPortRequirement;

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

typedef struct _RootHookInvocation
{
	std::wstring FriendlyName;
	DWORD        Flags;
	BYTE         HookData[ 32 ];
	FSIdentifier HookFSID;
} RootHookInvocation;

typedef std::vector<RootHookInvocation> RootHookInvocations;

typedef struct _RootHook
{
	std::wstring FriendlyName;
	HBITMAP      HookIcon;

	PluginIdentifier Plugin;

	RootHookInvocations Invocations;
} RootHook;

typedef struct _FOPDirectoryType
{
	std::wstring FriendlyName;
	std::wstring Identifier;
} FOPDirectoryType;

typedef struct _WrapperDescriptor
{
	std::wstring FriendlyName;
	WrapperIdentifier Identifier;
} WrapperDescriptor;

#endif
