#ifndef PLUGIN_DESCRIPTOR_
#define PLUGIN_DESCRIPTOR_

#include <string>
#include <list>

typedef unsigned long long QWORD;

typedef struct _FileDescriptor {
	std::wstring FriendlyName;
	DWORD PUID;
	DWORD Flags;
	DWORD NumIcons;
	DWORD IconIDs[64];
	VOID  *Icons[64];
	DWORD NumExts;
	WCHAR *Exts[64];
	DWORD IconTypes[64];
	DWORD IconMaps[64];
	DWORD SectorSize;
	QWORD MaxSize;
} FSDescriptor;

typedef struct _FontDescriptor {
	std::wstring FriendlyName;
	DWORD PUID;
	DWORD Flags;
	BYTE MinChar;
	BYTE MaxChar;
	BYTE ProcessableControlCodes[32];
	BYTE  *pFontData;
	DWORD MatchingEncodings[16];
} FontDescriptor;

typedef struct _TextTranslator {
	std::wstring FriendlyName;
	DWORD TUID;
	DWORD Flags;
} TextTranslator;

typedef struct _GraphicTranslator {
	std::wstring FriendlyName;
	DWORD TUID;
	DWORD Flags;
} GraphicTranslator;

typedef struct _RootHook
{
	std::wstring FriendlyName;
	DWORD        HookFSID;
	HBITMAP      HookIcon;
	BYTE         HookData[ 32 ];
} RootHook;

typedef struct _PluginDescriptor {
	std::wstring Provider;
	DWORD PUID;
	WORD  NumFS;
	WORD  NumFonts;
	WORD  NumTextXlators;
	WORD  NumGraphicXlators;
	WORD  NumRootHooks;
	WORD  NumGlobalCommands;

	FSDescriptor      *FSDescriptors;
	FontDescriptor    *FontDescriptors;
	TextTranslator    *TextXlators;
	GraphicTranslator *GraphicXlators;
	RootHook          *RootHooks;
	std::wstring      GlobalCommand[ 8 ];
} PluginDescriptor;

#endif
