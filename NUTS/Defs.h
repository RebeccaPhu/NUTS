#pragma once

#include <vector>
#include <map>

#define ENCODING_ASCII 0x000A5C11
#define FT_WINDOWS     0x80000000
#define FT_ROOT        0x00000000

typedef unsigned long long QWORD;

/* Generic filetypes, not necessary indicative within any given FS */
typedef enum _FileType {
	FT_None      =  0, // Used to mean "no file" where appropriate
	FT_Arbitrary =  1, // "could be anything to be honest"
	FT_Binary    =  2, // "Binary blob. Could be anything, but definitely not text"
	FT_Code      =  3, // Executable code
	FT_Script    =  4, // Executable text script (e.g. RiscOS Obey file)
	FT_Spool     =  5, // Text that can be spooled as if typed ( e.g. Acorn *SPOOL file)
	FT_Text      =  6, // Plain text. 'Nuff said.
	FT_BASIC     =  7, // Some kind of BASIC program. Doesn't imply whose.
	FT_Data      =  8, // Arbitrary data, as distinct from Code, BASIC, SCript or SPool.
	FT_Graphic   =  9, // Graphical data in some way.
	FT_Sound     = 10, // Audio data in some way.
	FT_Pref      = 11, // Some kind of settings file
	FT_App       = 12, // Application (e.g. RiscOS application)
	FT_DiskImage = 13, // Image of a disk
	FT_TapeImage = 14, // Image of government red tape
	FT_MiscImage = 15, // Image of something else
	FT_CDROM     = 16, // CDROM Drive
	FT_HardDisc  = 17, // Hard Drive
	FT_Floppy    = 18, // Floppy Drive
	FT_Directory = 19, // Directory (J, R, Hartley)
} FileType;

typedef struct _NativeFile {
	DWORD fileID;          // File index into the pDirectory->Files vector
	BYTE  Filename[256];   // Filename, in EncodingID encoding
	BYTE  Extension[4];    // Extension (if Flags & FF_Extension)
	DWORD Flags;           // File flags
	QWORD Length;          // Length in bytes
	DWORD Attributes[16];  // Arbitrary set of FS-specific attributes
	FileType Type;         // Type of file in terms of content (see below)
	DWORD Icon;            // Icon number to display (can be FT)
	DWORD EncodingID;      // Text encoding of filename/extension
	DWORD FSFileType;      // Type of file in terms of filesystem, e.g. "This is a C64 file".
	DWORD XlatorID;        // ID of a translator plugin that can deal with this file
	bool  HasResolvedIcon; // Has a icon resolved from the file system itself (Dynamic icon)
} NativeFile;

typedef std::vector<NativeFile>::iterator NativeFileIterator;

typedef enum _ActionID {
	AA_FORMAT      = 1,
	AA_COPY        = 2,
	AA_DELETE      = 3,
	AA_PROPS       = 4,
	AA_SET_PROPS   = 5,
	AA_NEWIMAGE    = 6,
	AA_SET_FSPROPS = 7,
	AA_INSTALL     = 8,
} ActionID;

typedef struct _AppAction {
	ActionID Action;
	void     *Pane;
	void     *FS;
	void     *pData;
	HWND     hWnd;

	std::vector<NativeFile> Selection;
} AppAction;

typedef enum _FileFlags {
	FF_Directory = 0x1,
	FF_Extension = 0x2,
	FF_Pseudo    = 0x40000000,
	FF_Special   = 0x80000000,
} FileFlags;

typedef enum _BuiltInFSIDs {
	FS_Null    = 0x00000000,
	FS_Root    = 0x80001001,
	FS_Windows = 0x80001002,
} BuiltInFSIDs;

typedef struct _FSHint {
	DWORD FSID;
	WORD  Confidence;
} FSHint;

typedef std::vector<FSHint> FSHints;

typedef struct _TitleComponent {
	BYTE String[512];
	DWORD Encoding;
} TitleComponent;

typedef enum _FSFlags {
	FSF_Formats_Raw      = 0x00000001,
	FSF_Formats_Image    = 0x00000002,
	FSF_Creates_Image    = 0x00000004,
	FSF_Supports_Spaces  = 0x00000008,
	FSF_Supports_Dirs    = 0x00000010,
	FSF_ArbitrarySize    = 0x00000020,
	FSF_UseSectors       = 0x00000040,
	FSF_DynamicSize      = 0x00000080,
	FSF_FixedSize        = 0x00000100,
	FSF_ReadOnly         = 0x00000200,
	FSF_SupportFreeSpace = 0x00000400,
	FSF_SupportBlocks    = 0x00000800,
	FSF_Size             = 0x00001000,
	FSF_Capacity         = 0x00002000,
	FSF_Reorderable      = 0x00004000,
} FSFlags;

typedef struct _ProviderDesc {
	std::wstring Provider;
	DWORD        PUID;
} ProviderDesc;

typedef std::vector<ProviderDesc> ProviderList;
typedef std::vector<ProviderDesc>::iterator ProviderDesc_iter;

typedef struct _FormatDesc {
	std::wstring Format;
	DWORD        FUID;
	DWORD        Flags;
	QWORD        MaxSize;
	DWORD        SectorSize;
} FormatDesc;

typedef std::vector<FormatDesc> FormatList;
typedef std::vector<FormatDesc>::iterator FormatDesc_iter;

typedef enum _FormatTypes {
	FormatType_Quick = 1,
	FormatType_Full  = 2,
} FormatType;

typedef struct _FSSpace {
	DWORD UsedBytes;
	DWORD Capacity;
	BYTE *pBlockMap;
} FSSpace;

typedef enum _BlockType {
	BlockFree   = 0,
	BlockUsed   = 1,
	BlockFixed  = 2,
	BlockAbsent = 0xFF,
} BlockType;

typedef enum _AttrType {
	AttrVisible  = 0x00000001,
	AttrEnabled  = 0x00000002,
	AttrSelect   = 0x00000004,
	AttrBool     = 0x00000008,
	AttrCombo    = 0x00000010,
	AttrNumeric  = 0x00000020,
	AttrHex      = 0x00000040,
	AttrOct      = 0x00000080,
	AttrDec      = 0x00000100,
	AttrWarning  = 0x00000200,
	AttrDanger   = 0x00000400,
	AttrFile     = 0x00000800,
    AttrDir      = 0x00001000,
	AttrNegative = 0x00002000,
	AttrString   = 0x00004000,
	AttrTime     = 0x00008000,
} AttrType;

typedef struct _AttrOption {
	std::wstring Name;
	DWORD        EquivalentValue;
	BYTE         *EquivalentStringValue;
	bool         Dangerous;
} AttrOption;

typedef struct _AttrDesc {
	BYTE  Index;
	DWORD Type;
	BYTE  MaxStringLength;
	BYTE  *pStringVal;
	DWORD StartingValue;

	std::wstring Name;

	std::vector<AttrOption> Options;
} AttrDesc;

typedef struct _ExtDef {
	FileType Type;
	FileType Icon;
} ExtDef;

typedef enum _GFXFlags {
	GFXLogicalPalette = 0x00000001,
	GFXMultipleModes  = 0x00000002,
} GFXFlags;

typedef std::vector<AttrDesc> AttrDescriptors;
typedef std::vector<AttrDesc>::iterator AttrDesc_iter;
typedef std::vector<AttrOption> AttrOptions;
typedef std::vector<AttrOption>::iterator AttrOpt_iter;

typedef struct _ScreenMode {
	std::wstring FriendlyName;
	DWORD        ModeID;
} ScreenMode;

typedef std::vector<ScreenMode> ModeList;
typedef ModeList::iterator ModeList_iter;

typedef std::map<WORD,WORD> LogPalette;
typedef std::map<WORD,std::pair<WORD,WORD>> PhysColours;
typedef std::map<WORD,DWORD> PhysPalette;

typedef std::pair<WORD, WORD> AspectRatio;

typedef enum _DisplayPref {
	DisplayScaledScreen = 1,
	DisplayNatural      = 2,
} DisplayPref;

typedef struct _GFXTranslateOptions {
	BITMAPINFO *bmi;
	void **pGraphics;
	LogPalette  *pLogicalPalette;
	PhysPalette *pPhysicalPalette;
	PhysColours *pPhysicalColours;
	DWORD ModeID;
	long long Offset;
	QWORD Length;
	bool  FlashPhase;
} GFXTranslateOptions;

typedef struct _TXTTranslateOptions {
	BYTE **pTextBuffer;
	DWORD  TextBodyLength;
	std::vector<DWORD> LinePointers;
	DWORD EncodingID;
} TXTTranslateOptions;

typedef struct _IconDef {
	BITMAPINFOHEADER bmi;
	void *pImage;
	AspectRatio Aspect;
} IconDef;

typedef std::map<DWORD, IconDef> ResolvedIconList;
typedef ResolvedIconList::iterator ResolvedIcon_iter;

typedef struct _Panel {
	DWORD ID;
	BYTE  Text[ 256 ];
	DWORD Width;
	DWORD Flags;
	DWORD FontID;
} Panel;

typedef std::map<DWORD, Panel> PanelList;
typedef PanelList::iterator    PanelIterator;

typedef enum _PanelFlags {
	PF_LEFT   = 0x00000001,
	PF_CENTER = 0x00000002,
	PF_RIGHT  = 0x00000004,
	PF_SIZER  = 0x00000008,
	PF_SPARE  = 0x00000010,
} PanelFlags;

typedef struct _FSTool {
	HBITMAP  ToolIcon;
	std::wstring ToolName;
	std::wstring ToolDesc;
} FSTool;

typedef std::vector<FSTool> FSToolList;
typedef FSToolList::iterator FSToolIterator;

#define FILEOP_SUCCESS      0
#define FILEOP_NEEDS_ASCII  1
#define FILEOP_DELETE_FILE  2
#define FILEOP_COPY_FILE    3
#define FILEOP_EXISTS       4
#define FILEOP_ISDIR        5
#define FILEOP_WARN_ATTR    6

#define BlocksWide  23
#define BlocksHigh  28

#define TotalBlocks ( BlocksWide * BlocksHigh )

#define NixWindow( W ) if ( W != nullptr ) { DestroyWindow( W ); W = nullptr; }

#define	WM_ENTERICON        (WM_APP + 0)
#define WM_GOTOPARENT       (WM_APP + 1)
#define	WM_UPDATESTATUS     (WM_APP + 2)
#define	WM_SCPALCHANGED     (WM_APP + 3)
#define WM_FVSTARTDRAG      (WM_APP + 4)
#define WM_FVENDDRAG        (WM_APP + 5)
#define WM_COPYOBJECT       (WM_APP + 6)
#define WM_FORMATPROGRESS   (WM_APP + 7)
#define	WM_SETDRAGTYPE      (WM_APP + 8)
#define WM_IDENTIFYFONT     (WM_APP + 9)
#define WM_ROOTFS           (WM_APP + 10)
#define WM_FILEOP_PROGRESS  (WM_APP + 11)
#define WM_FILEOP_REDRAW    (WM_APP + 12)
#define WM_FILEOP_SUSPEND   (WM_APP + 13)
#define WM_NOTIFY_FREE      (WM_APP + 14)
#define WM_DOENTERAS        (WM_APP + 15)
#define WM_RESETPALETTE     (WM_APP + 16)
#define WM_PALETTECLOSED    (WM_APP + 17)
#define WM_SCCLOSED         (WM_APP + 18)
#define WM_FSTOOL_SETDESC   (WM_APP + 19)
#define WM_FSTOOL_PROGLIMIT (WM_APP + 20)
#define WM_FSTOOL_PROGRESS  (WM_APP + 21)
#define WM_FSTOOL_CURRENTOP (WM_APP + 22)
#define WM_FV_FOCUS         (WM_APP + 23)
#define WM_INSTALLOBJECT    (WM_APP + 24)

#define TUID_TEXT           0x73477347
