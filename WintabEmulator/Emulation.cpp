/*------------------------------------------------------------------------------
WintabEmulator - Emulation.cpp
Copyright (c) 2013 Carl G. Ritson <critson@perlfu.co.uk>

This file may be freely used, copied, or distributed without compensation 
or licensing restrictions, but is done so without any warranty or implication
of merchantability or fitness for any particular purpose.
------------------------------------------------------------------------------*/

#include "stdafx.h"
#include <tlhelp32.h>
#include <assert.h>

#include "wintab.h"
#include "pktdef.h"

#include "Emulation.h"
#include "logging.h"


#define MI_WP_SIGNATURE 0xFF515700
#define MI_SIGNATURE_MASK 0xFFFFFF00
#define IsPenEvent(dw) (((dw) & MI_SIGNATURE_MASK) == MI_WP_SIGNATURE)


#define MAX_STRING_BYTES LC_NAMELEN
#define MAX_HOOKS 16
#define MAX_POINTERS 4
#define TIME_CLOSE_MS 2

#define MOUSE_POINTER_ID 1


static BOOL logging = FALSE;
static BOOL debug = TRUE;

typedef struct _packet_data_t {
    UINT serial;
    UINT contact;
    LONG time;
	DWORD x, y;
    DWORD buttons;
    UINT pressure;
    UINT pad[9];
} packet_data_t;

typedef struct _hook_t {
    HHOOK handle;
    DWORD thread;
} hook_t;


static BOOL enabled = FALSE;
static BOOL processing = FALSE;
static BOOL overrideFeedback = TRUE;
static HMODULE module = NULL;

static HWND window = NULL;
static hook_t hooks[MAX_HOOKS];

static UINT32 pointers[MAX_POINTERS];
static UINT n_pointers = 0;

static packet_data_t *queue = NULL;
static UINT next_serial = 1;
static UINT q_start, q_end, q_length;
static CRITICAL_SECTION q_lock; 
static UINT screen_height = 0;
static UINT screen_width = 0;

static LOGCONTEXTA default_context;
static LPLOGCONTEXTA context = NULL;


static void init_context(LOGCONTEXTA *ctx)
{
    strncpy_s(ctx->lcName, "Windows", LC_NAMELEN);
    ctx->lcOptions  = 0;
    ctx->lcStatus   = CXO_SYSTEM | CXO_PEN;
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
    ctx->lcInExtX   = screen_width;
    ctx->lcInExtY   = screen_height;
    ctx->lcInExtZ   = 0;
    ctx->lcOutOrgX  = ctx->lcInOrgX;
    ctx->lcOutOrgY  = ctx->lcInOrgY;
    ctx->lcOutOrgZ  = ctx->lcInOrgZ;
    ctx->lcOutExtX  = ctx->lcInExtX;
    ctx->lcOutExtY  = ctx->lcInExtY;
    ctx->lcOutExtZ  = ctx->lcInExtZ;
    ctx->lcSensX    = 0; // FIXME
    ctx->lcSensY    = 0; // FIXME
    ctx->lcSensZ    = 0; // FIXME
    ctx->lcSysMode  = 0; // FIXME
    ctx->lcSysOrgX  = 0; // FIXME
    ctx->lcSysOrgY  = 0; // FIXME
    ctx->lcSysExtX  = 0; // FIXME
    ctx->lcSysExtY  = 0; // FIXME
    ctx->lcSysSensX = 0; // FIXME
    ctx->lcSysSensY = 0; // FIXME
}

static BOOL update_screen_metrics(LOGCONTEXTA *ctx)
{
    // FIXME: need to think about how to handle multiple displays...
    BOOL changed = FALSE;
    int width = screen_width = GetSystemMetrics(SM_CXSCREEN);
    int height = screen_height = GetSystemMetrics(SM_CYSCREEN);

    if (logging)
        LogEntry("screen metrics, width: %d, height: %d\n", width, height);

    if (ctx->lcInExtX != width) {
        ctx->lcInExtX = width;
        changed = TRUE;
    } 
    if (ctx->lcInExtY != height) {
        ctx->lcInExtY = height;
        changed = TRUE;
    }
    
    if (changed) {
        ctx->lcOutOrgX  = ctx->lcInOrgX;
        ctx->lcOutOrgY  = ctx->lcInOrgY;
        ctx->lcOutOrgZ  = ctx->lcInOrgZ;
        ctx->lcOutExtX  = ctx->lcInExtX;
        ctx->lcOutExtY  = ctx->lcInExtY;
        ctx->lcOutExtZ  = ctx->lcInExtZ;
    }

    return changed;
}

static UINT convert_contextw(LPLOGCONTEXTW dst, LPLOGCONTEXTA src)
{
    if (dst) {
        _snwprintf_s(dst->lcName, LC_NAMELEN - 1, L"%s", src->lcName);
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

static UINT convert_contexta(LPLOGCONTEXTA dst, LPLOGCONTEXTW src)
{
    if (dst) {
        _snprintf_s(dst->lcName, LC_NAMELEN - 1, "%S", src->lcName);
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

static void _allocate_queue(void)
{
    UINT queue_bytes = sizeof(packet_data_t) * q_length;
    
    if (queue) {
        free(queue);
    }

    queue = (packet_data_t *) malloc(queue_bytes);
    memset(queue, 0, queue_bytes);
    q_start = 0;
    q_end = 0;
}

static void allocate_queue(void)
{
    EnterCriticalSection(&q_lock);
    _allocate_queue();
    LeaveCriticalSection(&q_lock);
}

static void release_queue(void)
{
    EnterCriticalSection(&q_lock);

    free(queue);
    q_length = 0;
    q_start = 0;
    q_end = 0;

    LeaveCriticalSection(&q_lock);
}

static void set_queue_length(int length)
{
    EnterCriticalSection(&q_lock);
    q_length = length + 1;
    _allocate_queue();
    LeaveCriticalSection(&q_lock);
}

// XXX: only call this when holding the queue lock
static UINT queue_size(void)
{
    if (q_start <= q_end) {
        return (q_end - q_start);
    } else {
        return (q_length - q_start) + q_end;
    }
}

static inline BOOL time_is_close(LONG a, LONG b)
{
    LONG d = (a - b);
    d *= d;
    return (d <= (TIME_CLOSE_MS * TIME_CLOSE_MS));
}

// XXX: only call this when holding the queue lock
static BOOL duplicate_packet(packet_data_t *pkt)
{
    UINT idx = (q_start + (queue_size() - 1)) % q_length;
    return (pkt->x == queue[idx].x)
        && (pkt->y == queue[idx].y)
        && time_is_close(pkt->time, queue[idx].time)
        && (pkt->pressure == queue[idx].pressure)
        && (pkt->contact == queue[idx].contact)
        && (pkt->buttons == queue[idx].buttons);
}

static BOOL enqueue_packet(packet_data_t *pkt)
{
    UINT size, idx;
    BOOL ret = FALSE;

    EnterCriticalSection(&q_lock);

    if (q_length > 0) {
        if (!duplicate_packet(pkt)) {
            size = queue_size();
            idx = q_end;

            q_end = (q_end + 1) % q_length;
            if (size >= (q_length - 1))
                q_start = (q_start + 1) % q_length;

            pkt->serial = next_serial++;
            memcpy(&(queue[idx]), pkt, sizeof(packet_data_t));
            ret = TRUE;
        }
    }

    LeaveCriticalSection(&q_lock);
    
    return ret;
}

static BOOL dequeue_packet(UINT serial, packet_data_t *pkt)
{
    UINT idx;
    BOOL ret = FALSE;

    EnterCriticalSection(&q_lock);

    idx = q_start;
    while (idx != q_end) {
        if (queue[idx].serial == serial) {
            break;
        } else {
            idx = (idx + 1) % q_length;
        }
    }
    if (idx != q_end) {
       q_start = (idx + 1) % q_length;
       memcpy(pkt, &(queue[idx]), sizeof(packet_data_t));
       ret = TRUE;
    }

    LeaveCriticalSection(&q_lock);

    return ret;
}

static UINT copy_handle(LPVOID lpOutput, HANDLE hVal)
{
    if (lpOutput) {
        *((HANDLE *)lpOutput) = hVal;
    }
    return sizeof(HANDLE);
}

static UINT copy_wtpkt(LPVOID lpOutput, WTPKT wtVal)
{
    if (lpOutput) {
        *((WTPKT *)lpOutput) = wtVal;
    }
    return sizeof(WTPKT);
}

static UINT copy_int(LPVOID lpOutput, int nVal)
{
    if (lpOutput) {
        *((int *)lpOutput) = nVal;
    }
    return sizeof(int);
}

static UINT copy_uint(LPVOID lpOutput, UINT nVal)
{
    if (lpOutput) {
        *((UINT *)lpOutput) = nVal;
    }
    return sizeof(UINT);
}

static UINT copy_long(LPVOID lpOutput, LONG nVal)
{
    if (lpOutput) {
        *((LONG *)lpOutput) = nVal;
    }
    return sizeof(LONG);
}

static UINT copy_dword(LPVOID lpOutput, DWORD nVal)
{
    if (lpOutput) {
        *((DWORD *)lpOutput) = nVal;
    }
    return sizeof(DWORD);
}

static UINT copy_strw(LPVOID lpOutput, CHAR *str)
{
    int ret = 0;
    if (lpOutput) {
        wchar_t *out = (wchar_t *)lpOutput;
        _snwprintf_s(out, MAX_STRING_BYTES, _TRUNCATE, L"%s", str);
        out[MAX_STRING_BYTES - 1] = L'\0'; 
    }
    return ((strlen(str) + 1) * sizeof(wchar_t));
}

static UINT copy_stra(LPVOID lpOutput, CHAR *str)
{
    int ret = 0;
    if (lpOutput) {
        char *out = (char *)lpOutput;
        _snprintf_s(out, MAX_STRING_BYTES, _TRUNCATE, "%s", str);
        out[MAX_STRING_BYTES - 1] = '\0';
    }
    return ((strlen(str) + 1) * sizeof(char));
}

static UINT copy_axis(LPVOID lpOutput, LONG axMin, LONG axMax, LONG axUnits, FIX32 axResolution)
{
    if (lpOutput) {
        LPAXIS ax = (LPAXIS)lpOutput;
        ax->axMin           = axMin;
        ax->axMax           = axMax;
        ax->axUnits         = axUnits;
        ax->axResolution    = axResolution;
    }
    return sizeof(AXIS);
}

static UINT fill_orientation(LPVOID lpOutput)
{
    if (lpOutput) {
        LPORIENTATION o = (LPORIENTATION)lpOutput;
        memset(o, 0, sizeof(ORIENTATION));
    }
    return sizeof(ORIENTATION);
}

static UINT fill_rotation(LPVOID lpOutput)
{
    if (lpOutput) {
        LPROTATION o = (LPROTATION)lpOutput;
        memset(o, 0, sizeof(ROTATION));
    }
    return sizeof(ROTATION);
}

static UINT write_packet(LPVOID lpPtr, packet_data_t *pkt)
{
    LPBYTE ptr = (LPBYTE)lpPtr;
	UINT data = context->lcPktData;
    UINT mode = context->lcPktMode;
    UINT n = 0;

	if (data & PK_CONTEXT)
		n += copy_handle((LPVOID)(ptr ? ptr + n : NULL), context);
	if (data & PK_STATUS)
		n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), 0);
	if (data & PK_TIME)
		n += copy_dword((LPVOID)(ptr ? ptr + n : NULL), pkt->time);
	if (data & PK_CHANGED)
		n += copy_wtpkt((LPVOID)(ptr ? ptr + n : NULL), 0xffff); // FIXME
	if (data & PK_SERIAL_NUMBER)
		n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), pkt->serial);
	if (data & PK_CURSOR)
		n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), 0x0); // XXX: check
	if (data & PK_BUTTONS)
		n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), pkt->buttons);
	if (data & PK_X)
		n += copy_long((LPVOID)(ptr ? ptr + n : NULL), pkt->x);
	if (data & PK_Y)
		n += copy_long((LPVOID)(ptr ? ptr + n : NULL), pkt->y);
	if (data & PK_Z)
		n += copy_long((LPVOID)(ptr ? ptr + n : NULL), 0);
	if (data & PK_NORMAL_PRESSURE) {
		if (mode & PK_NORMAL_PRESSURE)
		    n += copy_int((LPVOID)(ptr ? ptr + n : NULL), 0); // FIXME
		else
		    n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), pkt->pressure);
	}
	if (data & PK_TANGENT_PRESSURE) {
		if (mode & PK_TANGENT_PRESSURE)
		    n += copy_int((LPVOID)(ptr ? ptr + n : NULL), 0); // FIXME
		else
		    n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), pkt->pressure); // FIXME
	}
	if (data & PK_ORIENTATION)
		n += fill_orientation((LPVOID)(ptr ? ptr + n : NULL));
	if (data & PK_ROTATION)
		n += fill_rotation((LPVOID)(ptr ? ptr + n : NULL));

	return n;
}

static void LogPointerInfo(POINTER_INFO *pi)
{
	LogEntry(
		"pointerType           = %x\n"
		"pointerId             = %x\n"
		"frameId               = %x\n"
		"pointerFlags          = %x\n"
		"sourceDevice          = %x\n"
		"hwndTarget            = %x\n"
		"ptPixelLocation       = %d %d\n"
		"ptHimetricLocation    = %d %d\n"
		"ptPixelLocationRaw    = %d %d\n"
		"ptHimetricLocationRaw = %d %d\n"
		"dwTime                = %x\n"
		"historyCount          = %d\n"
		"inputData             = %x\n"
		"dwKeyStates           = %x\n"
		"ButtonChangeType      = %x\n",
		pi->pointerType,
		pi->pointerId,
		pi->frameId,
		pi->pointerFlags,
		pi->sourceDevice,
		pi->hwndTarget,
		pi->ptPixelLocation.x, pi->ptPixelLocation.y,
		pi->ptHimetricLocation.x, pi->ptHimetricLocation.y,
		pi->ptPixelLocationRaw.x, pi->ptPixelLocationRaw.y,
		pi->ptHimetricLocationRaw.x, pi->ptHimetricLocationRaw.y,
		pi->dwTime,
		pi->historyCount,
		pi->InputData,
		pi->dwKeyStates,
		pi->ButtonChangeType);
}

static void LogPacket(packet_data_t *pkt)
{
    LogEntry(
        "packet:\n"
        " serial     = %x\n"
        " contact    = %d\n"
        " time       = %d\n"
        " x          = %d\n"
        " y          = %d\n"
        " buttons    = %x\n"
        " pressure   = %d\n",
        pkt->serial,
        pkt->contact,
        pkt->time,
        pkt->x, pkt->y,
        pkt->buttons,
        pkt->pressure);
}

static BOOL handleMessage(UINT32 pointerId, POINTER_INPUT_TYPE pointerType, BOOL leavingWindow, LPMSG msg)
{
    const UINT n_buttons = 5;
    POINTER_PEN_INFO info;
    packet_data_t pkt;
    BOOL buttons[n_buttons];
    BOOL contact = FALSE;
    BOOL ret;
    UINT i;
    
    for (i = 0; i < n_buttons; ++i)
        buttons[i] = FALSE;
    
    if (pointerType == PT_PEN) {
        ret = GetPointerPenInfo(pointerId, &info);
        if (!leavingWindow) {
            buttons[0] = IS_POINTER_FIRSTBUTTON_WPARAM(msg->wParam);
            buttons[1] = IS_POINTER_SECONDBUTTON_WPARAM(msg->wParam);
            buttons[2] = IS_POINTER_THIRDBUTTON_WPARAM(msg->wParam);
            buttons[3] = IS_POINTER_FOURTHBUTTON_WPARAM(msg->wParam);
            buttons[4] = IS_POINTER_FIFTHBUTTON_WPARAM(msg->wParam);
            contact = IS_POINTER_INCONTACT_WPARAM(msg->wParam);
        }
    } else {
        ret = GetPointerInfo(pointerId, &(info.pointerInfo));
        info.penFlags   = 0;
        info.penMask    = 0;
        info.pressure   = 100;
        info.rotation   = 0;
        info.tiltX      = 0;
        info.tiltY      = 0;
        if (!leavingWindow) {
            buttons[0] = (info.pointerInfo.pointerFlags & POINTER_FLAG_FIRSTBUTTON) == POINTER_FLAG_FIRSTBUTTON;
            buttons[1] = (info.pointerInfo.pointerFlags & POINTER_FLAG_SECONDBUTTON) == POINTER_FLAG_SECONDBUTTON;
            buttons[2] = (info.pointerInfo.pointerFlags & POINTER_FLAG_THIRDBUTTON) == POINTER_FLAG_THIRDBUTTON;
            contact = (buttons[0] || buttons[1] || buttons[2]);
        }
    }
    if (!ret) {
        if (logging)
            LogEntry("failed to get pointer info for %x\n", pointerId);
        return FALSE;
    }

    pkt.serial  = 0;
    pkt.contact = contact;
    pkt.x       = info.pointerInfo.ptPixelLocation.x;
    pkt.y       = context->lcInExtY - info.pointerInfo.ptPixelLocation.y;
    pkt.pressure= contact ? info.pressure : 0;
    pkt.time    = info.pointerInfo.dwTime;
    pkt.buttons = (buttons[0] ? SBN_LCLICK : 0) 
                | (buttons[1] ? SBN_RCLICK : 0)
                | (buttons[2] ? SBN_MCLICK : 0);

    // do we need to do the following?
    // SkipPointerFrameMessages(info.pointerInfo.frameId);

    if (enqueue_packet(&pkt)) {
        if (logging) {
            LogEntry("queued packet\n");
            LogPacket(&pkt);
        }
        if (window)
            PostMessage(window, WT_PACKET, (WPARAM)pkt.serial, (LPARAM)context);
        return TRUE;
    } else {
        // packet is probably duplicate, or the queue has been deleted
        return FALSE;
    }
}

static void eraseMessage(LPMSG msg)
{
    // we can't actually delete messages, so change its type
    if (logging && debug)
        LogEntry("erase %04x\n", msg->message);
    msg->message = 0x0;
}

static void setWindowFeedback(HWND hWnd)
{
    FEEDBACK_TYPE settings[] = {
        FEEDBACK_PEN_BARRELVISUALIZATION,
        FEEDBACK_PEN_TAP,
        FEEDBACK_PEN_DOUBLETAP,
        FEEDBACK_PEN_PRESSANDHOLD,
        FEEDBACK_PEN_RIGHTTAP
    };
	BOOL setting;
    BOOL ret;
    int i;

    if (logging)
        LogEntry("configuring feedback for window: %p\n", hWnd);
    
    for (i = 0; i < (sizeof(settings) / sizeof(FEEDBACK_TYPE)); ++i) {
        setting = FALSE;
        ret = SetWindowFeedbackSetting(hWnd,
            settings[i],
            0,
            sizeof(BOOL), 
            &setting
        );
        if (logging)
            LogEntry(" setting: %d, ret: %d\n", settings[i], ret);
    }
}

static BOOL CALLBACK setFeedbackForThreadWindow(HWND hWnd, LPARAM lParam)
{
    setWindowFeedback(hWnd);
    return TRUE;
}

static void setFeedbackForWindows(void)
{
    if (overrideFeedback) {
        // enumerate windows of each thread (previouly detected)
        int i;
        for (i = 0; i < MAX_HOOKS; ++i) {
            if (hooks[i].thread)
                EnumThreadWindows(hooks[i].thread, setFeedbackForThreadWindow, NULL);
        }
    }
}

LRESULT CALLBACK emuHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    //LPCWPSTRUCT msg = (LPCWPSTRUCT)lParam;
	LPMSG msg = (LPMSG)lParam;
    DWORD thread = GetCurrentThreadId();
    HHOOK hook = NULL;
    UINT i;

    if (nCode < 0)
        goto end;

    for (i = 0; (i < MAX_HOOKS) && !hook; ++i) {
        if (hooks[i].thread == thread)
            hook = hooks[i].handle;
    }

    if (enabled && processing && queue) {
        POINTER_INPUT_TYPE pointerType = PT_POINTER;
        UINT32 pointerId = MOUSE_POINTER_ID;
        BOOL leavingWindow = FALSE;
        BOOL ignore = FALSE;
        LPARAM ext;

        switch (msg->message) {
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDBLCLK:
            case WM_NCLBUTTONDBLCLK:
            case WM_NCRBUTTONDBLCLK:
            case WM_MOUSEMOVE:
			case WM_NCMOUSEMOVE:
			case WM_LBUTTONDOWN: case WM_LBUTTONUP:
			case WM_RBUTTONDOWN: case WM_RBUTTONUP:
			case WM_NCLBUTTONDOWN: case WM_NCLBUTTONUP:
			case WM_NCRBUTTONDOWN: case WM_NCRBUTTONUP:
            //case WM_NCMOUSELEAVE:
                ext = GetMessageExtraInfo();
                //leavingWindow = (msg->message == WM_NCMOUSELEAVE);
                
                if (logging) {
                    LogEntry("%p %p %04x wParam:%x lParam:%x ext:%x, ignore: %d\n", 
                        hook, msg->hwnd, msg->message, msg->wParam, msg->lParam, ext, ignore);
                    if (debug) {
                        LogEntry(" x:%d y:%d\n", GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
                    }
                }
                
                if (IsPenEvent(ext) && (!(ext & 0x80))) {
                    // this is a pen event so hide it from the application
                    eraseMessage(msg);
                } else if (debug) {
                    // emulate pointer (from mouse events)
                    if (ignore) {
                        eraseMessage(msg);
                    } else if (handleMessage(pointerId, pointerType, leavingWindow, msg)) {
                        eraseMessage(msg);
                    }
                }
                break;
			
            case WM_POINTERUPDATE:
			case WM_POINTERUP: case WM_POINTERDOWN:
			case WM_POINTERENTER: case WM_POINTERLEAVE:
			case WM_POINTERDEVICEINRANGE: case WM_POINTERDEVICEOUTOFRANGE:
			case WM_NCPOINTERUPDATE:
			case WM_NCPOINTERUP: case WM_NCPOINTERDOWN:
                pointerId = GET_POINTERID_WPARAM(msg->wParam);
                // only process message if this is a pen
                if (GetPointerType(pointerId, &pointerType)) {
                    if (pointerType == PT_PEN) {
                        if (logging) {
                            LogEntry("%p %04x wParam:%x lParam:%x pointerId:%x pointerType:%x\n", hook, msg->message, msg->wParam, msg->lParam, pointerId, pointerType);
                        }
                        // we are interested in this pointer
                        if (handleMessage(pointerId, pointerType, leavingWindow, msg)) {
                            eraseMessage(msg);
                        }
                    }
                }
                break;

            case WM_DISPLAYCHANGE:
                if (logging) {
                    LogEntry("display changed, wParam:%d, lParam:%08x\n", msg->wParam, msg->lParam);
                }
                // FIXME: implement update and dispatch of WT_ message
                break;

            default:
                //LogEntry("i %04x\n", msg->message);
                break;
        }
    }

end:
    return CallNextHookEx(hook, nCode, wParam, lParam);
}

static int findThreadsForHooking(void)
{
    THREADENTRY32 te32;
    HANDLE hThreadSnap;
    DWORD pid = GetCurrentProcessId();
    int n = 0;

    hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); 
    if (hThreadSnap == INVALID_HANDLE_VALUE)
        return 0;

    te32.dwSize = sizeof(THREADENTRY32);
    if (!Thread32First(hThreadSnap, &te32)) {
        CloseHandle(hThreadSnap);
        return 0;
    }

    do { 
        if (te32.th32OwnerProcessID == pid) {
            hooks[n].thread = te32.th32ThreadID;
            n++;
        }
    } while (Thread32Next(hThreadSnap, &te32) && (n < MAX_HOOKS));

    CloseHandle(hThreadSnap);

    return n;
}

static void installHook(int id, hook_t *hook)
{
    hook->handle = SetWindowsHookEx(
        WH_GETMESSAGE, //WH_CALLWNDPROCRET,
        emuHookProc,
        NULL,
        hook->thread
    );
    if (logging)
        LogEntry("hook %d, thread = %08x, handle = %p\n", id, hook->thread, hook->handle);
}

static void installHooks(void)
{
    int n_hooks;
    int i;

    // clear array
    for (i = 0; i < MAX_HOOKS; ++i) {
        hooks[i].handle = NULL;
        hooks[i].thread = 0;
    }

    // map threads
    n_hooks = findThreadsForHooking();
    
    // install hooks per thread
    for (i = 0; i < n_hooks; ++i) {
        installHook(i, &(hooks[i]));
    }

    // setup feedback overrides
    setFeedbackForWindows();
}

static void uninstallHooks(void)
{
    int i = 0;
    
    while ((hooks[i].thread != 0) && (i < MAX_HOOKS)) {
        if (hooks[i].handle)
			UnhookWindowsHookEx(hooks[i].handle);
        hooks[i].handle = NULL;
        hooks[i].thread = 0;
        i++;
    }
}

void emuEnableThread(DWORD thread)
{
    int i;

    // don't hook if we are not enabled
    if (!enabled)
        return;
    
    if (logging)
        LogEntry("emuEnableThread(%08x)\n", thread);

    // find a free hook structure
    for (i = 0; i < MAX_HOOKS; ++i) {
        if (hooks[i].handle && hooks[i].thread == thread) {
            // thread is already hooked
            return;
        } else if (hooks[i].thread == 0) {
            hooks[i].thread = thread;
            installHook(i, &(hooks[i]));
            // FIXME: override windows feedback?
            return;
        }
    }

    // all hook structures are in use
}

void emuDisableThread(DWORD thread)
{
    int i;

    // no need to do anything if we are not enabled
    if (!enabled)
        return;
    
    if (logging)
        LogEntry("emuDisableThread(%08x)\n", thread);
    
    // find hook and remove it
    for (i = 0; i < MAX_HOOKS; ++i) {
        if (hooks[i].thread == thread) {
            if (hooks[i].handle)
			    UnhookWindowsHookEx(hooks[i].handle);
            hooks[i].handle = NULL;
            hooks[i].thread = 0;
            return;
        }
    }
}

static void enableProcessing(void)
{
    if (!enabled) {
        installHooks();
        enabled = TRUE;
    }
    processing = TRUE;
}

static void disableProcessing(BOOL hard)
{
    processing = FALSE;
    if (hard) {
        uninstallHooks();
	    enabled = FALSE;
    }
}

void emuSetModule(HMODULE hModule)
{
	module = hModule;
}

#if 0
static void findPointers(void)
{
    POINTER_DEVICE_INFO deviceData[MAX_POINTERS];
    UINT32 deviceCount = 0;
    BOOL ret = FALSE;
    UINT i;

    n_pointers = 0;

    ret = GetPointerDevices(&deviceCount, NULL);
    LogEntry("GetPointerDevices = %d, count = %d\n", ret, deviceCount);
    if (!ret)
        return;
    
    if (deviceCount > MAX_POINTERS) {
        deviceCount = MAX_POINTERS;
    } else if (deviceCount == 0) {
        // emulate for debugging
        pointers[n_pointers++] = MOUSE_POINTER_ID;
        return;
    }

    ret = GetPointerDevices(&deviceCount, deviceData);
    LogEntry("GetPointerDevices = %d, count = %d\n", ret, deviceCount);
    if (!ret)
        return;
	
    for (i = 0; i < deviceCount; ++i) {
        LogEntry("deviceData[%d] = %x\n", deviceData[i].pointerDeviceType);
    
        if (deviceData[i].pointerDeviceType == POINTER_DEVICE_TYPE_INTEGRATED_PEN
                || deviceData[i].pointerDeviceType == POINTER_DEVICE_TYPE_EXTERNAL_PEN) {
            // FIXME: work out pointerID
			// pointers[n_pointers++] = deviceData[i].;

            /*
            POINTER_DEVICE_CURSOR_INFO cursorData[MAX_CURSORS];
            UINT32 cursorCount = 0;
            UINT j;
            
            ret = GetPointerDeviceCursors(deviceData[i].device, &cursorCount, cursorData);
            LogEntry("GetPointerDeviceCursors = %d, count = %d\n", cursorCount);
            if (!ret)
                continue;

            for (j = 0; j < cursorCount && n_cursors < MAX_CURSORS; ++j) {
                LogEntry("%x %x\n", cursorData[j].cursorId, cursorData[j].cursor);
                cursors[n_cursors++] = cursorData[j].cursorId;
            }
            */
		}
    }
}
#endif

void emuInit(BOOL fLogging, BOOL fDebug)
{
    logging = fLogging;
    debug = fDebug;
    
    InitializeCriticalSection(&q_lock);

    init_context(&default_context);
    update_screen_metrics(&default_context);

    enabled = FALSE;
    processing = FALSE;
    q_length = 128;
    allocate_queue();

	EnableMouseInPointer(TRUE);
}

void emuShutdown(void)
{
    if (enabled)
        disableProcessing(TRUE);

    release_queue();

    window = NULL;
    module = NULL;

    if (context) {
        free(context);
        context = NULL;
    }
}

static UINT emuWTInfo(BOOL fUnicode, UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
    UINT ret = 0;
    switch (wCategory) {
        case WTI_INTERFACE:
            switch (nIndex) {
                case IFC_WINTABID:
                    if (fUnicode) {
                        ret = copy_strw(lpOutput, "Windows Pointer Emulation");
                    } else {
                        ret = copy_stra(lpOutput, "Windows Pointer Emulation");
                    }
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
                    break;
                case STA_BUTTONUSE:
                    ret = copy_dword(lpOutput, 0x7);
                    break;
                case STA_SYSBTNUSE:
                    ret = copy_dword(lpOutput, 0x7);
                    break;
            }
            break;
        case WTI_DEFCONTEXT:
        case WTI_DEFSYSCTX:
            if (nIndex == 0) {
                if (fUnicode) {
                    ret = convert_contextw((LPLOGCONTEXTW)lpOutput, &default_context);
                } else {
                    ret = copy_contexta((LPLOGCONTEXTA)lpOutput, &default_context);
                }
            }
            break;
        case WTI_DDCTXS:
        case WTI_DSCTXS:
            if (nIndex == 0) {
                if (fUnicode) {
                    ret = convert_contextw((LPLOGCONTEXTW)lpOutput, (context ? context : &default_context));
                } else {
                    ret = copy_contexta((LPLOGCONTEXTA)lpOutput, (context ? context : &default_context));
                }
            }
            break;
        case WTI_DEVICES:
            switch (nIndex) {
                case DVC_NAME:
                    if (fUnicode) {
                        ret = copy_strw(lpOutput, "Windows");
                    } else {
                        ret = copy_stra(lpOutput, "Windows");
                    }
                    break;
                case DVC_HARDWARE:
                    ret = copy_uint(lpOutput, HWC_INTEGRATED);
                    break;
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
                    break;
                case DVC_NPRESSURE:
                    ret = copy_axis(lpOutput, 0, 1023, 0, 0);
                    break;
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
    if (unicode) {
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
            disableProcessing(TRUE);
        }
        free(hCtx);
        context = NULL;
    }
	return TRUE;
}

int emuWTPacketsGet(HCTX hCtx, int cMaxPkts, LPVOID lpPkt)
{
	int ret = 0;
    
    if (hCtx && ((LPVOID)hCtx == (LPVOID)context)) {
        LPBYTE out = (LPBYTE)lpPkt;
        EnterCriticalSection(&q_lock);
        while ((ret < cMaxPkts) && (queue_size() > 0)) {
            out += write_packet((LPVOID)out, &(queue[q_start]));
            q_start = (q_start + 1) % q_length;
            ret++;
        }
        LeaveCriticalSection(&q_lock);
    }

    return ret;
}

BOOL emuWTPacket(HCTX hCtx, UINT wSerial, LPVOID lpPkt)
{
    packet_data_t pkt;
	BOOL ret = FALSE;

    if (hCtx && ((LPVOID)hCtx == (LPVOID)context)) {
        ret = dequeue_packet(wSerial, &pkt);
        if (ret && lpPkt) {
            write_packet(lpPkt, &pkt);
        }
    }

	return ret;
}

BOOL emuWTEnable(HCTX hCtx, BOOL fEnable)
{
    if (processing != fEnable) {
        if (fEnable) {
            enableProcessing();
        } else {
            disableProcessing(FALSE);
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

int emuWTPacketsPeek(HCTX hCtx, int cMaxPkt, LPVOID lpPkts)
{
	int ret = 0;
    
    if (hCtx && ((LPVOID)hCtx == (LPVOID)context)) {
        LPBYTE out = (LPBYTE)lpPkts;
        UINT old_q_start;
        
        EnterCriticalSection(&q_lock);
        old_q_start = q_start;

        while ((ret < cMaxPkt) && (queue_size() > 0)) {
            out += write_packet((LPVOID)out, &(queue[q_start]));
            q_start = (q_start + 1) % q_length;
            ret++;
        }

        q_start = old_q_start;
        LeaveCriticalSection(&q_lock);
    }
	
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
        return q_length;
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
