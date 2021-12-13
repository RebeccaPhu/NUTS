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
#define DSector( X ) ( (MYFSID==FSID_ADFS_D)?(X>>2):X )
#define DSectorSize ( (MYFSID==FSID_ADFS_D)?1024:256 )

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

#define FSID_DFS_40    0x00000000
#define FSID_DFS_80    0x00000001
#define FSID_DFS_DSD   0x00000002
#define FSID_ADFS_S    0x00000003
#define FSID_ADFS_M    0x00000004
#define FSID_ADFS_L    0x00000005
#define FSID_ADFS_H    0x00000006
#define FSID_ADFS_H8   0x00000007

#define FSID_ADFS_L2   0x00000100
#define FSID_ADFS_D    0x00000101
#define FSID_ADFS_E    0x00000102
#define FSID_ADFS_EP   0x00000103
#define FSID_ADFS_F    0x00000104
#define FSID_ADFS_FP   0x00000105
#define FSID_ADFS_G    0x00000106
#define FSID_ADFS_HO   0x00000107
#define FSID_ADFS_HN   0x00000108
#define FSID_ADFS_HP   0x00000109
#define FSID_SPRITE    0x0000010A

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

extern DWORD ENCODING_ACORN;
extern DWORD ENCODING_RISCOS;

extern DWORD FT_ACORN;
extern DWORD FT_SPRITE;
extern DWORD FT_ACORNX;

extern DWORD GRAPHIC_ACORN;
extern DWORD GRAPHIC_SPRITE;
extern DWORD BBCBASIC;
extern DWORD AUDIO_ARMADEUS;
