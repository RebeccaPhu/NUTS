#pragma once

#include "../Nuts/Defs.h"

#include <map>

class RISCOSIcons
{
public:
	RISCOSIcons(void);
	~RISCOSIcons(void);

private:
	static std::map<DWORD, HBITMAP>     Bitmaps;
	static std::map<DWORD, DWORD>       TypeMap;
	static std::map<DWORD, FileType>    IntTypeMap;
	static std::map<DWORD, std::string> TypeNames;

public:
	static void AddIconMaps( DWORD IconID, DWORD TypeID, FileType IntTypeID, std::string Name, HBITMAP bitmap )
	{
		TypeMap[ TypeID ]    = IconID;
		Bitmaps[ IconID ]    = bitmap;
		IntTypeMap[ TypeID ] = IntTypeID;
		TypeNames[ TypeID ]  = Name;
	}

	static DWORD GetIconForType( DWORD TypeID )
	{
		if ( TypeMap.find( TypeID ) != TypeMap.end() )
		{
			return TypeMap[ TypeID ];
		}

		return (DWORD) FT_Arbitrary;
	}

	static FileType GetIntTypeForType( DWORD TypeID )
	{
		if ( IntTypeMap.find( TypeID ) != IntTypeMap.end() )
		{
			return IntTypeMap[ TypeID ];
		}

		return FT_Arbitrary;
	}
	
	static std::string GetNameForType( DWORD TypeID )
	{
		if ( TypeNames.find( TypeID ) != TypeNames.end() )
		{
			return TypeNames[ TypeID ];
		}

		return "File";
	}

	static HBITMAP GetBitmapForType( DWORD TypeID )
	{
		if ( Bitmaps.find( TypeID ) != Bitmaps.end() )
		{
			return Bitmaps[ TypeID ];
		}

		return nullptr;
	}
};

