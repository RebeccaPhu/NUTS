#ifndef NUTSTYPES_H
#define NUTSTYPES_H

#include "stdafx.h"

#include <string>

typedef std::wstring FSIdentifier;
typedef std::wstring FTIdentifier;
typedef std::wstring FontIdentifer;
typedef std::wstring EncodingIdentifier;
typedef std::wstring TXIdentifier;
typedef std::wstring PluginIdentifier;
typedef std::wstring ProviderIdentifier;
typedef std::wstring FontIdentifier;
typedef std::wstring WrapperIdentifier;

typedef unsigned long long QWORD;

#define ENCODING_ASCII L"ASCII_Encoding"
#define FT_NONE        L"NOT_USED_OBJECT"
#define FT_WINDOWS     L"WindowsFiletype"
#define FT_ROOT        L"RootObject"
#define FT_UNSET       L"UNSET_FILE_OBJECT"
#define FT_NULL        L"NullObject"
#define FT_ZIP         L"ZIPFileContent"
#define FT_ISO         L"ISOFileContent"
#define FSID_NONE      L"Undefined_FileSystem"
#define FSID_ISO9660   L"ISO9660"
#define FSID_ISOHS     L"HighSierra"
#define FS_Null        L"NULL_FileSystem"
#define FS_Root        L"Root_FileSystem"
#define FS_Windows     L"Windows_FileBrowser"
#define TX_Null        L"NULL_Translator"
#define Provider_Null  L"NULL_Provider"
#define FONTID_PC437   L"Font_PC437"
#define WID_Null       L"NULL_Wrapper"

#define TUID_TEXT      L"Text_Translator"
#define TUID_MOD_MUSIC L"MOD_Music_Translator"
#define TUID_HEX       L"Hex_Dump_Translator"

#endif
