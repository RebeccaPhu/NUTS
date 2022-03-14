#pragma once

#include <vector>
#include <map>
#include <string>

#include "BYTEString.h"

#define ENCODING_ASCII L"ASCII_Encoding"
#define FT_NONE        L"NOT_USED_OBJECT"
#define FT_WINDOWS     L"WindowsFiletype"
#define FT_ROOT        L"RootObject"
#define FT_UNSET       L"UNSET_FILE_OBJECT"
#define FT_NULL        L"NullObject"
#define FT_ZIP         L"ZIPFileContent"
#define FT_ISO         L"ISOFileContent"
#define FSID_NONE      L"Undefined_FileSystem"
#define FSID_ISO9660   L"ISO9660"
#define FSID_ISOHS     L"HighSierra"
#define FS_Null        L"NULL_FileSystem"
#define FS_Root        L"Root_FileSystem"
#define FS_Windows     L"Windows_FileBrowser"
#define TX_Null        L"NULL_Translator"
#define Provider_Null  L"NULL_Provider"

typedef std::wstring FSIdentifier;
typedef std::wstring FTIdentifier;
typedef std::wstring FontIdentifier;
typedef std::wstring EncodingIdentifier;
typedef std::wstring TXIdentifier;
typedef std::wstring PluginIdentifier;
typedef std::wstring ProviderIdentifier;

typedef unsigned long long QWORD;

#define NUTS_PREFERRED_API_VERSION 0x00000001

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
	FT_Data      =  8, // Arbitrary data, as distinct from Code, BASIC, Script or Spool.
	FT_Graphic   =  9, // Graphical data in some way.
	FT_Sound     = 10, // Audio data in some way.
	FT_Pref      = 11, // Some kind of settings file
	FT_App       = 12, // Application (e.g. RiscOS application)
	FT_DiskImage = 13, // Image of a disk
	FT_TapeImage = 14, // Image of government red tape
	FT_HardImage = 15, // Image of the Portsmouth Dockyard
	FT_MiscImage = 16, // Image of something else
	FT_CDROM     = 17, // CDROM Drive
	FT_HardDisc  = 18, // Hard Drive
	FT_Floppy    = 19, // Floppy Drive
	FT_Directory = 20, // Directory (J, R, Hartley)
	FT_Archive   = 21, // Archive (ZIP, LHA, etc)
	FT_System    = 22, // System (the top level)
	FT_Windows   = 23, // Windows Volume
	FT_ROMDisk   = 24, // ROMDisk
	FT_CDImage   = 25, // CDROM Disc Image
	FT_MemCard   = 26, // Memory card
} FileType;

typedef struct _NativeFile {
	_NativeFile() {
		Filename        = (BYTE *) "";
		Extension       = (BYTE *) "";
		Flags           = 0;
		Length          = 0;
		Type            = FT_Arbitrary;
		Icon            = FT_Arbitrary;
		XlatorID        = TX_Null;
		EncodingID      = ENCODING_ASCII;
		HasResolvedIcon = false;
		ExtraForks      = 0;
		pAuxData        = nullptr;
		lAuxData        = 0;
		FSFileType      = FT_NONE;
		FSFileTypeX     = FT_NONE;
	}
	DWORD fileID;          // File index into the pDirectory->Files vector
	BYTEString  Filename;  // Filename, in EncodingID encoding
	BYTEString  Extension; // Extension (if Flags & FF_Extension), in EncodingID encoding 
	DWORD Flags;           // File flags
	QWORD Length;          // Length in bytes
	DWORD Attributes[32];  // Arbitrary set of FS-specific attributes
	FileType Type;         // Type of file in terms of content (see below)
	DWORD Icon;            // Icon number to display (can be FT)
	EncodingIdentifier EncodingID;      // Text encoding of filename/extension
	FTIdentifier       FSFileType;      // Type of file in terms of filesystem, e.g. "This is a C64 file".
	FTIdentifier       FSFileTypeX;     // Type of file in terms of filesystem containing it, e.g. "This is a CDROM ISO file".
	TXIdentifier       XlatorID;        // ID of a translator plugin that can deal with this file
	bool  HasResolvedIcon; // Has a icon resolved from the file system itself (Dynamic icon)
	BYTE  ExtraForks;      // Number of forks (additional to the data fork) used by this file
	BYTE  *pAuxData;       // Pointer to auxilliary directory data
	DWORD lAuxData;        // Size of auxilliary directory data
} NativeFile;

typedef std::vector<NativeFile>::iterator NativeFileIterator;

typedef enum _ActionID {
	AA_NONE        = 0,
	AA_FORMAT      = 1,
	AA_COPY        = 2,
	AA_DELETE      = 3,
	AA_PROPS       = 4,
	AA_SET_PROPS   = 5,
	AA_NEWIMAGE    = 6,
	AA_SET_FSPROPS = 7,
	AA_INSTALL     = 8,
	AA_DELETE_FS   = 9,
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
	FF_Directory        = 0x00000001, /* FS item is a directory */
	FF_Extension        = 0x00000002, /* FS item has an extension to it's filename */
	FF_EncodingOverride = 0x00000004, /* FS item should override the FS encoding */
	FF_NotRenameable    = 0x00000008, /* FS item cannot be renamed */
	FF_Audio            = 0x00000010, /* FS item contains renderable audio */
	FF_AvoidSidecar     = 0x00000020, /* Target FS has stored aux data separately, so a normally required sidecar is now not required */
	FF_Pseudo           = 0x40000000, /* FS item is a pseudo item (e.g. Drive in AcornDSD) */
} FileFlags;

typedef struct _FormatMenu {
	std::wstring FS;
	FSIdentifier ID;
} FormatMenu;

typedef struct _FSMenu {
	std::wstring Provider;
	std::vector<FormatMenu> FS;
} FSMenu;

typedef struct _FSHint {
	FSIdentifier FSID;
	WORD  Confidence;
} FSHint;

typedef std::vector<FSHint> FSHints;

typedef struct _TitleComponent {
	BYTE String[512];
	EncodingIdentifier Encoding;
} TitleComponent;

typedef enum _TitleFlags {
	TF_Final    = 0x00000001,
	TF_Titlebar = 0x00000002,
	TF_FileOps  = 0x00000004,
} TitleFlags;

typedef enum _FSFlags {
	FSF_Formats_Raw      = 0x00000001, /* FileSystem can be placed on a raw device such as a memory card or hard drive - NOT FLOPPIES */
	FSF_Formats_Image    = 0x00000002, /* FileSystem can be used to format an image - everything should have this, but you never know */
	FSF_Creates_Image    = 0x00000004, /* FileSystem can be used to create a new image - likewise */
	FSF_Supports_Spaces  = 0x00000008, /* Filenames can have spaces in them */
	FSF_Supports_Dirs    = 0x00000010, /* Subdirectories can exist */
	FSF_ArbitrarySize    = 0x00000020, /* The image can be created with an arbitrary image file size */
	FSF_UseSectors       = 0x00000040, /* The image size should be a multiple of sector size */
	FSF_DynamicSize      = 0x00000080, /* The image size should be as small as possible to begin with - it will grow as needed */
	FSF_FixedSize        = 0x00000100, /* The image should have a fixed specific size and never change */
	FSF_ReadOnly         = 0x00000200, /* The filesystem cannot be written to. Ever. EVER. */
	FSF_SupportFreeSpace = 0x00000400, /* The filesystem can report free space - Tape images cannot do this, e.g. */
	FSF_SupportBlocks    = 0x00000800, /* The filesystem can report used blocks (not necessarily sectors) - Tape images cannot do this, e.g. */
	FSF_Size             = 0x00001000, /* The filesystem can report its image size. Image sources use this, but it may not be available on physical devices. */
	FSF_Capacity         = 0x00002000, /* The filesystem can report its maximum capacity */
	FSF_Reorderable      = 0x00004000, /* Files in the filesystem are orderable, and order is (probably) important. Allow the user to re-order files */
	FSF_Exports_Sidecars = 0x00008000, /* When exporting files to Windows FSes, the filesystem can export a sidecar file. Also, import/from. */
	FSF_Prohibit_Nesting = 0x00010000, /* The Filesystem cannot allow another filesystem image inside of it. Chiefly used for special FSes like Root and AcornDSD. */
	FSF_Uses_DSK         = 0x00020000, /* The filesystem is represented in image form using the CPCEMU DSK format */
	FSF_No_Quick_Format  = 0x00040000, /* When formatting the filesystem do not offer the quick-format option */
	FSF_Uses_Extensions  = 0x00080000, /* Files have extensions */
	FSF_Fake_Extensions  = 0x00100000, /* File extensions are provided visually by the originating system, but they are not changeable by the user */
	FSF_NoDir_Extensions = 0x00200000, /* Directories may not have extensions (only files) */
	FSF_Accepts_Sidecars = 0x00400000, /* Indicates this FS should have sidecars exported to/imported from it, if the other FS exports/imports sidecars */
	FSF_Supports_FOP     = 0x00800000, /* Indicates this FS can contain "Foreign Objects" and so extra options (like dir type) should be presented */
} FSFlags;

typedef enum _DSFlags {
	DS_RawDevice         = 0x00000001, /* The data source is attached to a raw device, e.g. CF card, floppy drive, etc. */
	DS_SupportsLLF       = 0x00000002, /* The data source is attached to a device that supports Low Level Formatting. */
	DS_AlwaysLLF         = 0x00000004, /* The data source is attached to a device that requires Low Level Formatting every time */
	DS_SupportsTruncate  = 0x00000008, /* The data source does not require the extent to match the capacity */
	DS_SlowAccess        = 0x00000010, /* The data source is slow to respond, so enhanced interrogation processes should not be used */
	DS_ReadOnly          = 0x00000010, /* The data source does not support write operations. */
} DSFlags;

typedef struct _FormatDesc {
	std::wstring Format;
	BYTEString   PreferredExtension;
	FSIdentifier FSID;
	DWORD        Flags;
	QWORD        MaxSize;
	DWORD        SectorSize;
} FormatDesc;

typedef std::vector<FormatDesc> FormatList;
typedef std::vector<FormatDesc>::iterator FormatDesc_iter;

typedef enum _FormatFlags {
	FTF_LLF          = 0x00000001,
	FTF_Blank        = 0x00000002,
	FTF_Initialise   = 0x00000004,
	FTF_Truncate     = 0x00000008
} FormatType;

typedef struct _FSSpace {
	QWORD UsedBytes;
	QWORD Capacity;
	BYTE *pBlockMap;
} FSSpace;

typedef enum _BlockType {
	BlockFree   = 0,
	BlockUsed   = 1,
	BlockFixed  = 2,
	BlockAbsent = 0xFF,
} BlockType;

typedef enum _AttrType {
	AttrVisible  = 0x00000001, /* Attribute is visible on the Properties page */
	AttrEnabled  = 0x00000002, /* Attribute can be edited */
	AttrSelect   = 0x00000004, /* Attribute is one item from a list */
	AttrBool     = 0x00000008, /* Attribute is a checkbox */
	AttrCombo    = 0x00000010, /* Attribute is one item from a list OR a free form text. */
	AttrNumeric  = 0x00000020, /* Attribute is a number type as free form text */
	AttrHex      = 0x00000040, /* Attribute is in hexadecimal */
	AttrOct      = 0x00000080, /* Attribute is in octal */
	AttrDec      = 0x00000100, /* Attribute is in decimal (BOOR-RING) */
	AttrWarning  = 0x00000200, /* Attribute should trigger a warning on application that changing it will have non-innocuous (but not dangerous) effects */
	AttrDanger   = 0x00000400, /* Attribute should trigger a warning on application that changing it will probably be a really stupid idea */
	AttrFile     = 0x00000800, /* Attribute applies to files in the filesystem */
    AttrDir      = 0x00001000, /* Attribute applies to directories in the filesystem */
	AttrNegative = 0x00002000, /* Attribute supports negative numbers */
	AttrString   = 0x00004000, /* Attribute is a free-form text string */
	AttrTime     = 0x00008000, /* Attribute is a time stamp */
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

typedef enum _TXFlags {
	TXTextTranslator  = 0x00000001,
	TXGFXTranslator   = 0x00000002,
	GFXLogicalPalette = 0x00000004,
	GFXMultipleModes  = 0x00000008,
	TXAUDTranslator   = 0x00000010,
} TXFlags;

#define ALL_TX ( TXTextTranslator | TXGFXTranslator | TXAUDTranslator )

typedef enum _RootHookFlags {
	RHF_CreatesFileSystem = 0x00000001,
	RHF_CreatesDataSource = 0x00000002,
} RootHookFlags;

typedef std::vector<AttrDesc> AttrDescriptors;
typedef std::vector<AttrDesc>::iterator AttrDesc_iter;
typedef std::vector<AttrOption> AttrOptions;
typedef std::vector<AttrOption>::iterator AttrOpt_iter;

typedef enum _LCFlags {
	LC_IsChecked   = 0x00000001, /* Menu entry has a check mark */
	LC_IsSeparator = 0x00000002, /* Menu entry is a separator line */
	LC_ApplyNone   = 0x00000004, /* Menu entry applies when NO FS items are selected */
	LC_ApplyOne    = 0x00000008, /* Menu entry applies when ONE FS item is selected */
	LC_ApplyMany   = 0x00000010, /* Menu entry applies when MORE THAN ONE FS item are selected */
	LC_Disabled    = 0x00000020, /* Menu entry is disabled */
    LC_Always      = 0x0000001C, /* Menu entry applies REGARDLESS of how many FS items are selected */
	LC_OneOrMore   = 0x00000018, /* Menu entry applies when ONE OR MORE FS items are selected */
} LCFlags;

typedef struct _LocalCommand {
	DWORD Flags;
	std::wstring Name;
} LocalCommand;

typedef struct _LocalCommands {
	bool HasCommandSet;
	std::wstring Root;
	std::vector<LocalCommand> CommandSet;
} LocalCommands;

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
	HWND  NotifyWnd;
} GFXTranslateOptions;

typedef struct _TXTTranslateOptions {
	BYTE **pTextBuffer;
	DWORD  TextBodyLength;
	std::vector<DWORD> LinePointers;
	EncodingIdentifier EncodingID;
	HWND  ProgressWnd;
	HANDLE hStop;
	WORD  CharacterWidth;
	bool  RetranslateOnResize;
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
	FontIdentifier FontID;
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

typedef struct _SidecarExport {
	BYTEString Filename;
	void *FileObj;
} SidecarExport;

typedef struct _SidecarImport {
	BYTEString Filename;
} SidecarImport;

typedef std::vector<FSTool> FSToolList;
typedef FSToolList::iterator FSToolIterator;

typedef struct _RootCommand {
	PluginIdentifier PLID;
	DWORD CmdIndex;
	std::wstring Text;
} RootCommand;

typedef enum _RootCommandResult {
	GC_ResultNone        = 0,
	GC_ResultRefresh     = 1,
	GC_ResultRootRefresh = 2,
} RootCommandResult;

typedef std::vector<RootCommand> RootCommandSet;
typedef std::vector<RootCommand>::iterator RootCommandSetIterator;

/* Physical disk definitions */
typedef struct _DiskShape {
	DWORD Tracks;
	WORD  Sectors;
	WORD  Heads;
	WORD  TrackInterleave;
	WORD  LowestSector;
	WORD  SectorSize;
	bool  InterleavedHeads;
} DiskShape;

typedef struct _TrackSection {
	BYTE  Value;
	BYTE  Repeats;
} TrackSection;

typedef struct _SectorDefinition {
	TrackSection GAP2;
	TrackSection GAP2PLL;
	TrackSection GAP2SYNC;
	BYTE         IAM;
	BYTE         Track;
	BYTE         Side;
	BYTE         SectorID;
	BYTE         SectorLength;
	BYTE         IDCRC;
	TrackSection GAP3;
	TrackSection GAP3PLL;
	TrackSection GAP3SYNC;
	BYTE         DAM;
	BYTE         Data[1024];
	BYTE         DATACRC;
	TrackSection GAP4;
} SectorDefinition;

typedef enum _DiskDensity {
	SingleDensity = 1,
	DoubleDensity = 2,
	QuadDesnity   = 3,
	OctalDensity  = 4,
} DiskDensity;

typedef struct _TrackDefinition {
	DiskDensity  Density;

	TrackSection GAP1;

	std::vector<SectorDefinition> Sectors;

	BYTE         GAP5;
} TrackDefinition;

typedef enum _TapeFlag {
	TF_ExtraValid = 0x00000001,
	TF_Extra1     = 0x00000002,
	TF_Extra2     = 0x00000004,
	TF_Extra3     = 0x00000008,
} TapeFlag;

typedef struct _TapeCue {
	DWORD IndexPtr;
	std::wstring IndexName;
	std::wstring Extra;
	DWORD Flags;
} TapeCue;

typedef struct _TapeIndex {
	std::wstring Title;
	std::wstring Publisher;
	std::vector<TapeCue> Cues;
} TapeIndex;

#define FILEOP_SUCCESS      0
#define FILEOP_NEEDS_ASCII  1
#define FILEOP_DELETE_FILE  2
#define FILEOP_COPY_FILE    3
#define FILEOP_EXISTS       4
#define FILEOP_ISDIR        5
#define FILEOP_WARN_ATTR    6
#define CMDOP_REFRESH       7
#define FILEOP_DIR_EXISTS   8
#define FILEOP_ISFILE       9

#define BlocksWide  23
#define BlocksHigh  28

#define TotalBlocks ( BlocksWide * BlocksHigh )

#define NixWindow( W ) if ( W != nullptr ) { DestroyWindow( W ); W = nullptr; }
#define NixObject( O ) if ( O != NULL ) { DeleteObject( O ); O = NULL; }

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
#define WM_ABOUT_RESIZE     (WM_APP + 25)
#define WM_TEXT_PROGRESS    (WM_APP + 26)
#define WM_EXTERNALDROP     (WM_APP + 27)
#define WM_THREADDONE       (WM_APP + 28)
#define WM_FILEOP_NOTICE    (WM_APP + 29)
#define WM_REFRESH_PANE     (WM_APP + 30)
#define WM_FONTCHANGER      (WM_APP + 31)
#define WM_CHARMAPCHAR      (WM_APP + 32)
#define WM_RENAME_FILE      (WM_APP + 33)
#define WM_NEW_DIR          (WM_APP + 34)
#define WM_AUDIOELEMENT     (WM_APP + 35)
#define WM_TAPEKEY_DOWN     (WM_APP + 36)
#define WM_TAPEKEY_UP       (WM_APP + 37)
#define WM_CUEINDEX_JUMP    (WM_APP + 38)
#define WM_TBCLOSED         (WM_APP + 39)
#define WM_OPENCHARMAP      (WM_APP + 40)
#define WM_ENDSPLASH        (WM_APP + 41)
#define WM_SETDIRTYPE       (WM_APP + 42)

#define TUID_TEXT           L"Text_Translator"
#define TUID_MOD_MUSIC      L"MOD_Music_Translator"
#define TUID_HEX            L"Hex_Dump_Translator"



#define IDM_MOVEUP    54001
#define IDM_MOVEDOWN  54002
#define IDM_ROOTFS    54003
#define IDM_FONTSW    54004
#define IDM_PARENT    54005
#define IDM_DIRTYPE   54006

