#pragma once

#include <string>

class NUTSError
{
public:
	NUTSError( DWORD ErrorCode, std::wstring ErrorString )
	{
		NUTSError::Code   = ErrorCode;
		NUTSError::String = ErrorString;
	}

	~NUTSError(void);

	operator int()
	{
		if ( NUTSError::Code == 0 )
		{
			return 0;
		}

		return -1;
	}

public:
	static DWORD Code;
	static std::wstring String;
};

