#include "StdAfx.h"
#include "BitmapCache.h"

CBitmapCache BitmapCache;

CBitmapCache::CBitmapCache(void)
{
}

CBitmapCache::~CBitmapCache(void)
{
}

// ICON credits

/*
IDB_ACORN:      "Acorn Icon", FatCow Hosting Extra Icons 2, FatCow, CC Attribution 3.0 US
IDB_APP:        "Application Icon", Blue Bits Icons, Icojam, Freeware
IDB_BINARY:     "Mimetypes Binary Icon", Crystal Clear Icons, Everaldo Coelho, LGPL.
IDB_CDROM:      "CD-Rom Icon", Influens Icons, Mat-u, CC Attribution-NonCommerical-NoDerivs 3.0 Unported
IDB_DATA:       "Server Icon", Latte Developer Icons, Macintex, CC Attribution 3.0 Unported
IDB_DISKIMAGE:  "Floppy Icon", NX10 Icon Set, MazelNL77, CC Attribution 3.0 Unported
IDB_FLOPPYDISC: "Floppy Drive 3,5 Icon", DeepSea Blue Icons, MiRa, CC Attribution-Non-Commercial-NoDervis 3.0 Unported
IDB_FOLDER:     "Generic Folder Blue Icon", Smooth Leopard Icons, McDo Design, Free for non-commerical use.
IDB_GRAPHIC:    "Graphic File Icon", 32x32 Free Design Icons, Aha-Soft, CC Attribution 3.0 US
IDB_HARDDISC:   "Hard Drive Icon", Refresh CL Icons, TPDK, CC Attribution-NonCommercial-NoDerivs 3.0 Unported
IDB_SCRIPT:     "Script Icon", Stainless! Applications Icons, IconLeak, Unrestricted
IDB_TEXT:       "Text Icon", Crystal Office Icon Set, MediaJon (www.iconshots.com), Linkware
IDB_TAPEIMAGE:  "B Side Icon", Cassette Tape Icons, barkerbaggies, CC Attribution-NonCommercial-ShareAlike 3.0 Unported
IDB_HARDIMAGE:  "Apps Hard Drive 2 Icon", Crystal Project Icons, Everaldo Coelho, LGPL
IDB_ARCHIVE:    "Light Brown ZIP Icon", Cats Icons 2, McDo Design, Free for non-commerical use.
IDB_SYSTEM:     "Computer Icon", Aero Icons, Lokas Software, CC Attribution 3.0 Unported.
IDB_WINDOWS:    "Windows Icon", Crystal Intense Icons, Tatice, CC Attribution-Noncommercial-NoDervis 3.0
IDB_MUSICFILE:  "File Music Icon", Phuzion Icons, kyo-tux, CC Attribution-Noncommerical-ShareALike 3.0

IDI_NEWDIR:     "New Folder Graphite Icon", Aqua Candy Revolution Icons, McDo Design, Free for non-commercial use
IDI_FONTSWITCH: "Font Icon", FatCow Hosting Icons, FatCow, CC Attribution 3.0 US
IDI_PRINT:      "Print Icon", 12x12 Free Toolbar Icons, Aha-Soft, CC Attribution 3.0 US
IDI_SAVE:       "Save Icon", 12x12 Free Toolbar Icons, Aha-Soft, CC Attribution 3.0 US
IDI_COPY:       "Copy Icon", Mini Icons 2, Brand Spanking New, CC Attribution-ShareAlike 2.5 Generic
IDI_UPFILE:     "Move File Up Icon", Blue Bits Icons, Icojam, Freeware
IDI_DOWNFILE:   "Move File Down Icon", Blue Bits Icons, Icojam, Freeware
IDI_RENAME:     "Text Field Rename Icon", Web Design Icon Set, SEM Labs, Freeware
IDI_COPYFILE:   "Copy Icon", Funktional Icons, Creative Freedom, CC Attribution-NoDerivs 3.0 Unported
IDI_DELETEFILE: "Delete Icon", Funktional Icons, Creative Freedom, CC Attribution-NoDerivs 3.0 Unported
IDI_AUDIO:      "Sound Icon", FatCow Hosting Icons, FatCow, CC Attribution 3.0 US
IDI_ROOTFS:     "Actions Top Icon", Glaze Icons, Marco Martin, GNU Lesser General Public License
IDI_REFRESH:    "Refresh Icon", 16x16 Free Application Icons, Aha-Soft, CC Attribution-ShareAlike 3.0 Unported
IDI_BACK:       "Back Icon", 16x16 Free Application Icons, Aha-Soft, CC Attribution-ShareAlike 3.0 Unported
IDI_BASIC:      "Text Icon", Simply Icons 32px, Carlos Quintana, CC Attribution 3.0 Unported

IDI_CHARMAP:    "Apps Accessories Character Map Icon", Discovery Icon Theme, Hyike Bons, CC Attribution-ShareAlike 3.0 Unported
*/

void CBitmapCache::LoadBitmaps() {
	const TypePair FileTypes[] =
	{
		{ FT_Arbitrary, IDB_BLANK      },
		{ FT_Binary,    IDB_BINARY     },
		{ FT_Code,      IDB_BINARY     },
		{ FT_Script,    IDB_SCRIPT     },
		{ FT_Spool,     IDB_SCRIPT     },
		{ FT_Text,      IDB_TEXT       },
		{ FT_BASIC,     IDB_BINARY     },
		{ FT_Data,      IDB_DATA       },
		{ FT_Graphic,   IDB_GRAPHIC    },
		{ FT_Sound,     IDB_BLANK      },
		{ FT_Pref,      IDB_BLANK      },
		{ FT_App,       IDB_APP        },
		{ FT_DiskImage, IDB_DISKIMAGE  },
		{ FT_TapeImage, IDB_TAPEIMAGE  },
		{ FT_MiscImage, IDB_DISKIMAGE  },
		{ FT_HardImage, IDB_HARDIMAGE  },
		{ FT_CDROM,     IDB_CDROM      },
		{ FT_HardDisc,  IDB_HARDDISC   },
		{ FT_Floppy,    IDB_FLOPPYDISC },
		{ FT_Directory, IDB_FOLDER     },
		{ FT_Archive,   IDB_ARCHIVE    },
		{ FT_System,    IDB_SYSTEM     },
		{ FT_Windows,   IDB_WINDOWS    },
		{ FT_Sound,     IDB_MUSICFILE  },
		{ FT_None,      NULL },
	};

	int	i = 0;

	HBITMAP	hBitmap;

	while ( FileTypes[ i ].Type != FT_None ) {
		hBitmap	= LoadBitmap( hInst, MAKEINTRESOURCE( FileTypes[ i ].IconResource ) );

		DWORD f = GetLastError();

		bitmaps[ (DWORD) FileTypes[ i ].Type ] = hBitmap;

		i++;
	}
}

void CBitmapCache::AddBitmap( const DWORD ID, HBITMAP hBitmap )
{
	bitmaps[ ID ] = hBitmap;
}

HBITMAP	CBitmapCache::GetBitmap(const DWORD ID) {
	if (bitmaps.find(ID) == bitmaps.end())
		return NULL;

	return bitmaps[ID];
}

void CBitmapCache::Unload( void )
{
	std::map<DWORD, HBITMAP>::iterator iBitmap;

	for ( iBitmap = bitmaps.begin(); iBitmap != bitmaps.end(); iBitmap++ )
	{
		DeleteObject( iBitmap->second );
	}

	bitmaps.clear();
}
