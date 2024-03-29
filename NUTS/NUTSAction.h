#ifndef NUTSACTION_H
#define NUTSACTION_H

typedef enum _ActionID {
	AA_NONE        = 0,
	AA_FORMAT      = 1,
	AA_COPY        = 2,
	AA_DELETE      = 3,
	AA_PROPS       = 4,
	AA_SET_PROPS   = 5,
	AA_NEWIMAGE    = 6,
	AA_SET_FSPROPS = 7,
	AA_INSTALL     = 8,
	AA_DELETE_FS   = 9,
	AA_IMAGING     = 10,
} ActionID;

typedef enum _ActionType
{
	ActionDoEnter   = 1,
	ActionDoEnterAs = 2,
	ActionDoBack    = 3,
	ActionDoRoot    = 4,
	ActionDoRefresh = 5,
	ActionDoRename  = 6,
	ActionDoNewDir  = 7,
	ActionDoDirType = 8,
	ActionHookInvoke= 9,
} ActionType;

#endif