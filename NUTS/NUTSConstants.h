#ifndef NUTSCONSTANTS_H
#define NUTSCONSTANTS_H

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

#define ALL_TX ( TXTextTranslator | TXGFXTranslator | TXAUDTranslator )

#define NUTS_SUCCESS       0x00000000
#define ERROR_READONLY     0x00000020
#define ERROR_UNSUPPORTED  0x00000021
#define USE_STANDARD_WND   0x00000022
#define USE_CUSTOM_WND     0x00000023
#define ASCIIFILE_REQUIRED 0x00000024

#define CDF_ENTER_AFTER    0x00000001
#define CDF_INSTALL_OP     0x00000002
#define CDF_MANUAL_OP      0x00000004
#define CDF_MERGE_DIR      0x00000008
#define CDF_RENAME_DIR     0x00000010

#endif