#include "stdafx.h"

#include "FormatWizard.h"
#include "FileOps.h"
#include "PropsPage.h"
#include "ImageWizard.h"
#include "AppAction.h"

std::deque<AppAction> AppActions; // Actions to perform.
CRITICAL_SECTION      ActionLock; // Lock around access to action list.

AppAction CurrentAction;          // Current action definition.

HANDLE hActionThread;             // Thread for performing actions
HANDLE hShutdown;                 // Event to shutdown threads
HANDLE hActionTrigger;            // Event to cause action processing to occur.

unsigned int __stdcall ActionThread(void *param) {

	HANDLE	waits[2]	= {hShutdown, hActionTrigger};

	AppAction	Action;

	while (1) {
		WaitForMultipleObjects(2, waits, FALSE, 10000);

		if (WaitForSingleObject(hShutdown, 0) == WAIT_OBJECT_0) {
			return 0;
		}

		while (1) {
			EnterCriticalSection(&ActionLock);

			if (AppActions.size() == 0) {
				LeaveCriticalSection(&ActionLock);

				break;
			}

			Action = AppActions.front();

			char err[256];
			sprintf_s(err, 256, "Found action %d\n", Action.Action);
			OutputDebugStringA(err);

			AppActions.pop_front();

			LeaveCriticalSection(&ActionLock);

			switch (Action.Action) {
				case AA_FORMAT:
					Format_Handler(Action);

					break;

				case AA_COPY:
				case AA_DELETE:
				case AA_SET_PROPS:
					FileOP_Handler(Action);

					break;

				case AA_PROPS:
					PropsPage_Handler( Action );

					break;

				case AA_NEWIMAGE:
					ImageWiz_Handler( Action );

				default:
					break;
			}
		}
	}
}

int QueueAction(AppAction &Action) {
	EnterCriticalSection(&ActionLock);

	AppActions.push_back(Action);

	LeaveCriticalSection(&ActionLock);

	SetEvent(hActionTrigger);

	return Action.Action;
}

void InitAppActions( void )
{
	unsigned int	dwthreadid;

	hShutdown		= CreateEvent(NULL, TRUE, FALSE, NULL);
	hActionTrigger	= CreateEvent(NULL, FALSE, FALSE, NULL);

	AppActions.clear();

	InitializeCriticalSection(&ActionLock);

	hActionThread	= (HANDLE) _beginthreadex(NULL, NULL, ActionThread, NULL, NULL, &dwthreadid);
}

void StopAppActions( void )
{
	SetEvent(hShutdown);

	if (WaitForSingleObject(hActionThread, 10000) == WAIT_TIMEOUT) {
		TerminateThread(hActionThread, 500);
	}

	CloseHandle(hActionThread);
	CloseHandle(hShutdown);
	CloseHandle(hActionTrigger);

	DeleteCriticalSection(&ActionLock);
}
