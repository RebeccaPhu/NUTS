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
#define DSector( X ) ( (FSID==FSID_ADFS_D)?(X>>2):X )
#define DSectorSize ( (FSID==FSID_ADFS_D)?1024:256 )

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

#define FSID_DFS_40    L"Acorn_DFS_40T"
#define FSID_DFS_80    L"Acorn_DFS_80T"
#define FSID_DFS_DSD   L"Acorn_DFS_DS_Interleave"
#define FSID_ADFS_S    L"Acorn_ADFS_40T"
#define FSID_ADFS_M    L"Acorn_ADFS_80T"
#define FSID_ADFS_L    L"Acorn_ADFS_DS_Interleave"
#define FSID_ADFS_H    L"Acorn_ADFS_8Bit_HD"
#define FSID_ADFS_H8   L"Acorn_ADFS_8Bit_IDE_HD"

#define FSID_ADFS_L2   L"RiscOS_640K_L"
#define FSID_ADFS_D    L"RiscOS_800K_D"
#define FSID_ADFS_E    L"RiscOS_800K_E"
#define FSID_ADFS_EP   L"RiscOS_800K_E+"
#define FSID_ADFS_F    L"RiscOS_1.6M_F"
#define FSID_ADFS_FP   L"RiscOS_1.6M_F+"
#define FSID_ADFS_G    L"RiscOS_3.2M_G"
#define FSID_ADFS_HO   L"RiscOS_OldMap_HD"
#define FSID_ADFS_HN   L"RiscOS_NewMap_HD"
#define FSID_ADFS_HP   L"RiscOS_HD+"
#define FSID_SPRITE    L"RiscOS_SpritFile"

#define WID_EMUHDR     L"RiscOSEmuHdr"

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

extern const EncodingIdentifier ENCODING_ACORN;
extern const EncodingIdentifier ENCODING_RISCOS;

extern const FTIdentifier FT_ACORN;
extern const FTIdentifier FT_SPRITE;
extern const FTIdentifier FT_ACORNX;

extern const TXIdentifier GRAPHIC_ACORN;
extern const TXIdentifier GRAPHIC_SPRITE;
extern const TXIdentifier BBCBASIC;
extern const TXIdentifier AUDIO_ARMADEUS;
