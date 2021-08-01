#pragma once

/* 800K ADFS D Format is weird. It uses the old map format, but
   with 1024-byte sectors instead of 256-byte. But, it still
   uses references that work with 256-byte sectors. As such, the
   root directory is on logical sector 1, which is physical sector
   4 on track 0.

   The first macro shifts the sector number right 2 places if D
   format is in use. All D format sector references have the
   bottom two bits clear, so no data is lost.

   The sector format gives the sector size. 1024 bytes for D format,
   256 bytes for all others.

   Note that hard drives always use 256-byte sectors, regardless of
   Acorn Winchester style or Risc OS style (old map).

   New map Risc OS hard drives use 512-byte sectors. The disc record
   of new map discs indicates what sector size is to be used, and all
   sector references are in terms of the recorded sector size.
*/
#define DSector( X ) ( (UseDFormat)?(X>>2):X )
#define DSectorSize ( (UseDFormat)?1024:256 )

#define PlusFormat ( ( FSID == FSID_ADFS_EP ) || ( FSID == FSID_ADFS_FP ) || ( FSID == FSID_ADFS_G ) || ( FSID == FSID_ADFS_HP ) )

#define SSector    Attributes[0]
#define AttrLocked Attributes[1]
#define AttrRead   Attributes[2]
#define AttrPrefix Attributes[2]
#define AttrWrite  Attributes[3]
#define LoadAddr   Attributes[4]
#define ExecAddr   Attributes[5]
#define SeqNum     Attributes[6]
#define RISCTYPE   Attributes[7]
#define TimeStamp  Attributes[8]
#define AttrExec   Attributes[9]

#define FSID_DFS_40    0x0DF50101
#define FSID_DFS_80    0x0DF50102
#define FSID_DFS_DSD   0x0DF50103
#define FSID_ADFS_S    0xADF50101
#define FSID_ADFS_L    0xADF50102
#define FSID_ADFS_M    0xADF50103
#define FSID_ADFS_H    0xADF50104
#define FSID_ADFS_H8   0xADF581DE
#define FSID_ADFS_HO   0xADF50105
#define FSID_ADFS_HN   0xADF50106
#define FSID_ADFS_HP   0xADF50107
#define FSID_ADFS_D    0xADF50201
#define FSID_ADFS_E    0xADF50202
#define FSID_ADFS_F    0xADF50203
#define FSID_ADFS_L2   0xADF50204
#define FSID_ADFS_EP   0xADF50205
#define FSID_ADFS_FP   0xADF50206
#define FSID_ADFS_G    0xADF50207
#define FSID_SPRITE    0x50A17E01

#define FT_ACORN  0xAC060101
#define FT_SPRITE 0xAC060102
#define FT_ACORNX 0xAC060103

#define ENCODING_ACORN  0xAC060000
#define ENCODING_RISCOS 0xAC060001

#define FONTID_ACORN    0xAC060001
#define FONTID_TELETEXT 0x7E1E7EF7
#define FONTID_RISCOS   0xAC060002

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
	DWORD Zone;
} FileFragment;

typedef std::vector<Fragment> FragmentList;
typedef std::vector<FileFragment> FileFragments;
typedef FragmentList::iterator Fragment_iter;
typedef FileFragments::iterator FileFragment_iter;

typedef struct _TargetedFileFragments
{
	DWORD FragID;
	DWORD SectorOffset;
	FileFragments Frags;
} TargetedFileFragments;

typedef struct _CompactionObject
{
	DWORD StartSector;
	DWORD NumSectors;
	DWORD ContainingDirSector;
	DWORD ReferenceFileID;
} CompactionObject;
