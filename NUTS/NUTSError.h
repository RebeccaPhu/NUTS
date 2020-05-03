#pragma once

#include <string>

class NUTSError
{
public:
	NUTSError( DWORD ErrorCode, std::wstring ErrorString )
	{
		NUTSError::Code   = ErrorCode;
		NUTSError::String = ErrorString;

		/* These are used in the cast operator to import the details */
		IntCode   = ErrorCode;
		IntString = ErrorString;
	}

	~NUTSError(void);

	operator int()
	{
		NUTSError::Code   = IntCode;
		NUTSError::String = IntString;

		if ( NUTSError::Code == 0 )
		{
			return 0;
		}

		return -1;
	}

	static void Report( std::wstring Operation, HWND hWnd )
	{
		std::wstring err =
			Operation + std::wstring( L" failed:\n\n") +
			std::wstring( L"Error " ) + std::to_wstring( (unsigned long long) NUTSError::Code ) + std::wstring( L"\n\n" ) +
			NUTSError::String;

		MessageBoxW( hWnd, err.c_str(), L"NUTS", MB_ICONERROR | MB_OK );
	}

public:
	static DWORD Code;
	static std::wstring String;

private:
	std::wstring IntString;
	DWORD        IntCode;
};

