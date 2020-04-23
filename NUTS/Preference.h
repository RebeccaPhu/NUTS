#pragma once

#include <string>

class Preference
{
public:
	Preference( std::wstring PrefName, bool Default );
	Preference( std::wstring PrefName, DWORD Default );
	Preference( std::wstring PrefName, std::wstring Default );
	Preference( std::wstring PrefName );
	~Preference(void);

	operator DWORD ();
	operator std::wstring ();
	operator bool ();

	Preference& operator=(const std::wstring &v );
	Preference& operator=(const bool &v );
	Preference& operator=(const DWORD &v );

private:
	HKEY GetRegistryKey();

	std::wstring KeyName;

	std::wstring SDefValue;
	bool         BDefValue;
	DWORD        DDefValue;
};

