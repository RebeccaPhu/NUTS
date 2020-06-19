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
IDB_DISKIMAGE:  "Floppy Large Icon", iIcons, Treetog ArtWork, Free for personal use
IDB_FLOPPYDISC: "Floppy Drive 5 Icon", Junior Icons, Treetog ArtWork, Free for personal use
IDB_FOLDER:     "Folder Gray Icon", Aqua Candy Revolution Icons, McDo Design, Free for non-commerical use.
IDB_GRAPHIC:    "Graphic File Icon", 32x32 Free Design Icons, Aha-Soft, CC Attribution 3.0 US
IDB_HARDDISC:   "Hard Disk Icon", Influens Icons, Mat-u, CC Attribution-NonCommercial-NoDerivs 3.0 Unported
IDB_SCRIPT:     "File Script Icon", Junior Icons, Treetog ArtWork, Free for personal use
IDB_TEXT:       "Text Icon", Crystal Office Icon Set, MediaJon (www.iconshots.com), Linkware
IDB_TAPEIMAGE:  "Cassette Graphite Icon", Cassette Icons, iTweek, Free for personal use.
IDB_ARCHIVE:    "Light Brown, ZIP, Archive", Cats Icons 2, McDo Design, Free for non-commerical use.
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
		{ FT_CDROM,     IDB_CDROM      },
		{ FT_HardDisc,  IDB_HARDDISC   },
		{ FT_Floppy,    IDB_FLOPPYDISC },
		{ FT_Directory, IDB_FOLDER     },
		{ FT_Archive,   IDB_ARCHIVE    },
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


