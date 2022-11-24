#pragma once

#include "stdafx.h"

#include "NUTSUI.h"

#include "FileViewer.h"

#include <deque>

#include <winioctl.h>
#include <process.h>

extern AppAction CurrentAction;

unsigned int __stdcall ActionThread(void *param);
int QueueAction(AppAction &Action);
void InitAppActions( void );
void StopAppActions( void );
