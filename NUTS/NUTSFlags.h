#ifndef NUTSFLAGS_H
#define NUTSFLAGS_H

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
	FSF_Imaging          = 0x00020000, /* The Filesystem can participate in the Imaging wizard */
	FSF_No_Quick_Format  = 0x00040000, /* When formatting the filesystem do not offer the quick-format option */
	FSF_Uses_Extensions  = 0x00080000, /* Files have extensions */
	FSF_Fake_Extensions  = 0x00100000, /* File extensions are provided visually by the originating system, but they are not changeable by the user */
	FSF_NoDir_Extensions = 0x00200000, /* Directories may not have extensions (only files) */
	FSF_Accepts_Sidecars = 0x00400000, /* Indicates this FS should have sidecars exported to/imported from it, if the other FS exports/imports sidecars */
	FSF_Supports_FOP     = 0x00800000, /* Indicates this FS can contain "Foreign Objects" and so extra options (like dir type) should be presented */
	FSF_NoInPlaceAttrs   = 0x01000000, /* This FS does not support changing attributes in place. Attributes can only be set by writing a file. */
} FSFlags;

typedef enum _DSFlags {
	DS_RawDevice         = 0x00000001, /* The data source is attached to a raw device, e.g. CF card, floppy drive, etc. */
	DS_SupportsLLF       = 0x00000002, /* The data source is attached to a device that supports Low Level Formatting. */
	DS_AlwaysLLF         = 0x00000004, /* The data source is attached to a device that requires Low Level Formatting every time */
	DS_SupportsTruncate  = 0x00000008, /* The data source does not require the extent to match the capacity */
	DS_SlowAccess        = 0x00000010, /* The data source is slow to respond, so enhanced interrogation processes should not be used */
	DS_ReadOnly          = 0x00000010, /* The data source does not support write operations. */
} DSFlags;

typedef enum _TXFlags {
	TXTextTranslator  = 0x00000001,
	TXGFXTranslator   = 0x00000002,
	GFXLogicalPalette = 0x00000004,
	GFXMultipleModes  = 0x00000008,
	TXAUDTranslator   = 0x00000010,
} TXFlags;

typedef enum _RootHookFlags {
	RHF_CreatesFileSystem = 0x00000001,
	RHF_CreatesDataSource = 0x00000002,
} RootHookFlags;

typedef enum _PanelFlags {
	PF_LEFT   = 0x00000001,
	PF_CENTER = 0x00000002,
	PF_RIGHT  = 0x00000004,
	PF_SIZER  = 0x00000008,
	PF_SPARE  = 0x00000010,
} PanelFlags;

typedef enum _TapeFlag {
	TF_ExtraValid = 0x00000001,
	TF_Extra1     = 0x00000002,
	TF_Extra2     = 0x00000004,
	TF_Extra3     = 0x00000008,
} TapeFlag;

#endif
