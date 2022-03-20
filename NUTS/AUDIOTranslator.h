#pragma once

#include "stdafx.h"

#include "NUTSTypes.h"
#include "NUTSXLAT.h"
#include "BYTEString.h"
#include "TempFile.h"

#include <vector>

typedef struct _AudioCuePoint
{
	DWORD        Position;
	std::wstring Name;
	BYTEString   EncodingName;
	bool         UseEncoding;

	EncodingIdentifier EncodingID;
} AudioCuePoint;

typedef struct _AudioTranslateOptions
{
	std::wstring FormatName;
	DWORD        AudioSize;
	bool         WideBits;
	bool         Stereo;

	AudioCuePoint Title;

	std::vector<AudioCuePoint> CuePoints;

	HANDLE       hStopTranslating;
	HWND         hParentWnd;
} AudioTranslateOptions;

class AUDIOTranslator
{
public:
	AUDIOTranslator()
	{
	}

	~AUDIOTranslator(void)
	{
	}

	virtual int Translate( CTempFile &obj, CTempFile &Output, AudioTranslateOptions *tx )
	{
		return -1;
	}
};

