#pragma once

#define SSector    Attributes[0]
#define AttrLocked Attributes[1]
#define AttrRead   Attributes[2]
#define AttrWrite  Attributes[3]
#define LoadAddr   Attributes[4]
#define ExecAddr   Attributes[5]
#define SeqNum     Attributes[6]
#define RISCTYPE   Attributes[7]
#define TimeStamp  Attributes[8]

#define FSID_DFS_40    0x0DF50101
#define FSID_DFS_80    0x0DF50102
#define FSID_DFS_DSD   0x0DF50103
#define FSID_ADFS_S    0xADF50101
#define FSID_ADFS_L    0xADF50102
#define FSID_ADFS_L2   0xADF50203
#define FSID_ADFS_M    0xADF50103
#define FSID_ADFS_H    0xADF50104
#define FSID_ADFS_HN   0xADF50105
#define FSID_ADFS_D    0xADF50204
#define FSID_ADFS_E    0xADF50201
#define FSID_ADFS_F    0xADF50202
#define FSID_SPRITE    0x50A17E01

#define FT_ACORN  0xAC060101
#define FT_SPRITE 0xAC060102

#define ENCODING_ACORN 0xAC060000

#define FONTID_ACORN    0xAC060001
#define FONTID_TELETEXT 0x7E1E7EF7

#define PLUGINID_ACORN  0xAC060001

#define GRAPHIC_ACORN   0xAC060002
#define GRAPHIC_SPRITE  0xAC060003
#define BBCBASIC        0xAC060004

typedef struct _FreeSpace {
	long	StartSector;
	long	Length;
	long	OccupiedSectors;
} FreeSpace;

typedef struct _Fragment {
	DWORD FragID;
	WORD  Zone;
	DWORD  Length;
	DWORD FragOffset;
} Fragment;

typedef struct _FileFragment {
	DWORD Sector;
	DWORD Length;
} FileFragment;

typedef std::vector<Fragment> FragmentList;
typedef std::vector<FileFragment> FileFragments;
typedef FragmentList::iterator Fragment_iter;
typedef FileFragments::iterator FileFragment_iter;



