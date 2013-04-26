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

#define MAX_STRING_BYTES LC_NAMELEN

static BOOL logging = FALSE;
static BOOL debug = FALSE;

typedef struct _packet_data_t {
} packet_data_t;

static BOOL enabled = FALSE;
static HWND window = NULL;
static packet_data_t *queue;
static UINT q_start, q_end, q_length;
static LOGCONTEXTA default_context;
static LOGCONTEXTA context = NULL;

static void init_context(LOGCONTEXTA *ctx)
{
    strncpy(ctx->lcName, "Windows", LC_NAMELEN);
    ctx->lcOptions  = 0;
    ctx->lcStatus   = 0;
    ctx->lcLocks    = 0;
    ctx->lcMsgBase  = 0x7ff0;
    ctx->lcDevice   = 0;
    ctx->lcPktRate  = 64;
    ctx->lcPktData  = PK_CURSOR | PK_X | PK_Y | PK_NORMAL_PRESSURE;
    ctx->lcPktMode  = 0;
    ctx->lcMoveMask = ctx->lcPktData;
    ctx->lcBtnDnMask= 0xffffffff;
    ctx->lcBtnUpMask= 0xffffffff;
    ctx->lcInOrgX   = 0;
    ctx->lcInOrgY   = 0;
    ctx->lcInOrgZ   = 0;
    ctx->lcInExtX   = 1920;
    ctx->lcInExtY   = 1080;
    ctx->lcInExtZ   = 1024;
    ctx->lcOutOrgX  = ctx->lcInOrgX;
    ctx->lcOutOrgY  = ctx->lcInOrgY;
    ctx->lcOutOrgZ  = ctx->lcInOrgZ;
    ctx->lcOutExtX  = ctx->lcInExtX;
    ctx->lcOutExtY  = ctx->lcInExtY;
    ctx->lcOutExtZ  = ctx->lcInExtZ;
    ctx->lcSensX    = src->lcSensX;
    ctx->lcSensY    = src->lcSensY;
    ctx->lcSensZ    = src->lcSensZ;
    ctx->lcSysMode  = 0; // FIXME
    ctx->lcSysOrgX  = 0; // FIXME
    ctx->lcSysOrgY  = 0; // FIXME
    ctx->lcSysExtX  = 0; // FIXME
    ctx->lcSysExtY  = 0; // FIXME
    ctx->lcSysSensX = 0; // FIXME
    ctx->lcSysSensY = 0; // FIXME
}

static UINT convert_contextw(LOGCONTEXTW *dst, LOGCONTEXTA *src)
{
    if (dst) {
        _snwprintf(dst->lcName, LC_NAMELEN - 1, "%s", src->lcName);
        dst->lcName[LC_NAMELEN - 1] = L'\0';
        dst->lcOptions  = src->lcOptions;
        dst->lcStatus   = src->lcStatus;
        dst->lcLocks    = src->lcLocks;
        dst->lcMsgBase  = src->lcMsgBase;
        dst->lcDevice   = src->lcDevice;
        dst->lcPktRate  = src->lcPktRate;
        dst->lcPktData  = src->lcPktData;
        dst->lcPktMode  = src->lcPktMode;
        dst->lcMoveMask = src->lcMoveMask;
        dst->lcBtnDnMask= src->lcBtnDnMask;
        dst->lcBtnUpMask= src->lcBtnUpMask;
        dst->lcInOrgX   = src->lcInOrgX;
        dst->lcInOrgY   = src->lcInOrgY;
        dst->lcInOrgZ   = src->lcInOrgZ;
        dst->lcInExtX   = src->lcInExtX;
        dst->lcInExtY   = src->lcInExtY;
        dst->lcInExtZ   = src->lcInExtZ;
        dst->lcOutOrgX  = src->lcOutOrgX;
        dst->lcOutOrgY  = src->lcOutOrgY;
        dst->lcOutOrgZ  = src->lcOutOrgZ;
        dst->lcOutExtX  = src->lcOutExtX;
        dst->lcOutExtY  = src->lcOutExtY;
        dst->lcOutExtZ  = src->lcOutExtZ;
        dst->lcSensX    = src->lcSensX;
        dst->lcSensY    = src->lcSensY;
        dst->lcSensZ    = src->lcSensZ;
        dst->lcSysMode  = src->lcSysMode;
        dst->lcSysOrgX  = src->lcSysOrgX;
        dst->lcSysOrgY  = src->lcSysOrgY;
        dst->lcSysExtX  = src->lcSysExtX;
        dst->lcSysExtY  = src->lcSysExtY;
        dst->lcSysSensX = src->lcSysSensX;
        dst->lcSysSensY = src->lcSysSensY;
    }
    return sizeof(LOGCONTEXTW);
}

static UINT convert_contexta(LOGCONTEXTA *dst, LOGCONTEXTW *src)
{
    if (dst) {
        _snprintf(dst->lcName, LC_NAMELEN - 1, "%S", src->lcName);
        dst->lcName[LC_NAMELEN - 1] = '\0';
        dst->lcOptions  = src->lcOptions;
        dst->lcStatus   = src->lcStatus;
        dst->lcLocks    = src->lcLocks;
        dst->lcMsgBase  = src->lcMsgBase;
        dst->lcDevice   = src->lcDevice;
        dst->lcPktRate  = src->lcPktRate;
        dst->lcPktData  = src->lcPktData;
        dst->lcPktMode  = src->lcPktMode;
        dst->lcMoveMask = src->lcMoveMask;
        dst->lcBtnDnMask= src->lcBtnDnMask;
        dst->lcBtnUpMask= src->lcBtnUpMask;
        dst->lcInOrgX   = src->lcInOrgX;
        dst->lcInOrgY   = src->lcInOrgY;
        dst->lcInOrgZ   = src->lcInOrgZ;
        dst->lcInExtX   = src->lcInExtX;
        dst->lcInExtY   = src->lcInExtY;
        dst->lcInExtZ   = src->lcInExtZ;
        dst->lcOutOrgX  = src->lcOutOrgX;
        dst->lcOutOrgY  = src->lcOutOrgY;
        dst->lcOutOrgZ  = src->lcOutOrgZ;
        dst->lcOutExtX  = src->lcOutExtX;
        dst->lcOutExtY  = src->lcOutExtY;
        dst->lcOutExtZ  = src->lcOutExtZ;
        dst->lcSensX    = src->lcSensX;
        dst->lcSensY    = src->lcSensY;
        dst->lcSensZ    = src->lcSensZ;
        dst->lcSysMode  = src->lcSysMode;
        dst->lcSysOrgX  = src->lcSysOrgX;
        dst->lcSysOrgY  = src->lcSysOrgY;
        dst->lcSysExtX  = src->lcSysExtX;
        dst->lcSysExtY  = src->lcSysExtY;
        dst->lcSysSensX = src->lcSysSensX;
        dst->lcSysSensY = src->lcSysSensY;
    }
    return sizeof(LOGCONTEXTA);
}

static UINT copy_contexta(LOGCONTEXTA *dst, LOGCONTEXTA *src)
{
    if (dst) {
        memcpy(dst, src, sizeof(LOGCONTEXTA));
    }
    return sizeof(LOGCONTEXTA);
}

static void allocate_queue(void)
{
    if (queue) {
        free(queue);
    }
    queue = malloc(sizeof(packet_data_t) * q_length);
    q_start = 0;
    q_end = 0;
}

static void set_queue_length(int length)
{
    q_length = length;
    allocate_queue();
}

void emuInit(BOOL fLogging, BOOL fDebug)
{
    logging = fLogging;
    debug = fDebug;
    
    init_context(&default_context);
    enabled = FALSE;
    q_length = 128;
    allocate_queue();
}

static UINT copy_uint(LPVOID lpOutput, UINT nVal)
{
    if (lpOutput) {
        ((UINT *)lpOutput) = nVal;
    }
    return sizeof(UINT);
}

static UINT copy_strw(LPVOID lpOutput, CHAR *str)
{
    int ret = 0;
    if (lpOutput) {
        wchar_t *out = (wchar_t *)lpOutput;
        _snwprintf(out, MAX_STRING_BYTES, _T("%s"), str);
        out[MAX_STRING_BYTES - 1] = L'\0'; 
    }
    return (strlen(ret) * sizeof(wchar_t));
}

static UINT copy_stra(LPVOID lpOutput, CHAR *str)
{
    int ret = 0;
    if (lpOutput) {
        char *out = (char *)lpOutput;
        _snprintf(out, MAX_STRING_BYTES, "%s", str);
        out[MAX_STRING_BYTES - 1] = '\0';
    }
    return (strlen(ret) * sizeof(char));
}

static void enableProcessing(void)
{
}

static void disableProcessing(void)
{
}

static UINT emuWTInfo(BOOL fUnicode, UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
    UINT ret = 0;
    switch (wCategory) {
        case WTI_INTERFACE:
            switch (nIndex) {
                case IFC_WINTABID:
                    ret = copy_uint(lpOutput, 0); // FIXME
                    break;
	            case IFC_SPECVERSION:
                    ret = copy_uint(lpOutput, 0x0104);
                    break;
	            case IFC_IMPLVERSION:
                    ret = copy_uint(lpOutput, 0x01);
                    break;
	            case IFC_NDEVICES:
                    ret = copy_uint(lpOutput, 1);
                    break;
	            case IFC_NCURSORS:
                    ret = copy_uint(lpOutput, 1);
                    break;
	            case IFC_NCONTEXTS:
                    ret = copy_uint(lpOutput, 1);
                    break;
	            case IFC_CTXOPTIONS:
                    ret = copy_uint(lpOutput, 0); // FIXME
                    break;
	            case IFC_CTXSAVESIZE:
                    ret = copy_uint(lpOutput, 0); // FIXME
                    break;
	            case IFC_NEXTENSIONS:
                    ret = copy_uint(lpOutput, 0);
                    break;
	            case IFC_NMANAGERS:
                    ret = copy_uint(lpOutput, 0);
                    break;
            }
            break;
        case WTI_STATUS:
            switch (nIndex) {
                case STA_CONTEXTS:
                case STA_SYSCTXS:
                case STA_PKTRATE:
                case STA_PKTDATA:
                case STA_MANAGERS:
                case STA_SYSTEM:
                case STA_BUTTONUSE: // FIXME
                case STA_SYSBTNUSE:
                    break;
            }
            break;
        case WTI_DEFCONTEXT:
        case WTI_DEFSYSCTX:
            if (nIndex == 0) {
                if (fUnicode) {
                    ret = convert_contextw((LPLOGCONTEXTW)lpOutput, default_context);
                } else {
                    ret = copy_contexta((LPLOGCONTEXTA)lpOutput, default_context);
                }
            }
            break;
        case WTI_DDCTXS:
        case WTI_DSCTXS:
            if (nIndex == 0) {
                if (fUnicode) {
                    ret = convert_contextw((LPLOGCONTEXTW)lpOutput, context ? context : default_context);
                } else {
                    ret = copy_contexta((LPLOGCONTEXTA)lpOutput, context ? context : default_context);
                }
            }
            break;
        case WTI_DEVICES:
            switch (nIndex) {
                case DVC_NAME:
                case DVC_HARDWARE:
                case DVC_NCSRTYPES:
                case DVC_FIRSTCSR:
                case DVC_PKTRATE:
                case DVC_PKTDATA:
                case DVC_PKTMODE:
                case DVC_CSRDATA:
                case DVC_XMARGIN:
                case DVC_YMARGIN:
                case DVC_ZMARGIN:
                case DVC_X:
                case DVC_Y:
                case DVC_Z:
                case DVC_NPRESSURE: // FIXME
                case DVC_TPRESSURE:
                case DVC_ORIENTATION:
                case DVC_ROTATION: /* 1.1 */
                case DVC_PNPID: /* 1.1 */
                    break;
            }
            break;
        case WTI_CURSORS:
            // always first cursor
            switch (nIndex) {
	            case CSR_NAME:
                    if (fUnicode) {
                        ret = copy_strw(lpOutput, "Windows Pointer");
                    } else {
                        ret = copy_stra(lpOutput, "Windows Pointer");
                    }
                    break;
	            case CSR_ACTIVE:
	            case CSR_PKTDATA:
	            case CSR_BUTTONS:
	            case CSR_BUTTONBITS:
            	case CSR_BTNNAMES:
            	case CSR_BUTTONMAP:
            	case CSR_SYSBTNMAP:
            	case CSR_NPBUTTON:
            	case CSR_NPBTNMARKS:
              	case CSR_NPRESPONSE:
            	case CSR_TPBUTTON:
            	case CSR_TPBTNMARKS:
            	case CSR_TPRESPONSE:
            	case CSR_PHYSID: /* 1.1 */
            	case CSR_MODE: /* 1.1 */
            	case CSR_MINPKTDATA: /* 1.1 */
            	case CSR_MINBUTTONS: /* 1.1 */
            	case CSR_CAPABILITIES: /* 1.1 */
            	case CSR_TYPE: /* 1.2 */
                    break;
            }
            break;
        case WTI_EXTENSIONS:
            break;
        default:
            break;
    }
    return ret;
}

UINT emuWTInfoA(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
	return emuWTInfo(FALSE, wCategory, nIndex, lpOutput); 
}

UINT emuWTInfoW(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
	return emuWTInfo(TRUE, wCategory, nIndex, lpOutput); 
}

static HCTX emuWTOpen(BOOL unicode, HWND hWnd, LPVOID lpLogCtx, BOOL fEnable)
{
    if (context || !lpLogCtx)
        return NULL;

    window = hWnd;
    context = (LPLOGCONTEXTA) malloc(sizeof(LOGCONTEXTA));
    if (unicode)
        convert_contexta(context, (LPLOGCONTEXTW) lpLogCtx);
    } else {
        memcpy(context, lpLogCtx, sizeof(LOGCONTEXTA));
    }

    if (fEnable) {
        enableProcessing();
    }

    return (HCTX)context;
}

HCTX emuWTOpenA(HWND hWnd, LPLOGCONTEXTA lpLogCtx, BOOL fEnable)
{
	return emuWTOpen(FALSE, hWnd, lpLogCtx, fEnable);
}

HCTX emuWTOpenW(HWND hWnd, LPLOGCONTEXTW lpLogCtx, BOOL fEnable)
{
	return emuWTOpen(TRUE, hWnd, lpLogCtx, fEnable);
}

BOOL emuWTClose(HCTX hCtx)
{
    if (hCtx && ((LPVOID)hCtx == (LPVOID)context)) {
        if (enabled) {
            disableProcessing();
        }
        free(hCtx);
        context = NULL;
    }
	return TRUE;
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
    if (enabled != fEnable) {
        if (fEnable) {
            enableProcessing();
        } else {
            disableProcessing();
        }
    }
    return TRUE;
}

BOOL emuWTOverlap(HCTX hCtx, BOOL fToTop)
{
	return TRUE;
}

BOOL emuWTConfig(HCTX hCtx, HWND hWnd)
{
	BOOL ret = FALSE;
	// FIXME: implement?
    return ret;
}

BOOL emuWTGetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx)
{
	if (lpLogCtx && hCtx && ((LPVOID)hCtx == (LPVOID)context)) {
        memcpy(lpLogCtx, hCtx, sizeof(LOGCONTEXTA));
        return TRUE;
    } else {
        return FALSE;
    }
}

BOOL emuWTGetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx)
{
	if (lpLogCtx && hCtx && ((LPVOID)hCtx == (LPVOID)context)) {
        convert_contextw(lpLogCtx, context);
        return TRUE;
    } else {
        return FALSE;
    }
}

BOOL emuWTSetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx)
{
	if (lpLogCtx && hCtx && ((LPVOID)hCtx == (LPVOID)context)) {
        memcpy(hCtx, lpLogCtx, sizeof(LOGCONTEXTA));
        return TRUE;
    } else {
        return FALSE;
    }
}

BOOL emuWTSetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx)
{
	if (lpLogCtx && hCtx && ((LPVOID)hCtx == (LPVOID)context)) {
        convert_contexta(context, (LPLOGCONTEXTW)lpLogCtx);
        return TRUE;
    } else {
        return FALSE;
    }
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
    if (hCtx && ((LPVOID)hCtx == (LPVOID)context)) {
        return q_len;
    } else {
        return 0;
    }
}

BOOL emuWTQueueSizeSet(HCTX hCtx, int nPkts)
{
    if (hCtx && ((LPVOID)hCtx == (LPVOID)context) && (nPkts > 0)) {
        set_queue_length(nPkts);
        return TRUE;
    } else {
        return 0;
    }
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
