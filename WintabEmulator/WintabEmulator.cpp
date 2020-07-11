/*------------------------------------------------------------------------------
WintabEmulator - WintabEmulator.cpp
Copyright (c) 2013, 2020 Carl G. Ritson <critson@perlfu.co.uk>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------*/

#include "stdafx.h"
#include <tchar.h>
#include <assert.h>

#define API __declspec(dllexport) WINAPI
#include "wintab.h"
#include "Emulation.h"

#define INI_FILE                _T("wintab.ini")
#define DEFAULT_LOG_FILE        _T("wintab.txt")
#define DEFAULT_WINTAB_DLL      _T("C:\\WINDOWS\\WINTAB32.DLL")

static BOOL     initialised     = FALSE;

static BOOL     logging         = TRUE;
static BOOL     debug           = TRUE;
static BOOL     useEmulation    = TRUE;
static LPTSTR   logFile         = NULL;
static LPTSTR   wintabDLL       = NULL;
static FILE     *fhLog          = NULL;

static WTPKT    packetData      = 0;
static WTPKT    packetMode      = 0;

#define WTAPI WINAPI

typedef UINT ( WTAPI * WTINFOA ) ( UINT, UINT, LPVOID );
typedef HCTX ( WTAPI * WTOPENA )( HWND, LPLOGCONTEXTA, BOOL );
typedef UINT ( WTAPI * WTINFOW ) ( UINT, UINT, LPVOID );
typedef HCTX ( WTAPI * WTOPENW )( HWND, LPLOGCONTEXTW, BOOL );
typedef BOOL ( WTAPI * WTGETA ) ( HCTX, LPLOGCONTEXTA );
typedef BOOL ( WTAPI * WTSETA ) ( HCTX, LPLOGCONTEXTA );
typedef BOOL ( WTAPI * WTGETW ) ( HCTX, LPLOGCONTEXTW );
typedef BOOL ( WTAPI * WTSETW ) ( HCTX, LPLOGCONTEXTW );
typedef BOOL ( WTAPI * WTCLOSE ) ( HCTX );
typedef BOOL ( WTAPI * WTENABLE ) ( HCTX, BOOL );
typedef BOOL ( WTAPI * WTPACKET ) ( HCTX, UINT, LPVOID );
typedef BOOL ( WTAPI * WTOVERLAP ) ( HCTX, BOOL );
typedef BOOL ( WTAPI * WTSAVE ) ( HCTX, LPVOID );
typedef BOOL ( WTAPI * WTCONFIG ) ( HCTX, HWND );
typedef HCTX ( WTAPI * WTRESTORE ) ( HWND, LPVOID, BOOL );
typedef BOOL ( WTAPI * WTEXTSET ) ( HCTX, UINT, LPVOID );
typedef BOOL ( WTAPI * WTEXTGET ) ( HCTX, UINT, LPVOID );
typedef int ( WTAPI * WTPACKETSPEEK ) ( HCTX, int, LPVOID );
typedef int ( WTAPI * WTQUEUESIZEGET ) ( HCTX );
typedef BOOL ( WTAPI * WTQUEUESIZESET ) ( HCTX, int );
typedef int  ( WTAPI * WTDATAGET ) ( HCTX, UINT, UINT, int, LPVOID, LPINT);
typedef int  ( WTAPI * WTDATAPEEK ) ( HCTX, UINT, UINT, int, LPVOID, LPINT);
typedef int  ( WTAPI * WTPACKETSGET ) (HCTX, int, LPVOID);
typedef HMGR ( WTAPI * WTMGROPEN ) ( HWND, UINT );
typedef BOOL ( WTAPI * WTMGRCLOSE ) ( HMGR );
typedef HCTX ( WTAPI * WTMGRDEFCONTEXT ) ( HMGR, BOOL );
typedef HCTX ( WTAPI * WTMGRDEFCONTEXTEX ) ( HMGR, UINT, BOOL );

static HINSTANCE        hOrigWintab         = NULL;
static WTINFOA          origWTInfoA         = NULL;
static WTINFOW          origWTInfoW         = NULL;
static WTOPENA          origWTOpenA         = NULL;
static WTOPENW          origWTOpenW         = NULL;
static WTGETA           origWTGetA          = NULL;
static WTSETA           origWTSetA          = NULL;
static WTGETW           origWTGetW          = NULL;
static WTSETW           origWTSetW          = NULL;
static WTCLOSE          origWTClose         = NULL;
static WTPACKET         origWTPacket        = NULL;
static WTENABLE         origWTEnable        = NULL;
static WTOVERLAP        origWTOverlap       = NULL;
static WTSAVE           origWTSave          = NULL;
static WTCONFIG         origWTConfig        = NULL;
static WTRESTORE        origWTRestore       = NULL;
static WTEXTSET         origWTExtSet        = NULL;
static WTEXTGET         origWTExtGet        = NULL;
static WTPACKETSPEEK    origWTPacketsPeek   = NULL;
static WTQUEUESIZEGET   origWTQueueSizeGet  = NULL;
static WTQUEUESIZESET   origWTQueueSizeSet  = NULL;
static WTDATAGET        origWTDataGet       = NULL;
static WTDATAPEEK       origWTDataPeek      = NULL;
static WTPACKETSGET     origWTPacketsGet    = NULL;
static WTMGROPEN        origWTMgrOpen       = NULL;
static WTMGRCLOSE       origWTMgrClose      = NULL;
static WTMGRDEFCONTEXT  origWTMgrDefContext = NULL;
static WTMGRDEFCONTEXTEX origWTMgrDefContextEx = NULL;

#define GETPROCADDRESS(type, func) \
	orig##func = (type)GetProcAddress(hOrigWintab, #func);

static BOOL LoadWintab(TCHAR *path)
{
	hOrigWintab = LoadLibrary(path);

	if (!hOrigWintab) {
		return FALSE;
	}

	GETPROCADDRESS(WTOPENA, WTOpenA);
	GETPROCADDRESS(WTINFOA, WTInfoA);
	GETPROCADDRESS(WTGETA, WTGetA);
	GETPROCADDRESS(WTSETA, WTSetA);
	GETPROCADDRESS(WTOPENW, WTOpenW);
	GETPROCADDRESS(WTINFOW, WTInfoW);
	GETPROCADDRESS(WTGETW, WTGetW);
	GETPROCADDRESS(WTSETW, WTSetW);
	GETPROCADDRESS(WTPACKET, WTPacket);
	GETPROCADDRESS(WTCLOSE, WTClose);
	GETPROCADDRESS(WTENABLE, WTEnable);
	GETPROCADDRESS(WTOVERLAP, WTOverlap);
	GETPROCADDRESS(WTSAVE, WTSave);
	GETPROCADDRESS(WTCONFIG, WTConfig);
	GETPROCADDRESS(WTRESTORE, WTRestore);
	GETPROCADDRESS(WTEXTSET, WTExtSet);
	GETPROCADDRESS(WTEXTGET, WTExtGet);
	GETPROCADDRESS(WTPACKETSPEEK, WTPacketsPeek);
	GETPROCADDRESS(WTQUEUESIZEGET, WTQueueSizeGet);
	GETPROCADDRESS(WTQUEUESIZESET, WTQueueSizeSet);
	GETPROCADDRESS(WTDATAGET, WTDataGet);
	GETPROCADDRESS(WTDATAPEEK, WTDataPeek);
	GETPROCADDRESS(WTPACKETSGET, WTPacketsGet);
	GETPROCADDRESS(WTMGROPEN, WTMgrOpen);
	GETPROCADDRESS(WTMGRCLOSE, WTMgrClose);
	GETPROCADDRESS(WTMGRDEFCONTEXT, WTMgrDefContext);
	GETPROCADDRESS(WTMGRDEFCONTEXTEX, WTMgrDefContextEx);

	return TRUE;
}

static void UnloadWintab(void)
{
    origWTInfoA         = NULL;
    origWTInfoW         = NULL;
    origWTOpenA         = NULL;
    origWTOpenW         = NULL;
    origWTGetA          = NULL;
    origWTSetA          = NULL;
    origWTGetW          = NULL;
    origWTSetW          = NULL;
    origWTClose         = NULL;
    origWTPacket        = NULL;
    origWTEnable        = NULL;
    origWTOverlap       = NULL;
    origWTSave          = NULL;
    origWTConfig        = NULL;
    origWTRestore       = NULL;
    origWTExtSet        = NULL;
    origWTExtGet        = NULL;
    origWTPacketsPeek   = NULL;
    origWTQueueSizeGet  = NULL;
    origWTQueueSizeSet  = NULL;
    origWTDataGet       = NULL;
    origWTDataPeek      = NULL;
    origWTPacketsGet    = NULL;
    origWTMgrOpen       = NULL;
    origWTMgrClose      = NULL;
    origWTMgrDefContext = NULL;
    origWTMgrDefContextEx = NULL;
    
    if (hOrigWintab) {
        FreeLibrary(hOrigWintab);
        hOrigWintab = NULL;
    }
}

static BOOL OpenLogFile(void)
{
	_tfopen_s (&fhLog, logFile, _T("a"));
	return (fhLog != NULL ? TRUE : FALSE);
}

void FlushLog(void)
{
    if (fhLog) {
        fflush(fhLog);
    }
}

void LogEntry(char *fmt, ...)
{
    va_list ap;

    if (fhLog) {
        va_start(ap, fmt);
        vfprintf(fhLog, fmt, ap);
        va_end(ap);
    }
}

static void LogLogContextA(LPLOGCONTEXTA lpCtx)
{
    if (!fhLog)
        return;

    if (!lpCtx) {
        fprintf(fhLog, "lpCtx(A) = NULL\n");
    } else {
        fprintf(fhLog, "lpCtx(A) = %p\n"
            " lpOptions = %x\n"
            " lcStatus = %x\n"
            " lcLocks = %d\n"
            " lcMsgBase = %x\n"
            " lcDevice = %x\n"
            " lcPktRate = %x\n"
            " lcPktData = %x\n"
            " lcPktMode = %x\n"
            " lcMoveMask = %x\n"
            " lcBtnDnMask = %x\n"
            " lcBtnUpMask = %x\n"
            " lcInOrgX = %ld\n"
            " lcInOrgY = %ld\n"
            " lcInOrgZ = %ld\n"
            " lcInExtX = %ld\n"
            " lcInExtY = %ld\n"
            " lcInExtZ = %ld\n"
            " lcOutOrgX = %ld\n"
            " lcOutOrgY = %ld\n"
            " lcOutOrgZ = %ld\n"
            " lcOutExtX = %ld\n"
            " lcOutExtY = %ld\n"
            " lcOutExtZ = %ld\n"
            " lcOutExtX = %ld\n"
            " lcOutExtY = %ld\n"
            " lcOutExtZ = %ld\n"
            " lcSensX = %d.%d\n"
            " lcSensY = %d.%d\n"
            " lcSensZ = %d.%d\n"
            " lcSysMode = %d\n"
            " lcSysOrgX = %d\n"
            " lcSysOrgY = %d\n"
            " lcSysExtX = %d\n"
            " lcSysExtY = %d\n"
            " lcSysSensX = %d.%d\n"
            " lcSysSensX = %d.%d\n",
            lpCtx,
            lpCtx->lcOptions,
            lpCtx->lcStatus,
            lpCtx->lcLocks,
            lpCtx->lcMsgBase,
            lpCtx->lcDevice,
            lpCtx->lcPktRate,
            lpCtx->lcPktData,
            lpCtx->lcPktMode,
            lpCtx->lcMoveMask,
            lpCtx->lcBtnDnMask,
            lpCtx->lcBtnUpMask,
            lpCtx->lcInOrgX,
            lpCtx->lcInOrgY,
            lpCtx->lcInOrgZ,
            lpCtx->lcInExtX,
            lpCtx->lcInExtY,
            lpCtx->lcInExtZ,
            lpCtx->lcOutOrgX,
            lpCtx->lcOutOrgY,
            lpCtx->lcOutOrgZ,
            lpCtx->lcOutExtX,
            lpCtx->lcOutExtY,
            lpCtx->lcOutExtZ,
            INT(lpCtx->lcSensX), FRAC(lpCtx->lcSensX),
            INT(lpCtx->lcSensY), FRAC(lpCtx->lcSensY),
            INT(lpCtx->lcSensZ), FRAC(lpCtx->lcSensZ),
            lpCtx->lcSysMode,
            lpCtx->lcSysOrgX,
            lpCtx->lcSysOrgY,
            lpCtx->lcSysExtX,
            lpCtx->lcSysExtY,
            INT(lpCtx->lcSysSensX), FRAC(lpCtx->lcSysSensX),
            INT(lpCtx->lcSysSensY), FRAC(lpCtx->lcSysSensY)
        );
    }
}

static void LogLogContextW(LPLOGCONTEXTW lpCtx)
{
    if (!fhLog)
        return;

    if (!lpCtx) {
        fprintf(fhLog, "lpCtx(W) = NULL\n");
    } else {
        fprintf(fhLog, "lpCtx(W) = %p\n"
            " lpOptions = %x\n"
            " lcStatus = %x\n"
            " lcLocks = %d\n"
            " lcMsgBase = %x\n"
            " lcDevice = %x\n"
            " lcPktRate = %x\n"
            " lcPktData = %x\n"
            " lcPktMode = %x\n"
            " lcMoveMask = %x\n"
            " lcBtnDnMask = %x\n"
            " lcBtnUpMask = %x\n"
            " lcInOrgX = %ld\n"
            " lcInOrgY = %ld\n"
            " lcInOrgZ = %ld\n"
            " lcInExtX = %ld\n"
            " lcInExtY = %ld\n"
            " lcInExtZ = %ld\n"
            " lcOutOrgX = %ld\n"
            " lcOutOrgY = %ld\n"
            " lcOutOrgZ = %ld\n"
            " lcOutExtX = %ld\n"
            " lcOutExtY = %ld\n"
            " lcOutExtZ = %ld\n"
            " lcOutExtX = %ld\n"
            " lcOutExtY = %ld\n"
            " lcOutExtZ = %ld\n"
            " lcSensX = %d.%d\n"
            " lcSensY = %d.%d\n"
            " lcSensZ = %d.%d\n"
            " lcSysMode = %d\n"
            " lcSysOrgX = %d\n"
            " lcSysOrgY = %d\n"
            " lcSysExtX = %d\n"
            " lcSysExtY = %d\n"
            " lcSysSensX = %d.%d\n"
            " lcSysSensX = %d.%d\n",
            lpCtx,
            lpCtx->lcOptions,
            lpCtx->lcStatus,
            lpCtx->lcLocks,
            lpCtx->lcMsgBase,
            lpCtx->lcDevice,
            lpCtx->lcPktRate,
            lpCtx->lcPktData,
            lpCtx->lcPktMode,
            lpCtx->lcMoveMask,
            lpCtx->lcBtnDnMask,
            lpCtx->lcBtnUpMask,
            lpCtx->lcInOrgX,
            lpCtx->lcInOrgY,
            lpCtx->lcInOrgZ,
            lpCtx->lcInExtX,
            lpCtx->lcInExtY,
            lpCtx->lcInExtZ,
            lpCtx->lcOutOrgX,
            lpCtx->lcOutOrgY,
            lpCtx->lcOutOrgZ,
            lpCtx->lcOutExtX,
            lpCtx->lcOutExtY,
            lpCtx->lcOutExtZ,
            INT(lpCtx->lcSensX), FRAC(lpCtx->lcSensX),
            INT(lpCtx->lcSensY), FRAC(lpCtx->lcSensY),
            INT(lpCtx->lcSensZ), FRAC(lpCtx->lcSensZ),
            lpCtx->lcSysMode,
            lpCtx->lcSysOrgX,
            lpCtx->lcSysOrgY,
            lpCtx->lcSysExtX,
            lpCtx->lcSysExtY,
            INT(lpCtx->lcSysSensX), FRAC(lpCtx->lcSysSensX),
            INT(lpCtx->lcSysSensY), FRAC(lpCtx->lcSysSensY)
        );
    }
}

static UINT PacketBytes(UINT data, UINT mode)
{
	UINT n = 0;

	if (data & PK_CONTEXT)
		n += sizeof(HCTX);
	if (data & PK_STATUS)
		n += sizeof(UINT);
	if (data & PK_TIME)
		n += sizeof(DWORD);
	if (data & PK_CHANGED)
		n += sizeof(WTPKT);
	if (data & PK_SERIAL_NUMBER)
		n += sizeof(UINT);
	if (data & PK_CURSOR)
		n += sizeof(UINT);
	if (data & PK_BUTTONS)
		n += sizeof(UINT);
	if (data & PK_X)
		n += sizeof(LONG);
	if (data & PK_Y)
		n += sizeof(LONG);
	if (data & PK_Z)
		n += sizeof(LONG);
	if (data & PK_NORMAL_PRESSURE) {
		if (mode & PK_NORMAL_PRESSURE)
			n += sizeof(int);
		else
			n += sizeof(UINT);
	}
	if (data & PK_TANGENT_PRESSURE) {
		if (mode & PK_TANGENT_PRESSURE)
			n += sizeof(int);
		else
			n += sizeof(UINT);
	}
	if (data & PK_ORIENTATION)
		n += sizeof(ORIENTATION);
	if (data & PK_ROTATION)
		n += sizeof(ROTATION);

	return n;
}

static void LogPacket(UINT data, UINT mode, LPVOID lpData)
{
    BYTE *bData = (BYTE *)lpData;

    if (!fhLog)
        return;

    fprintf(fhLog, "packet = %p (data=%d, mode=%x)\n", lpData, data, mode);

	if (data & PK_CONTEXT) {
		fprintf(fhLog, " PK_CONTEXT = %p\n", *((HCTX *)bData));
        bData += sizeof(HCTX);
    }
	if (data & PK_STATUS) {
		fprintf(fhLog, " PK_STATUS = %x\n", *((UINT *)bData));
        bData += sizeof(UINT);
    }
	if (data & PK_TIME) {
		fprintf(fhLog, " PK_TIME = %d\n", *((DWORD *)bData));
        bData += sizeof(DWORD);
    }
	if (data & PK_CHANGED) {
		fprintf(fhLog, " PK_CHANGED = %x\n", *((WTPKT *)bData));
        bData += sizeof(WTPKT);
    }
	if (data & PK_SERIAL_NUMBER) {
		fprintf(fhLog, " PK_SERIAL_NUMBER = %u\n", *((UINT *)bData));
        bData += sizeof(UINT);
    }
	if (data & PK_CURSOR) {
		fprintf(fhLog, " PK_CURSOR = %u\n", *((UINT *)bData));
        bData += sizeof(UINT);
    }
	if (data & PK_BUTTONS) {
		fprintf(fhLog, " PK_BUTTONS = %x\n", *((UINT *)bData));
        bData += sizeof(UINT);
    }
	if (data & PK_X) {
		fprintf(fhLog, " PK_X = %ld\n", *((LONG *)bData));
        bData += sizeof(LONG);
    }
	if (data & PK_Y) {
		fprintf(fhLog, " PK_Y = %d\n", *((LONG *)bData));
        bData += sizeof(LONG);
    }
	if (data & PK_Z) {
		fprintf(fhLog, " PK_Z = %d\n", *((LONG *)bData));
        bData += sizeof(LONG);
    }
	if (data & PK_NORMAL_PRESSURE) {
		if (mode & PK_NORMAL_PRESSURE) {
		    fprintf(fhLog, " PK_NORMAL_PRESSURE = %d\n", *((int *)bData));
            bData += sizeof(int);
		} else {
		    fprintf(fhLog, " PK_NORMAL_PRESSURE = %d\n", *((UINT *)bData));
            bData += sizeof(UINT);
        }
	}
	if (data & PK_TANGENT_PRESSURE) {
		if (mode & PK_TANGENT_PRESSURE) {
		    fprintf(fhLog, " PK_TANGENT_PRESSURE = %d\n", *((int *)bData));
            bData += sizeof(int);
		} else {
		    fprintf(fhLog, " PK_TANGENT_PRESSURE = %d\n", *((UINT *)bData));
            bData += sizeof(UINT);
        }
	}
	if (data & PK_ORIENTATION) {
		ORIENTATION *o = (ORIENTATION *)bData;
		fprintf(fhLog, " PK_ORIENTATION = %d %d %d\n", o->orAzimuth, o->orAltitude, o->orTwist);
        bData += sizeof(ORIENTATION);
    }
	if (data & PK_ROTATION) {
		ROTATION *r = (ROTATION *)bData;
		fprintf(fhLog, " PK_ROTATION = %d %d %d\n", r->roPitch, r->roRoll, r->roYaw);
        bData += sizeof(ROTATION);
    }
}

static void LogBytes(LPVOID lpData, UINT nBytes)
{
	BYTE *dataPtr = (BYTE *)lpData;

	if (!fhLog)
		return;

	fprintf(fhLog, "data =");
	if (dataPtr) {
		UINT n;
		for (n = 0; n < nBytes; ++n) {
			fprintf(fhLog, " %02x", dataPtr[n]);
		}
	} else {
		fprintf(fhLog, " NULL");
	}
	fprintf(fhLog, "\n");
}

static void getINIPath(TCHAR *out, UINT length)
{
    TCHAR pwd[MAX_PATH];
    
    GetCurrentDirectory(MAX_PATH, pwd);
    _sntprintf_s(out, length, _TRUNCATE, _T("%s\\%s"), pwd, INI_FILE);
}

static void SetDefaults(emu_settings_t *settings)
{
    settings->disableFeedback       = TRUE;
    settings->disableGestures       = TRUE;
    settings->shiftX                = 0;
    settings->shiftY                = 0;
    settings->pressureExpand        = TRUE;
    settings->pressureMin           = 0;
    settings->pressureMax           = 1023;
    settings->pressureCurve         = FALSE;
    settings->pressurePoint[0]      = 0;
    settings->pressurePoint[1]      = 253;
    settings->pressurePoint[2]      = 511;
    settings->pressurePoint[3]      = 767;
    settings->pressurePoint[4]      = 1023;
    settings->pressureValue[0]      = 0;
    settings->pressureValue[1]      = 253;
    settings->pressureValue[2]      = 511;
    settings->pressureValue[3]      = 767;
    settings->pressureValue[4]      = 1023;
}

static void LoadSettings(emu_settings_t *settings)
{
    const UINT stringLength = MAX_PATH;
    TCHAR iniPath[MAX_PATH];
    DWORD dwRet;
    UINT nRet;

    getINIPath(iniPath, MAX_PATH);
    
    nRet = GetPrivateProfileInt(
        _T("Logging"),
        _T("Mode"),
        logging ? 1 : 0,
        iniPath
    );
    logging = (nRet != 0);

    logFile = (TCHAR *) malloc(sizeof(TCHAR) * stringLength);
    dwRet = GetPrivateProfileString(
        _T("Logging"),
        _T("LogFile"),
        DEFAULT_LOG_FILE,
        logFile,
        stringLength,
        iniPath
    );
    
    nRet = GetPrivateProfileInt(
        _T("Emulation"),
        _T("Mode"),
        useEmulation ? 1 : 0,
        iniPath
    );
    useEmulation = (nRet != 0);
    
    nRet = GetPrivateProfileInt(
        _T("Emulation"),
        _T("Debug"),
        debug ? 1 : 0,
        iniPath
    );
    debug = (nRet != 0);

    wintabDLL = (TCHAR *) malloc(sizeof(TCHAR) * stringLength);
    dwRet = GetPrivateProfileString(
        _T("Emulation"),
        _T("WintabDLL"),
        DEFAULT_LOG_FILE,
        wintabDLL,
        stringLength,
        iniPath
    );
    
    nRet = GetPrivateProfileInt(
        _T("Emulation"),
        _T("DisableFeedback"),
        settings->disableFeedback ? 1 : 0,
        iniPath
    );
    settings->disableFeedback = (nRet != 0);
    
    nRet = GetPrivateProfileInt(
        _T("Emulation"),
        _T("DisableGestures"),
        settings->disableGestures ? 1 : 0,
        iniPath
    );
    settings->disableGestures = (nRet != 0);
    
    // Adjustment of positions
    settings->shiftX = GetPrivateProfileInt(
        _T("Adjust"), _T("ShiftX"),
        settings->shiftX,
        iniPath
    );
    settings->shiftY = GetPrivateProfileInt(
        _T("Adjust"), _T("ShiftY"),
        settings->shiftY,
        iniPath
    );
    
    // Pressure clamping
    nRet = GetPrivateProfileInt(
        _T("Adjust"),
        _T("PressureExpand"),
        settings->pressureExpand ? 1 : 0,
        iniPath
    );
    settings->pressureExpand = (nRet != 0);
    
    settings->pressureMin = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureMin"),
        settings->pressureMin,
        iniPath
    );
    settings->pressureMax = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureMax"),
        settings->pressureMax,
        iniPath
    );

    // Pressure curve
    nRet = GetPrivateProfileInt(
        _T("Adjust"),
        _T("PressureCurve"),
        settings->pressureCurve ? 1 : 0,
        iniPath
    );
    settings->pressureCurve = (nRet != 0);

    settings->pressurePoint[0] = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureCurveP0"),
        settings->pressurePoint[0],
        iniPath
    );
    settings->pressureValue[0] = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureCurveP0V"),
        settings->pressureValue[0],
        iniPath
    );
    settings->pressurePoint[1] = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureCurveP1"),
        settings->pressurePoint[1],
        iniPath
    );
    settings->pressureValue[1] = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureCurveP1V"),
        settings->pressureValue[1],
        iniPath
    );
    settings->pressurePoint[2] = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureCurveP2"),
        settings->pressurePoint[2],
        iniPath
    );
    settings->pressureValue[2] = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureCurveP2V"),
        settings->pressureValue[2],
        iniPath
    );
    settings->pressurePoint[3] = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureCurveP3"),
        settings->pressurePoint[3],
        iniPath
    );
    settings->pressureValue[3] = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureCurveP3V"),
        settings->pressureValue[3],
        iniPath
    );
    settings->pressurePoint[4] = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureCurveP4"),
        settings->pressurePoint[4],
        iniPath
    );
    settings->pressureValue[4] = GetPrivateProfileInt(
        _T("Adjust"), _T("PressureCurveP4V"),
        settings->pressureValue[4],
        iniPath
    );
}

static void Init(void)
{
    emu_settings_t settings;

    if (initialised)
        return;

    SetDefaults(&settings);
    LoadSettings(&settings);

    if (logging) {
		logging = OpenLogFile();
    }

    LogEntry(
        "init, logging = %d, debug = %d, useEmulation = %d\n", 
        logging, debug, useEmulation
    );

    if (useEmulation) {
        emuInit(logging, debug, &settings);
    } else {
        LoadWintab(wintabDLL);
    }

    initialised = TRUE;
}

// FIXME arrange for this to be called?
static void Shutdown(void)
{
    if (logging)
        LogEntry("shutdown started\n");
    
    if (useEmulation) {
        emuShutdown();
    } else {
        UnloadWintab();
    }
    
    if (logging)
        LogEntry("shutdown finished\n");
    
    if (fhLog) {
        fclose(fhLog);
        fhLog = NULL;
    }
    if (logFile) {
        free(logFile);
        logFile = NULL;
    }
    if (wintabDLL) {
        free(wintabDLL);
        wintabDLL = NULL;
    }
}

UINT API WTInfoA(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
	UINT ret = 0;
    
    Init();

    if (useEmulation) {
        ret = emuWTInfoA(wCategory, nIndex, lpOutput);
    } else if (hOrigWintab) {
        ret = origWTInfoA(wCategory, nIndex, lpOutput);
    }
    
    if (logging) {
	    LogEntry("WTInfoA(%x, %x, %p) = %d\n", wCategory, nIndex, lpOutput, ret);
	    LogBytes(lpOutput, ret);
    }

	return ret; 
}

UINT API WTInfoW(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
	UINT ret = 0;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTInfoW(wCategory, nIndex, lpOutput);
    } else if (hOrigWintab) {
        ret = origWTInfoW(wCategory, nIndex, lpOutput);
    }
    if (logging) {
        LogEntry("WTInfoW(%x, %x, %p) = %d\n", wCategory, nIndex, lpOutput, ret);
	    LogBytes(lpOutput, ret);
    }

	return ret;
}

HCTX API WTOpenA(HWND hWnd, LPLOGCONTEXTA lpLogCtx, BOOL fEnable)
{
	HCTX ret = NULL;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTOpenA(hWnd, lpLogCtx, fEnable);
    } else if (hOrigWintab) {
        ret = origWTOpenA(hWnd, lpLogCtx, fEnable);
    }
        
    if (lpLogCtx) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }

	if (logging) {
	    LogEntry("WTOpenA(%x, %p, %d) = %x\n", hWnd, lpLogCtx, fEnable, ret);
	    LogBytes(lpLogCtx, sizeof(*lpLogCtx));
		LogLogContextA(lpLogCtx);
	}

	return ret;
}

HCTX API WTOpenW(HWND hWnd, LPLOGCONTEXTW lpLogCtx, BOOL fEnable)
{
	HCTX ret = NULL;
    
    Init();

    if (useEmulation) {
	    ret = emuWTOpenW(hWnd, lpLogCtx, fEnable);
    } else if (hOrigWintab) {
	    ret = origWTOpenW(hWnd, lpLogCtx, fEnable);
    }
    
    if (lpLogCtx) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }
    
    if (logging) {
	    LogEntry("WTOpenW(%x, %p, %d) = %x\n", hWnd, lpLogCtx, fEnable, ret);
	    LogBytes(lpLogCtx, sizeof(*lpLogCtx));
		LogLogContextW(lpLogCtx);
    }

	return ret;
}

BOOL API WTClose(HCTX hCtx)
{
	BOOL ret = FALSE;
    
    Init();

    if (useEmulation) {
        ret = emuWTClose(hCtx);
    } else if (hOrigWintab) {
        ret = origWTClose(hCtx);
    }

    if (logging) {
	    LogEntry("WTClose(%x) = %d\n", hCtx, ret);
    }

	return ret;
}

int API WTPacketsGet(HCTX hCtx, int cMaxPkts, LPVOID lpPkt)
{
	int ret = 0;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTPacketsGet(hCtx, cMaxPkts, lpPkt);
    } else if (hOrigWintab) {
        ret = origWTPacketsGet(hCtx, cMaxPkts, lpPkt);
    }

    if (logging) {
	    LogEntry("WTPacketGet(%x, %d, %p) = %d\n", hCtx, cMaxPkts, lpPkt, ret);
	    if (ret > 0)
		    LogBytes(lpPkt, PacketBytes(packetData, packetMode) * ret);
    }
	
    return ret;
}


BOOL API WTPacket(HCTX hCtx, UINT wSerial, LPVOID lpPkt)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTPacket(hCtx, wSerial, lpPkt);
    } else if (hOrigWintab) {
        ret = origWTPacket(hCtx, wSerial, lpPkt);
    }

    if (logging) {
        LogEntry("WTPacket(%x, %x, %p) = %d\n", hCtx, wSerial, lpPkt, ret);
        if (ret) {
            LogBytes(lpPkt, PacketBytes(packetData, packetMode));
        }
    }

	return ret;
}


BOOL API WTEnable(HCTX hCtx, BOOL fEnable)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
	    ret = emuWTEnable(hCtx, fEnable);
    } else if (hOrigWintab) {
	    ret = origWTEnable(hCtx, fEnable);
    }

    if (logging) {
	    LogEntry("WTEnable(%x, %d) = %d\n", hCtx, fEnable, ret);
    }

    return ret;
}

BOOL API WTOverlap(HCTX hCtx, BOOL fToTop)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTOverlap(hCtx, fToTop);
    } else if (hOrigWintab) {
        ret = origWTOverlap(hCtx, fToTop);
    }
    
    if (logging) {
	    LogEntry("WTOverlap(%x, %d) = %d\n", hCtx, fToTop, ret);
    }

	return ret;
}

BOOL API WTConfig(HCTX hCtx, HWND hWnd)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTConfig(hCtx, hWnd);
    } else if (hOrigWintab) {
        ret = origWTConfig(hCtx, hWnd);
    }

    if (logging) {
	    LogEntry("WTConfig(%x, %x) = %d\n", hCtx, hWnd, ret);
    }

	return ret;
}


BOOL API WTGetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTGetA(hCtx, lpLogCtx);
    } else if (hOrigWintab) {
        ret = origWTGetA(hCtx, lpLogCtx);
	}
    
    if (lpLogCtx && ret) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }

    if (logging) {
        LogEntry("WTGetA(%x, %p) = %d\n", hCtx, lpLogCtx, ret);
        LogBytes(lpLogCtx, sizeof(*lpLogCtx));
        LogLogContextA(lpLogCtx);
    }

	return ret;
}

BOOL API WTGetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTGetW(hCtx, lpLogCtx);
    } else if (hOrigWintab) {
        ret = origWTGetW(hCtx, lpLogCtx);
    }
    
    if (lpLogCtx && ret) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }

    if (logging) {
	    LogEntry("WTGetW(%x, %p) = %d\n", hCtx, lpLogCtx, ret);
	    LogBytes(lpLogCtx, sizeof(*lpLogCtx));
		LogLogContextW(lpLogCtx);
    }

	return ret;
}


BOOL API WTSetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTSetA(hCtx, lpLogCtx);
    } else if (hOrigWintab) {
        ret = origWTSetA(hCtx, lpLogCtx);
    }
    
    if (lpLogCtx && ret) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }
	
    if (logging) {
        LogEntry("WTSetA(%x, %p) = %d\n", hCtx, lpLogCtx, ret);
	    LogBytes(lpLogCtx, sizeof(*lpLogCtx));
		LogLogContextA(lpLogCtx);
    }

	return ret;
}

BOOL API WTSetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTSetW(hCtx, lpLogCtx);
    } else if (hOrigWintab) {
        ret = origWTSetW(hCtx, lpLogCtx);
    }
    
    if (lpLogCtx && ret) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }
	
    if (logging) {
        LogEntry("WTSetW(%x, %p) = %d\n", hCtx, lpLogCtx, ret);
	    LogBytes(lpLogCtx, sizeof(*lpLogCtx));
		LogLogContextW(lpLogCtx);
    }
	
    return ret;
}

BOOL API WTExtGet(HCTX hCtx, UINT wExt, LPVOID lpData)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTExtGet(hCtx, wExt, lpData);
    } else if (hOrigWintab) {
        ret = origWTExtGet(hCtx, wExt, lpData);
    }

    if (logging) {
        LogEntry("WTExtGet(%x, %x, %p) = %d\n", hCtx, wExt, lpData, ret);
        //LogBytes(lpLogCtx, sizeof(*lpLogCtx));
    }

	return ret;
}

BOOL API WTExtSet(HCTX hCtx, UINT wExt, LPVOID lpData)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTExtSet(hCtx, wExt, lpData);
    } else if (hOrigWintab) {
        ret = origWTExtSet(hCtx, wExt, lpData);
    }

    if (logging) {
	    LogEntry("WTExtSet(%x, %x, %p) = %d\n", hCtx, wExt, lpData, ret);
	    //LogBytes(lpLogCtx, sizeof(*lpLogCtx));
    }

	return ret;
}

BOOL API WTSave(HCTX hCtx, LPVOID lpData)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTSave(hCtx, lpData);
    } else if (hOrigWintab) {
        ret = origWTSave(hCtx, lpData);
    }

    if (logging) {
	    LogEntry("WTSave(%x, %p) = %d\n", hCtx, lpData, ret);
	    //LogBytes(lpLogCtx, sizeof(*lpLogCtx));
    }

	return ret;
}

HCTX API WTRestore(HWND hWnd, LPVOID lpSaveInfo, BOOL fEnable)
{
	HCTX ret = NULL;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTRestore(hWnd, lpSaveInfo, fEnable);
    } else if (hOrigWintab) {
        ret = origWTRestore(hWnd, lpSaveInfo, fEnable);
    }

    if (logging) {
	    LogEntry("WTRestore(%x, %p, %d) = %x\n", hWnd, lpSaveInfo, fEnable, ret);
	    //LogBytes(lpLogCtx, sizeof(*lpLogCtx));
    }

	return ret;
}

int API WTPacketsPeek(HCTX hWnd, int cMaxPkt, LPVOID lpPkts)
{
	int ret = 0;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTPacketsPeek(hWnd, cMaxPkt, lpPkts);
    } else if (hOrigWintab) {
        ret = origWTPacketsPeek(hWnd, cMaxPkt, lpPkts);
    }

    if (logging) {
        LogEntry("WTPacketsPeek(%x, %d, %p) = %d\n", hWnd, cMaxPkt, lpPkts, ret);
        if (ret > 0)
            LogBytes(lpPkts, PacketBytes(packetData, packetMode) * ret);
    }

	return ret;
}

int API WTDataGet(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts)
{
	int ret = 0;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTDataGet(hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts);
    } else if (hOrigWintab) {
        ret = origWTDataGet(hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts);
    }
    
    if (logging) {
        LogEntry("WTDataGet(%x, %x, %x, %d, %p, %p) = %d\n", hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts, ret);
        if (lpNPkts)
            LogBytes(lpPkts, PacketBytes(packetData, packetMode) * (*lpNPkts));
    }

	return ret;
}

int API WTDataPeek(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts)
{
	int ret = 0;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTDataPeek(hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts);
    } else if (hOrigWintab) {
        ret = origWTDataPeek(hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts);
    }

    if (logging) {
        LogEntry("WTDataPeek(%x, %x, %x, %d, %p, %p) = %d\n", hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts, ret);
        if (lpNPkts)
            LogBytes(lpPkts, PacketBytes(packetData, packetMode) * (*lpNPkts));
    }

	return ret;
}

int API WTQueueSizeGet(HCTX hCtx)
{
	int ret = 0;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTQueueSizeGet(hCtx);
    } else if (hOrigWintab) {
        ret = origWTQueueSizeGet(hCtx);
    }

    if (logging) {
	    LogEntry("WTQueueSizeGet(%x) = %d\n", hCtx, ret);
    }

	return ret;
}

BOOL API WTQueueSizeSet(HCTX hCtx, int nPkts)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTQueueSizeSet(hCtx, nPkts);
    } else if (hOrigWintab) {
        ret = origWTQueueSizeSet(hCtx, nPkts);
    }
    
    if (logging) {
	    LogEntry("WTQueueSizeSet(%x, %d) = %d\n", hCtx, nPkts, ret);
    }

	return ret;
}

HMGR API WTMgrOpen(HWND hWnd, UINT wMsgBase)
{
	HMGR ret = NULL;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTMgrOpen(hWnd, wMsgBase);
    } else if (hOrigWintab) {
        ret = origWTMgrOpen(hWnd, wMsgBase);
    }
	
    if (logging) {
        LogEntry("WTMgrOpen(%x, %x) = %x\n", hWnd, wMsgBase, ret);
	}

    return ret;
}

BOOL API WTMgrClose(HMGR hMgr)
{
	BOOL ret = FALSE;
    
    Init();
    
    if (useEmulation) {
	    ret = emuWTMgrClose(hMgr);
    } else if (hOrigWintab) {
	    ret = origWTMgrClose(hMgr);
    }
	
    if (logging) {
        LogEntry("WTMgrClose(%x) = %d\n", hMgr, ret);
    }

	return ret;
}

BOOL API WTMgrContextEnum(HMGR hMgr, WTENUMPROC lpEnumFunc, LPARAM lParam)
{
    // XXX: unsupported
    Init();
    if (logging)
        LogEntry("Unsupported WTMgrContextEnum(%x, %p, %x)\n", hMgr, lpEnumFunc, lParam);
	return FALSE;
}

HWND API WTMgrContextOwner(HMGR hMgr, HCTX hCtx)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrContextOwner(%x, %x)\n", hMgr, hCtx);
	return NULL;
}

HCTX API WTMgrDefContext(HMGR hMgr, BOOL fSystem)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrDefContext(%x, %d)\n", hMgr, fSystem);
	return NULL;
}

HCTX API WTMgrDefContextEx(HMGR hMgr, UINT wDevice, BOOL fSystem)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrDefContextEx(%x, %x, %d)\n", hMgr, wDevice, fSystem);
	return NULL;
}

UINT API WTMgrDeviceConfig(HMGR hMgr, UINT wDevice, HWND hWnd)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrDeviceConfig(%x, %x, %x)\n", hMgr, wDevice, hWnd);
	return 0;
}

BOOL API WTMgrExt(HMGR hMgr, UINT wParam1, LPVOID lpParam1)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrExt(%x, %p, %x)\n", hMgr, wParam1, lpParam1);
	return FALSE;
}

BOOL API WTMgrCsrEnable(HMGR hMgr, UINT wParam1, BOOL fParam1)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrCsrEnable(%x, %x, %d)\n", hMgr, wParam1, fParam1);
	return FALSE;
}

BOOL API WTMgrCsrButtonMap(HMGR hMgr, UINT wCursor, LPBYTE lpLogBtns, LPBYTE lpSysBtns)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrCsrButtonMap(%x, %x, %p, %x)\n", hMgr, wCursor, lpLogBtns, lpSysBtns);
	return FALSE;
}

BOOL API WTMgrCsrPressureBtnMarks(HMGR hMgr, UINT wCsr, DWORD lpNMarks, DWORD lpTMarks)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrCsrPressureBtnMarks(%x, %x, %x, %x)\n", hMgr, wCsr, lpNMarks, lpTMarks);
	return FALSE;
}

BOOL API WTMgrCsrPressureResponse(HMGR hMgr, UINT wCsr, UINT FAR *lpNResp, UINT FAR *lpTResp)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrCsrPressureResponse(%x, %x, %p, %p)\n", hMgr, wCsr, lpNResp, lpTResp);
	return FALSE;
}

BOOL API WTMgrCsrExt(HMGR hMgr, UINT wCsr, UINT wParam1, LPVOID lpParam1)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrCsrExt(%x, %x, %x, %p)\n", hMgr, wCsr, wParam1, lpParam1);
	return FALSE;
}

BOOL API WTQueuePacketsEx(HCTX hCtx, UINT FAR *lpParam1, UINT FAR *lpParam2)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTQueuePacketsEx(%x, %p, %p)\n", hCtx, lpParam1, lpParam2);
	return FALSE;
}

BOOL API WTMgrConfigReplaceExA(HMGR hMgr, BOOL fParam1, LPSTR lpParam1, LPSTR lpParam2)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrConfigReplaceExA(%x, %d, %p, %p)\n", hMgr, fParam1, lpParam1, lpParam2);
	return FALSE;
}

BOOL API WTMgrConfigReplaceExW(HMGR hMgr, BOOL fParam1, LPWSTR lpParam1, LPSTR lpParam2)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrConfigReplaceExW(%x, %d, %p, %p)\n", hMgr, fParam1, lpParam1, lpParam2);
	return FALSE;
}

HWTHOOK API WTMgrPacketHookExA(HMGR hMgr, int cParam1, LPSTR lpParam1, LPSTR lpParam2)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrPacketHookExA(%x, %d, %p, %p)\n", hMgr, cParam1, lpParam1, lpParam2);
	return NULL;
}

HWTHOOK API WTMgrPacketHookExW(HMGR hMgr, int cParam1, LPWSTR lpParam1, LPSTR lpParam2)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrPacketHookExW(%x, %d, %p, %p)\n", hMgr, cParam1, lpParam1, lpParam2);
	return NULL;
}

BOOL API WTMgrPacketUnhook(HWTHOOK hWTHook)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrPacketUnhook(%x)\n", hWTHook);
	return FALSE;
}

LRESULT API WTMgrPacketHookNext(HWTHOOK hWTHook, int cParam1, WPARAM wParam1, LPARAM lpParam1)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrPacketHookNext(%x, %d, %x, %x)\n", hWTHook, cParam1, wParam1, lpParam1);
	return NULL;
}

BOOL API WTMgrCsrPressureBtnMarksEx(HMGR hMgr, UINT wCsr, UINT FAR *lpParam1, UINT FAR *lpParam2)
{
    // XXX: unsupported
    Init();
    if (logging)
	    LogEntry("Unsupported WTMgrCsrPressureBtnMarksEx(%x, %x, %p, %p)\n", hMgr, wCsr, lpParam1, lpParam2);
	return FALSE;
}

