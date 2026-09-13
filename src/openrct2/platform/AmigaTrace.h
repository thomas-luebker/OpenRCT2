#pragma once
// Stage-probe tracing for the AmigaOS port: one line per call, written straight to disk.
#ifdef __amigaos__
extern "C" void amiga_trace(const char* line);
extern "C" unsigned amiga_avail_kb(void);
extern "C" unsigned amiga_ticks_us(void);
extern "C" int amiga_env_flag(const char* name);
extern "C" int amiga_paint_log_armed;
extern "C" void amiga_malloc_stats(unsigned long* footprint, unsigned long* inUse);
    #define AMIGA_TRACE(msg) amiga_trace(msg)
    #define AMIGA_TRACE_ONCE(msg)                                                                                              \
        do                                                                                                                     \
        {                                                                                                                      \
            static bool _traced = false;                                                                                       \
            if (!_traced)                                                                                                      \
            {                                                                                                                  \
                _traced = true;                                                                                                \
                amiga_trace(msg);                                                                                              \
            }                                                                                                                  \
        } while (0)
#elif defined(OPENRCT2_TRACE_STDERR)
    #include <cstdio>
    #define AMIGA_TRACE(msg) std::fprintf(stderr, "TRACE: %s\n", msg)
    #define AMIGA_TRACE_ONCE(msg)                                                                                              \
        do                                                                                                                     \
        {                                                                                                                      \
            static bool _traced = false;                                                                                       \
            if (!_traced)                                                                                                      \
            {                                                                                                                  \
                _traced = true;                                                                                                \
                std::fprintf(stderr, "TRACE: %s\n", msg);                                                                      \
            }                                                                                                                  \
        } while (0)
#else
    #define AMIGA_TRACE(msg) ((void)0)
    #define AMIGA_TRACE_ONCE(msg) ((void)0)
#endif
