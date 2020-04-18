#include "StdAfx.h"
#include "RISCOSIcons.h"

std::map<DWORD, HBITMAP>     RISCOSIcons::Bitmaps;
std::map<DWORD, DWORD>       RISCOSIcons::TypeMap;
std::map<DWORD, FileType>    RISCOSIcons::IntTypeMap;
std::map<DWORD, std::string> RISCOSIcons::TypeNames;

RISCOSIcons::RISCOSIcons(void)
{
	Bitmaps.clear();
	TypeMap.clear();
	IntTypeMap.clear();
	TypeNames.clear();
}


RISCOSIcons::~RISCOSIcons(void)
{
}
