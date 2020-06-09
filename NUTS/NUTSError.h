#pragma once

#include <string>

class NUTSError;

extern NUTSError *pGlobalError;

class NUTSError
{
public:
	NUTSError( DWORD ErrorCode, std::wstring ErrorString )
	{
		/* These are used in the cast operator to import the details */
		IntCode   = ErrorCode;
		IntString = ErrorString;
	}

	~NUTSError(void);

	operator int()
	{
		pGlobalError->GlobalCode   = IntCode;
		pGlobalError->GlobalString = IntString;

		if ( IntCode == 0 )
		{
			return 0;
		}

		return -1;
	}

	static void Report( std::wstring Operation, HWND hWnd )
	{
		std::wstring err =
			Operation + std::wstring( L" failed:\n\n") +
			std::wstring( L"Error " ) + std::to_wstring( (unsigned long long) pGlobalError->GlobalCode ) + std::wstring( L"\n\n" ) +
			pGlobalError->GlobalString;

		MessageBoxW( hWnd, err.c_str(), L"NUTS", MB_ICONERROR | MB_OK );
	}

public:
	DWORD GlobalCode;
	std::wstring GlobalString;

private:
	std::wstring IntString;
	DWORD        IntCode;
};

