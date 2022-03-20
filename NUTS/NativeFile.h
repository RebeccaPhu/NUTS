#ifndef NATIVEFILE_H
#define NATIVEFILE_H

#include "stdafx.h"

#include <vector>
#include <map>
#include <string>

#include "NUTSTypes.h"
#include "BYTEString.h"

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

typedef enum _FileFlags {
	FF_Directory        = 0x00000001, /* FS item is a directory */
	FF_Extension        = 0x00000002, /* FS item has an extension to it's filename */
	FF_EncodingOverride = 0x00000004, /* FS item should override the FS encoding */
	FF_NotRenameable    = 0x00000008, /* FS item cannot be renamed */
	FF_Audio            = 0x00000010, /* FS item contains renderable audio */
	FF_AvoidSidecar     = 0x00000020, /* Target FS has stored aux data separately, so a normally required sidecar is now not required */
	FF_Pseudo           = 0x40000000, /* FS item is a pseudo item (e.g. Drive in AcornDSD) */
} FileFlags;

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
		FSFileType      = FT_NULL;
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

#endif