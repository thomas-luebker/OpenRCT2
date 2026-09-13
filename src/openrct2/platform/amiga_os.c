/*****************************************************************************
 * AmigaOS bindings kept in plain C so the NDK headers never meet the C++ tree.
 *****************************************************************************/
#ifdef __amigaos__

    #include <devices/timer.h>
    #include <dos/dosextens.h>
    #include <proto/dos.h>
    #include <proto/exec.h>
    #include <proto/timer.h>
    #include <string.h>

/* libnix reads this at startup and swaps to a stack of this size; the CLI default is 4 KB. */
unsigned long __stack = 8UL * 1024UL * 1024UL;

static struct MsgPort* s_timerPort = NULL;
static struct timerequest* s_timerReq = NULL;
unsigned amiga_ticks_ms(void);

void amiga_sleep_ms(unsigned ms)
{
    /* Delay() only knows 1/50 s ticks, so a 5 ms wait cost 20 ms and the frame loop could never reach its 40 Hz tick
     * rate; timer.device UNIT_MICROHZ waits exactly as long as asked. The request is the one opened for GetSysTime,
     * which never keeps it busy (single-threaded, synchronous use only). */
    if (amiga_ticks_ms() == 0 || s_timerReq == NULL)
    {
        Delay((ms + 19) / 20 > 0 ? (ms + 19) / 20 : 1);
        return;
    }
    s_timerReq->tr_node.io_Command = TR_ADDREQUEST;
    /* The frame loop asks for "the rest of the 25 ms tick", truncated to whole ms: a request for 0 ms means "less
     * than a millisecond", and returning at once would spin the loop thousands of times per frame. */
    s_timerReq->tr_time.tv_secs = ms / 1000;
    s_timerReq->tr_time.tv_micro = ms == 0 ? 500 : (ms % 1000) * 1000;
    DoIO((struct IORequest*)s_timerReq);
}

struct timerequest* amiga_timer_request(void)
{
    return s_timerReq;
}

unsigned amiga_ticks_ms(void)
{
    /* GetSysTime() is wall-clock with microsecond resolution; good enough for frame timing. */
    struct Library* TimerBase;
    struct timeval tv;
    if (s_timerReq == NULL)
    {
        s_timerPort = CreateMsgPort();
        if (s_timerPort == NULL)
            return 0;
        s_timerReq = (struct timerequest*)CreateIORequest(s_timerPort, sizeof(struct timerequest));
        if (s_timerReq == NULL)
            return 0;
        if (OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest*)s_timerReq, 0) != 0)
        {
            s_timerReq = NULL;
            return 0;
        }
    }
    TimerBase = (struct Library*)s_timerReq->tr_node.io_Device;
    GetSysTime(&tv);
    return (unsigned)(tv.tv_secs * 1000u + tv.tv_micro / 1000u);
}

/* Microseconds, wall clock, for fine-grained profiling in the trace. */
unsigned amiga_ticks_us(void)
{
    struct Library* TimerBase;
    struct timeval tv;
    if (amiga_ticks_ms() == 0)
        return 0; /* opens the timer device on first use */
    TimerBase = (struct Library*)s_timerReq->tr_node.io_Device;
    GetSysTime(&tv);
    return (unsigned)(tv.tv_secs * 1000000u + tv.tv_micro);
}

/* 1 when the named AmigaDOS environment variable exists (SetEnv NAME 1); for trace-time experiments. */
int amiga_env_flag(const char* name)
{
    char buf[8];
    return GetVar((STRPTR)name, (STRPTR)buf, sizeof(buf), 0) > 0 ? 1 : 0;
}

/* Set by the input shim on the first key press; lets trace experiments start logging when the tester acts. */
int amiga_paint_log_armed = 0;

/* Free memory in KB (all types), for the trace. */
unsigned amiga_avail_kb(void)
{
    return (unsigned)(AvailMem(MEMF_ANY) / 1024);
}

/* Full path of the running program, e.g. "Work:OpenRCT2/openrct2-cli". Returns 0 on failure. */
int amiga_program_path(char* buf, unsigned size)
{
    BPTR dir = GetProgramDir();
    char name[108];
    if (size == 0)
        return 0;
    buf[0] = '\0';
    if (dir != 0)
    {
        if (!NameFromLock(dir, buf, size))
            buf[0] = '\0';
    }
    if (!GetProgramName(name, sizeof(name)))
        name[0] = '\0';
    if (buf[0] == '\0')
        return 0;
    if (!AddPart(buf, name, size))
        return 0;
    return 1;
}

/* Append one line to a trace file, opening and closing it each time so it survives a wedged process.
 * Opt-in: the trace is written only when the environment variable OPENRCT2_TRACE names the file
 * (e.g. `SetEnv OPENRCT2_TRACE Work:OpenRCT2/trace.txt`). Off by default so a tester's build does not
 * pay thousands of Open() calls during the park load, nor leave a growing log behind. */
void amiga_trace(const char* line)
{
    static unsigned t0 = 0;
    static int enabled = -1;
    static char path[256];
    char stamp[24];
    unsigned now;
    BPTR fh;
    if (enabled < 0)
    {
        enabled = (GetVar((STRPTR) "OPENRCT2_TRACE", (STRPTR)path, sizeof(path), 0) > 0) ? 1 : 0;
    }
    if (!enabled)
        return;
    now = amiga_ticks_ms();
    if (t0 == 0)
        t0 = now;
    fh = Open((STRPTR)path, MODE_READWRITE);
    if (fh == 0)
        return;
    Seek(fh, 0, OFFSET_END);
    /* elapsed milliseconds since the first trace line, so the load can be profiled from the file */
    {
        unsigned ms = now - t0;
        int n = 0;
        char rev[12];
        stamp[n++] = '[';
        stamp[n++] = '+';
        {
            int r = 0;
            do
            {
                rev[r++] = (char)('0' + ms % 10);
                ms /= 10;
            } while (ms != 0);
            while (r > 0)
                stamp[n++] = rev[--r];
        }
        stamp[n++] = 'm';
        stamp[n++] = 's';
        stamp[n++] = ']';
        stamp[n++] = ' ';
        Write(fh, (APTR)stamp, n);
    }
    Write(fh, (APTR)line, strlen(line));
    Write(fh, (APTR) "\n", 1);
    Close(fh);
}

#endif
