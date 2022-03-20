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

#endif