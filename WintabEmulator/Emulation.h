/*------------------------------------------------------------------------------
WintabEmulator - Emulation.h
Copyright (c) 2013 Carl G. Ritson <critson@perlfu.co.uk>

This file may be freely used, copied, or distributed without compensation 
or licensing restrictions, but is done so without any warranty or implication
of merchantability or fitness for any particular purpose.
------------------------------------------------------------------------------*/

#pragma once

#include "wintab.h"

typedef struct _emu_settings_t {
    BOOL disableFeedback;
    BOOL disableGestures;
    INT shiftX;
    INT shiftY;
    BOOL pressureExpand;
    UINT pressureMin;
    UINT pressureMax;
    BOOL pressureCurve;
    UINT pressurePoint[5];
    UINT pressureValue[5];
} emu_settings_t;

void emuSetModule(HMODULE hModule);
void emuEnableThread(DWORD dwThread);
void emuDisableThread(DWORD dwThread);

void emuInit(BOOL fLogging, BOOL fDebug, emu_settings_t *settings);
void emuShutdown(void);
UINT emuWTInfoA(UINT wCategory, UINT nIndex, LPVOID lpOutput);
UINT emuWTInfoW(UINT wCategory, UINT nIndex, LPVOID lpOutput);
HCTX emuWTOpenA(HWND hWnd, LPLOGCONTEXTA lpLogCtx, BOOL fEnable);
HCTX emuWTOpenW(HWND hWnd, LPLOGCONTEXTW lpLogCtx, BOOL fEnable);
BOOL emuWTClose(HCTX hCtx);
int emuWTPacketsGet(HCTX hCtx, int cMaxPkts, LPVOID lpPkt);
BOOL emuWTPacket(HCTX hCtx, UINT wSerial, LPVOID lpPkt);
BOOL emuWTEnable(HCTX hCtx, BOOL fEnable);
BOOL emuWTOverlap(HCTX hCtx, BOOL fToTop);
BOOL emuWTConfig(HCTX hCtx, HWND hWnd);
BOOL emuWTGetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx);
BOOL emuWTGetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx);
BOOL emuWTSetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx);
BOOL emuWTSetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx);
BOOL emuWTExtGet(HCTX hCtx, UINT wExt, LPVOID lpData);
BOOL emuWTExtSet(HCTX hCtx, UINT wExt, LPVOID lpData);
BOOL emuWTSave(HCTX hCtx, LPVOID lpData);
HCTX emuWTRestore(HWND hWnd, LPVOID lpSaveInfo, BOOL fEnable);
int emuWTPacketsPeek(HCTX hCtx, int cMaxPkt, LPVOID lpPkts);
int emuWTDataGet(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts);
int emuWTDataPeek(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts);
int emuWTQueueSizeGet(HCTX hCtx);
BOOL emuWTQueueSizeSet(HCTX hCtx, int nPkts);
HMGR emuWTMgrOpen(HWND hWnd, UINT wMsgBase);
BOOL emuWTMgrClose(HMGR hMgr);
