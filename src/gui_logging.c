/*
 * AmigaDiskBench - A modern benchmark for AmigaOS 4.x
 * Copyright (c) 2026 Team Derfs. All rights reserved.
 */

/*
 * gui_logging.c
 *
 * Thread-safe user-facing log subsystem for the Log tab.
 *
 * CROSS-PROCESS SAFETY:
 *   On AmigaOS 4, each process has its own BSS/data segment.  The
 *   BenchmarkWorker subprocess sees its own zero-initialised copy of
 *   the GUIState (ui) — it cannot safely touch the main task's log_buf
 *   or any gadget objects.
 *
 *   The solution uses standard Exec message-passing (IPC):
 *
 *   Worker path (ui.log_main_task == NULL in worker BSS):
 *     LogUser() formats the timestamped line, allocates a BenchLogMsg
 *     from MEMF_SHARED, and PutMsg()s it to the worker_reply_port (the
 *     same port used for BenchStatus messages).  The main task's
 *     HandleWorkerReply() receives MSG_TYPE_LOG messages and calls
 *     LogAppendLine() + RefreshLogDisplay() directly.
 *
 *   Main task path (ui.log_main_task != NULL):
 *     LogUser() calls LogAppendLine() directly — no message needed.
 *     RefreshLogDisplay() is called immediately after.
 *
 * DISPLAY:
 *   log_buf is a heap string that grows in LOG_GROW_SIZE increments.
 *   RefreshLogDisplay() appends only newly added text to the gadget
 *   using GM_TEXTEDITOR_InsertText at GV_TEXTEDITOR_InsertText_Bottom,
 *   then calls ARexxCmd "GOTOBOTTOM" to auto-scroll.
 *   log_buf_displayed tracks how many bytes have already been inserted.
 *
 * CLIPBOARD:
 *   CopyLogToClipboard() writes ui.log_buf directly to clipboard unit 0
 *   via IFFParse (FTXT/CHRS chunk), bypassing the read-only gadget.
 */

#include "gui_internal.h"
#include <stdarg.h>
#include <time.h>

#define LOG_LINE_BUF    512         /* per-line format buffer             */
#define LOG_GROW_SIZE   (16 * 1024) /* heap growth increment for log_buf  */

/* Reply port set by the worker via LogSetWorkerReplyPort().
 * Lives in the worker's own BSS — points to a MEMF_SHARED MsgPort. */
static struct MsgPort *s_log_reply_port = NULL;

/* ------------------------------------------------------------------ */
/* Called by BenchmarkWorker at job start to register the reply port.  */
/* ------------------------------------------------------------------ */

void LogSetWorkerReplyPort(struct MsgPort *port)
{
    s_log_reply_port = port;
}

/* ------------------------------------------------------------------ */
/* Internal: append a pre-formatted line to the main task's log_buf.  */
/* Call from main task only.                                           */
/* ------------------------------------------------------------------ */

void LogAppendLine(const char *line)
{
    if (!line || line[0] == '\0')
        return;

    uint32 line_len = strlen(line);
    uint32 needed   = ui.log_buf_len + line_len + 1;

    if (needed > ui.log_buf_cap) {
        uint32 new_cap = needed + LOG_GROW_SIZE;
        char *new_buf  = IExec->AllocVecTags(new_cap, AVT_ClearWithValue, 0, TAG_DONE);
        if (!new_buf)
            return;
        if (ui.log_buf && ui.log_buf_len > 0)
            memcpy(new_buf, ui.log_buf, ui.log_buf_len);
        if (ui.log_buf)
            IExec->FreeVec(ui.log_buf);
        ui.log_buf     = new_buf;
        ui.log_buf_cap = new_cap;
    }

    memcpy(ui.log_buf + ui.log_buf_len, line, line_len);
    ui.log_buf_len += line_len;
    ui.log_buf[ui.log_buf_len] = '\0';
}

/* ------------------------------------------------------------------ */
/* Initialise the log subsystem (main task only, before window opens). */
/* ------------------------------------------------------------------ */

void InitUserLogging(void)
{
    ui.log_buf           = NULL;
    ui.log_buf_len       = 0;
    ui.log_buf_cap       = 0;
    ui.log_buf_displayed = 0;
    ui.log_main_task     = IExec->FindTask(NULL);
}

/* ------------------------------------------------------------------ */
/* Refresh the texteditor display — append only new text.             */
/* MUST be called from the main GUI task only.                         */
/* ------------------------------------------------------------------ */

void RefreshLogDisplay(void)
{
    if (!ui.log_editor || !ui.window)
        return;

    if (!ui.log_buf || ui.log_buf_len == 0)
        return;

    if (ui.log_buf_displayed >= ui.log_buf_len)
        return; /* nothing new */

    /* Insert only the newly added portion at the bottom.
     * DoGadgetMethod (not IDoMethod) is required for visual refresh. */
    const char *new_text = ui.log_buf + ui.log_buf_displayed;

    struct GP_TEXTEDITOR_InsertText insert;
    insert.MethodID = GM_TEXTEDITOR_InsertText;
    insert.GInfo    = NULL;
    insert.text     = (STRPTR)new_text;
    insert.pos      = GV_TEXTEDITOR_InsertText_Bottom;
    IIntuition->DoGadgetMethodA((struct Gadget *)ui.log_editor, ui.window, NULL, (Msg)&insert);

    ui.log_buf_displayed = ui.log_buf_len;

    /* Scroll to bottom — also needs DoGadgetMethod for rendering */
    IIntuition->DoGadgetMethod((struct Gadget *)ui.log_editor, ui.window, NULL,
                               GM_TEXTEDITOR_ARexxCmd, NULL, "GOTOBOTTOM");
}

/* ------------------------------------------------------------------ */
/* Append a formatted, timestamped line.  May be called from worker.  */
/* Uses C stdlib snprintf/vsnprintf — safe from worker (no IUtility). */
/* ------------------------------------------------------------------ */

void LogUser(const char *fmt, ...)
{
    if (!fmt)
        return;

    char msg[LOG_LINE_BUF];
    char line[LOG_LINE_BUF + 16];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (msg[0] == '\0')
        return;

    /* Build timestamped line */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[12];
    if (t != NULL)
        strftime(ts, sizeof(ts), "%H:%M:%S", t);
    else
        strncpy(ts, "Unknown", sizeof(ts));
    snprintf(line, sizeof(line), "[%s] %s\n", ts, msg);

    if (ui.log_main_task != NULL) {
        /* Main task: append directly */
        LogAppendLine(line);
        RefreshLogDisplay();
    } else {
        /* Worker subprocess: send via Exec message to main task */
        BenchLogMsg *lm = IExec->AllocVecTags(sizeof(BenchLogMsg),
                                               AVT_Type, MEMF_SHARED,
                                               AVT_ClearWithValue, 0,
                                               TAG_DONE);
        if (!lm)
            return;
        lm->msg_type = MSG_TYPE_LOG;
        /* Use standard strncpy — IUtility may be NULL in worker */
        strncpy(lm->line, line, sizeof(lm->line) - 1);
        lm->line[sizeof(lm->line) - 1] = '\0';
        if (s_log_reply_port)
            IExec->PutMsg(s_log_reply_port, &lm->msg);
        else
            IExec->FreeVec(lm);
    }
}

/* ------------------------------------------------------------------ */
/* Wipe the in-memory buffer and clear the texteditor display.         */
/* Call from main GUI task only.                                        */
/* ------------------------------------------------------------------ */

void ClearUserLog(void)
{
    if (!ui.log_editor || !ui.window)
        return;

    if (ui.log_buf) {
        IExec->FreeVec(ui.log_buf);
        ui.log_buf = NULL;
    }
    ui.log_buf_len       = 0;
    ui.log_buf_cap       = 0;
    ui.log_buf_displayed = 0;

    IIntuition->DoGadgetMethod((struct Gadget *)ui.log_editor, ui.window, NULL, GM_TEXTEDITOR_ClearText, NULL);

    /* Re-emit the session header so the log is never shown blank */
    LogUser("%s", APP_VER_TITLE);
    LogUser("Session started");
}

/* ------------------------------------------------------------------ */
/* Copy the entire log to the clipboard via IFFParse.                  */
/* Writes ui.log_buf directly — works regardless of GA_ReadOnly.      */
/* Call from main GUI task only.                                        */
/* ------------------------------------------------------------------ */

void CopyLogToClipboard(void)
{
    if (!ui.log_buf || ui.log_buf_len == 0) {
        ShowMessage("Log", "The log is empty.", "OK");
        return;
    }

    struct IFFHandle *iff = IIFFParse->AllocIFF();
    if (!iff) {
        ShowMessage("Log", "Could not allocate IFF handle.", "OK");
        return;
    }

    struct ClipboardHandle *ch = IIFFParse->OpenClipboard(PRIMARY_CLIP);
    if (!ch) {
        IIFFParse->FreeIFF(iff);
        ShowMessage("Log", "Could not open clipboard.", "OK");
        return;
    }

    iff->iff_Stream = (ULONG)ch;
    IIFFParse->InitIFFasClip(iff);

    LONG err = IIFFParse->OpenIFF(iff, IFFF_WRITE);
    if (err == 0) {
        /* IFF clipboard text format: FORM FTXT { CHRS <bytes> } */
        err = IIFFParse->PushChunk(iff, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN);
        if (err == 0) {
            err = IIFFParse->PushChunk(iff, 0, ID_CHRS, IFFSIZE_UNKNOWN);
            if (err == 0) {
                IIFFParse->WriteChunkBytes(iff, ui.log_buf, ui.log_buf_len);
                IIFFParse->PopChunk(iff); /* close CHRS */
            }
            IIFFParse->PopChunk(iff); /* close FORM */
        }
        IIFFParse->CloseIFF(iff);
    }

    IIFFParse->CloseClipboard(ch);
    IIFFParse->FreeIFF(iff);
}

/* ------------------------------------------------------------------ */
/* Free all resources.  Call from gui.c on exit.                       */
/* ------------------------------------------------------------------ */

void CleanupUserLogging(void)
{
    if (ui.log_buf) {
        IExec->FreeVec(ui.log_buf);
        ui.log_buf = NULL;
    }
    ui.log_buf_len       = 0;
    ui.log_buf_cap       = 0;
    ui.log_buf_displayed = 0;
    ui.log_main_task     = NULL;
}
