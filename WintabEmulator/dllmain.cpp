/*------------------------------------------------------------------------------
WintabEmulator - dllmain.cpp
Copyright (c) 2013 Carl G. Ritson <critson@perlfu.co.uk>

This file may be freely used, copied, or distributed without compensation 
or licensing restrictions, but is done so without any warranty or implication
of merchantability or fitness for any particular purpose.
------------------------------------------------------------------------------*/
#include "stdafx.h"
#include "Emulation.h"
#include "logging.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			emuSetModule(hModule);
			break;
		case DLL_THREAD_ATTACH:
            emuEnableThread(GetCurrentThreadId());
			break;
		case DLL_THREAD_DETACH:
            emuDisableThread(GetCurrentThreadId());
			break;
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

