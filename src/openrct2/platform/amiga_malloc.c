/* Doug Lea's malloc (MIT-0) configured for AmigaOS/libnix: memory comes from AllocMem() in large steps,
 * every step is returned to the system when the program exits. Replaces libnix's flat-list malloc, whose
 * free() and malloc() walk every block (200-400 us per call with 40k blocks live). */
#include <proto/exec.h>
#include <exec/memory.h>
#include <stdlib.h>

#define LACKS_UNISTD_H 1
#define LACKS_FCNTL_H 1
#define LACKS_SYS_PARAM_H 1
#define LACKS_SYS_MMAN_H 1
#define LACKS_SCHED_H 1
#define LACKS_TIME_H 1
#define HAVE_MMAP 0
#define HAVE_MREMAP 0
#define HAVE_MORECORE 1
#define MORECORE amiga_morecore
#define MORECORE_CONTIGUOUS 0
#define MORECORE_CANNOT_TRIM 1
#define USE_LOCKS 0
#define NO_MALLINFO 1
#define NO_MALLOC_STATS 1
#define malloc_getpagesize 4096
#define DEFAULT_GRANULARITY (1024UL * 1024UL)
/* Heap corruption detection: FOOTERS stores a check word behind every chunk and fails loudly instead of
 * looping forever on a damaged bin; the failure is written to the trace file before the process ends. */
#define FOOTERS 1
#define PROCEED_ON_ERROR 0
void amiga_trace(const char* line);
static void amiga_heap_abort(void)
{
    amiga_trace("HEAP: dlmalloc detected a corrupted chunk (FOOTERS) -- aborting");
    abort();
}
#define ABORT amiga_heap_abort()
#define USAGE_ERROR_ACTION(m, p) amiga_heap_abort()
#define CORRUPTION_ERROR_ACTION(m) amiga_heap_abort()
#define USE_DL_PREFIX 1

struct amiga_step { struct amiga_step* next; unsigned long size; };
static struct amiga_step* g_steps = NULL;
static char* g_top = NULL;

static void* amiga_morecore(long n)
{
    if (n == 0)
        return g_top != NULL ? (void*)g_top : (void*)-1;
    if (n < 0)
        return (void*)-1;
    {
        unsigned long size = (unsigned long)n + sizeof(struct amiga_step);
        struct amiga_step* s = (struct amiga_step*)AllocMem(size, MEMF_ANY);
        if (s == NULL)
            return (void*)-1;
        s->size = size;
        s->next = g_steps;
        g_steps = s;
        g_top = (char*)(s + 1) + n;
        return (void*)(s + 1);
    }
}

#include "dlmalloc.inc"

void* malloc(size_t n) { return dlmalloc(n); }
void free(void* p) { dlfree(p); }
void* calloc(size_t n, size_t m) { return dlcalloc(n, m); }
void* realloc(void* p, size_t n) { return dlrealloc(p, n); }
void* memalign(size_t a, size_t n) { return dlmemalign(a, n); }
int posix_memalign(void** pp, size_t a, size_t n) { return dlposix_memalign(pp, a, n); }
size_t malloc_usable_size(void* p) { return dlmalloc_usable_size(p); }

/* Runs after every other destructor (priority 101 = first constructor, last destructor). */
__attribute__((destructor(101))) static void amiga_malloc_cleanup(void)
{
    struct amiga_step* s = g_steps;
    g_steps = NULL;
    while (s != NULL)
    {
        struct amiga_step* next = s->next;
        FreeMem(s, s->size);
        s = next;
    }
}
