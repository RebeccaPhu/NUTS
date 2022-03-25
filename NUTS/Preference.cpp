#include "StdAfx.h"
#include "Preference.h"


Preference::Preference( std::wstring PrefName, bool Default )
{
	KeyName = PrefName;

	BDefValue = Default;
}

Preference::Preference( std::wstring PrefName, DWORD Default )
{
	KeyName = PrefName;

	DDefValue = Default;
}

Preference::Preference( std::wstring PrefName, std::wstring Default )
{
	KeyName = PrefName;

	SDefValue = Default;
}

Preference::Preference( std::wstring PrefName )
{
	KeyName = PrefName;

	SDefValue = L"";
	DDefValue = 0;
	BDefValue = false;
}

Preference::~Preference(void)
{
}

Preference::operator DWORD()
{
	DWORD v;
	DWORD s = sizeof( v );
	DWORD t= REG_DWORD;

	HKEY hRoot = GetRegistryKey();

	if ( RegQueryValueEx( hRoot, KeyName.c_str(), NULL, &t, (BYTE *) &v, &s ) == ERROR_FILE_NOT_FOUND )
	{
		return DDefValue;
	}

	return v;
}

Preference::operator std::wstring ()
{
	WCHAR v[256];
	DWORD s = 255;
	DWORD t= REG_SZ;

	HKEY hRoot = GetRegistryKey();

	if ( RegQueryValueEx( hRoot, KeyName.c_str(), NULL, &t, (BYTE *) &v, &s ) == ERROR_FILE_NOT_FOUND )
	{
		return SDefValue;
	}

	return std::wstring( v );
}

Preference::operator bool ()
{
	DWORD v;
	DWORD s = sizeof( v );
	DWORD t= REG_DWORD;

	HKEY hRoot = GetRegistryKey();

	if ( RegQueryValueEx( hRoot, KeyName.c_str(), NULL, &t, (BYTE *) &v, &s ) == ERROR_FILE_NOT_FOUND )
	{
		return BDefValue;
	}

	if ( v != 0 )
	{
		return true;
	}

	return false;
}

Preference& Preference::operator=(const std::wstring &v )
{
	HKEY hRoot = GetRegistryKey();

	RegSetValueEx( hRoot, KeyName.c_str(), NULL, REG_SZ, (BYTE *) v.c_str(), DWORD( size_t( v.length() * sizeof( WCHAR ) ) ) );

	return *this;
}

Preference& Preference::operator=(const bool &v )
{
	DWORD kv = (v)?0xFFFFFFFF:0x00000000;

	HKEY hRoot = GetRegistryKey();

	RegSetValueEx( hRoot, KeyName.c_str(), NULL, REG_DWORD, (BYTE *) &kv, sizeof( DWORD ) );

	return *this;
}

Preference& Preference::operator=(const DWORD &v )
{
	DWORD t  = REG_DWORD;

	HKEY hRoot = GetRegistryKey();

	RegSetValueEx( hRoot, KeyName.c_str(), NULL, REG_DWORD, (BYTE *) &v, sizeof( DWORD ) );

	return *this;
}

HKEY Preference::GetRegistryKey()
{
	HKEY Root;
	DWORD dis;
	
	RegCreateKeyEx( HKEY_CURRENT_USER, L"Software\\NUTS", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &Root, &dis );

	return Root;
}
