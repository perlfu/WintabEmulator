/*------------------------------------------------------------------------------
WintabEmulator - Emulation.cpp
Copyright (c) 2013 Carl G. Ritson <critson@perlfu.co.uk>

This file may be freely used, copied, or distributed without compensation 
or licensing restrictions, but is done so without any warranty or implication
of merchantability or fitness for any particular purpose.
------------------------------------------------------------------------------*/

#include "stdafx.h"
#include <assert.h>

#include "wintab.h"
#include "pktdef.h"

#include "Emulation.h"

static BOOL logging = FALSE;
static BOOL debug = FALSE;

void emuInit(BOOL fLogging, BOOL fDebug)
{
    logging = fLogging;
    debug = fDebug;
}

UINT emuWTInfoA(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
	UINT ret = 0;
	return ret; 
}

UINT emuWTInfoW(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
	UINT ret = 0;
	return ret;
}

HCTX emuWTOpenA(HWND hWnd, LPLOGCONTEXTA lpLogCtx, BOOL fEnable)
{
	HCTX ret = NULL;
	return ret;
}

HCTX emuWTOpenW(HWND hWnd, LPLOGCONTEXTW lpLogCtx, BOOL fEnable)
{
	HCTX ret = NULL;
	return ret;
}

BOOL emuWTClose(HCTX hCtx)
{
	BOOL ret = FALSE;
	return ret;
}

int emuWTPacketsGet(HCTX hCtx, int cMaxPkts, LPVOID lpPkt)
{
	int ret = 0;
    return ret;
}


BOOL emuWTPacket(HCTX hCtx, UINT wSerial, LPVOID lpPkt)
{
	BOOL ret = FALSE;
	return ret;
}


BOOL emuWTEnable(HCTX hCtx, BOOL fEnable)
{
	BOOL ret = FALSE;
    return ret;
}

BOOL emuWTOverlap(HCTX hCtx, BOOL fToTop)
{
	BOOL ret = FALSE;
	return ret;
}

BOOL emuWTConfig(HCTX hCtx, HWND hWnd)
{
	BOOL ret = FALSE;
	return ret;
}


BOOL emuWTGetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx)
{
	BOOL ret = FALSE;
	return ret;
}

BOOL emuWTGetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx)
{
	BOOL ret = FALSE;
	return ret;
}


BOOL emuWTSetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx)
{
	BOOL ret = FALSE;
	return ret;
}

BOOL emuWTSetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx)
{
	BOOL ret = FALSE;
    return ret;
}

BOOL emuWTExtGet(HCTX hCtx, UINT wExt, LPVOID lpData)
{
	BOOL ret = FALSE;
	return ret;
}

BOOL emuWTExtSet(HCTX hCtx, UINT wExt, LPVOID lpData)
{
	BOOL ret = FALSE;
	return ret;
}

BOOL emuWTSave(HCTX hCtx, LPVOID lpData)
{
	BOOL ret = FALSE;
	return ret;
}

HCTX emuWTRestore(HWND hWnd, LPVOID lpSaveInfo, BOOL fEnable)
{
	HCTX ret = NULL;
	return ret;
}

int emuWTPacketsPeek(HCTX hWnd, int cMaxPkt, LPVOID lpPkts)
{
	int ret = 0;
	return ret;
}

int emuWTDataGet(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts)
{
	int ret = 0;
	return ret;
}

int emuWTDataPeek(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts)
{
	int ret = 0;
	return ret;
}

int emuWTQueueSizeGet(HCTX hCtx)
{
	int ret = 0;
	return ret;
}

BOOL emuWTQueueSizeSet(HCTX hCtx, int nPkts)
{
	BOOL ret = FALSE;
	return ret;
}

HMGR emuWTMgrOpen(HWND hWnd, UINT wMsgBase)
{
	HMGR ret = NULL;
    return ret;
}

BOOL emuWTMgrClose(HMGR hMgr)
{
	BOOL ret = FALSE;
	return ret;
}
