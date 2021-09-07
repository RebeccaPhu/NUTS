#pragma once

#include "TempFile.h"
#include "Defs.h"

#include <vector>

typedef struct _AudioCuePoint
{
	DWORD        Position;
	std::wstring Name;
	BYTEString   EncodingName;
	bool         UseEncoding;
	DWORD        EncodingID;
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
	AUDIOTranslator();
	~AUDIOTranslator(void);

	virtual int Translate( CTempFile &obj, CTempFile &Output, AudioTranslateOptions *tx )
	{
		return -1;
	}
};

