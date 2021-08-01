#include "stdafx.h"

#include "TZXHardware.h"

const WCHAR *TZXBlockTypes[] = {
	L"ID 10 - Standard speed data (ZX Spectrum)",
	L"ID 11 - Turbo speed data",
	L"ID 12 - Pure tone",
	L"ID 13 - Sequence of pulses of various lengths",
	L"ID 14 - Pure data",
	L"ID 15 - Direct recording",
	L"ID 18 - CSW recording",
	L"ID 19 - Generalized data",
	L"ID 20 - Pause / 'Stop the tape'",
	L"ID 21 - Group start",
	L"ID 22 - Group end",
	L"ID 23 - Jump",
	L"ID 24 - Loop start",
	L"ID 25 - Loop end",
	L"ID 26 - Call sequence",
	L"ID 27 - Return from sequence",
	L"ID 28 - Select",
	L"ID 2A - Stop the tape if in 48K mode",
	L"ID 2B - Set signal level",
	L"ID 30 - Text description",
	L"ID 31 - Message",
	L"ID 32 - Archive info",
	L"ID 33 - Hardware type",
	L"ID 35 - Custom info",
	L"ID 5A - Glue",
	nullptr
};

const BYTE TZXMaxID[] = { 0x2D, 0x17, 0x0A, 0x0C, 0x04, 0x05, 0x01, 0x03, 0x07, 0x0A, 0x02, 0x01, 0x03, 0x00, 0x00, 0x01, 0x00, 0x03 };

const WCHAR *TZXComputers[] = {
	L"ZX Spectrum 16k", L"ZX Spectrum 48K/48K+", L"ZX Spectrum 48K Issue 1", L"ZXSpectrum 128K+", L"ZX SPectrum 128K+2 Grey", L"ZX Spectrum 128+2A/+3",
	L"Timex Sinclair TC-2048", L"Timex Sinclair TS-2068", L"Pentagon 128", L"Same Coupé", L"Didaktik M", L"Didaktik Gama", L"ZX80", L"ZX81",
	L"ZX Spectrum 128K Spanish", L"ZX Spectrum 128K Arabic", L"Microdigital TK 90-X", L"Microdigital TK 95", L"Byte", L"Elwro 800-3", L"ZS Scorpion 256",
	L"Amstrad CPC 464", L"Amstrad CPC 664", L"Amstrad CPC 6128", L"Amstrad CPC 464+", L"Amstrad CPC 6128+", L"Jupter ACE", L"Enterprise", L"Commodore 64",
	L"Commodore 128", L"Inves Spectrum+", L"Profi", L"GrandRomMax", L"Kay 1024", L"Ice Felix HC 91", L"Ice Felix HC 2000", L"Amaterske RADIO Mistrum",
	L"Quorum 128", L"MicroART ATM", L"MicroART ATM Turbo 2", L"Chrome", L"ZX Badaloc", L"TS-1500", L"Lambda", L"TK-65", L"ZX-97"
};

const WCHAR *TZXStorage[] = {
	L"ZX Microdrive", L"Opus Discovery", L"MGT Disciple", L"MGT Plus-D", L"Rotronics Wafadrive", L"TR-DOS (BetaDisk)", L"Byte Drive", L"Watsford",
	L"FIZ", L"Radofin", L"Didaktik disk drives", L"BS-DOS (MB-02)", L"ZX Spectrum +3 disk drive", L"JLO (Oliger disk interface)", L"Timex FDD3000",
	L"Zebra disk drive", L"Ramex Millenia", L"Larken", L"Kempston disk interface", L"Sandy", L"ZX Spectrum +3e hard disk", L"ZXATASP", L"DivIDE", L"ZXCF"
};

const WCHAR *TZXMemory[] = {
	L"Sam Ram", L"Multiface ONE", L"Multiface 128k", L"Multiface +3", L"MultiPrint", L"MB-02 ROM/RAM Expansion", L"SoftROM", L"1k", L"16k", L"48k", L"Memory in 8-16k used"
};

const WCHAR *TZXSound[] = {
	L"Classic AY hardware (inc. 128K ZX models)", L"Fuller Box AY sound hardware", L"Currah µSpeech", L"SpecDrum", L"AY ACB Stereo", L"AY ABC Stereo", L"RAM Music Machine",
	L"Covox", L"General Sound", L"Intec Electronics Digital Interface B8001", L"Zon-X AY", L"QuickSilva AY", L"Jupiter ACE"
};

const WCHAR *TZXJoysticks[] = { L"Kempston", L"Cursor/Protek/AGF", L"Sinclair IF2 Left Port", L"Sinclair IF2 Right Port", L"Fuller" };
const WCHAR *TZXMice[] = { L"AMX Mouse", L"Kempston Mouse" };
const WCHAR *TZXCtrls[] = { L"Trickstick", L"ZX Light Gun", L"Zebra Graphics Tablet", L"Defender Light Gun" };
const WCHAR *TZXSerial[] = { L"ZX Interface 1", L"ZX Spectrum 128k" };
const WCHAR *TZXParallel[] = {
	L"Kempston S", L"Kempston E", L"ZX Spectrum +3", L"Tasman", L"DK'Tronics", L"Hilderbay", L"INES Printerface", L"ZX LPrint Interface 3",
	L"MultiPRint", L"Opus Discovery", L"Standard 8255 chip with ports 31/63/95"
};

const WCHAR *TZXPrinters[] = { L"ZX Printer/Alphacom 32/Comaptible", L"Generic Printer", L"EPSON Compatible" };
const WCHAR *TZXModems[] = { L"Prism VTX5000", L"T/S 2050 or Westridge 2050" };
const WCHAR *TZXDigitizers[] = { L"RD Digital Tracer", L"DK'Tronics Light Pen", L"British MicroGraph Pad", L"Romantic Robot Videface" };
const WCHAR *TZXNetwork[] = { L"ZX Interface 1" };
const WCHAR *TZXKeyboards[] = { L"Keypad for ZX Spectrum 128k" };
const WCHAR *TZXADDA[] = { L"Harley Systems ADC 8.2", L"Blackboard Electronics" };
const WCHAR *TZXGraphics[] = { L"WRX Hi-Res", L"G007", L"Memotech", L"Lambda Colour" };

const WCHAR **TZXHardwareTypeList[] = {
	TZXComputers, TZXStorage, TZXMemory, TZXSound, TZXJoysticks, TZXMice,
	TZXCtrls, TZXSerial, TZXParallel, TZXPrinters, TZXModems, TZXDigitizers,
	TZXNetwork, TZXKeyboards, TZXADDA, TZXGraphics
};

const WCHAR *TZXHInfos[ 4 ] = {
	L"Runs, but may or may not use special hardware",
	L"Uses special hardware",
	L"Runs, but does not use special hardware",
	L"Does not run on this machine"
};

const WCHAR *TZXArchiveFieldTypes[] = {
	L"Title", L"Publisher", L"Author(s)", L"Year", L"Language", L"Type", L"Price", L"Protection scheme", L"Origin"
};

const BYTE TZXIDs[] = {
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x18, 0x19,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2A, 0x2B,
	0x30, 0x31, 0x32, 0x33, 0x35,
	0x5A,
	0x00
};

