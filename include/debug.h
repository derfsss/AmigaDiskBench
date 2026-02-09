#ifndef DEBUG_H
#define DEBUG_H

#include <proto/exec.h>

/* Global Debug Switch */
#define DEBUG_ENABLED 1

/* Debug Macro */
#if DEBUG_ENABLED
#define LOG_DEBUG(fmt, ...) IExec->DebugPrintF("[AmigaDiskBench] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)                                                                                            \
    do {                                                                                                               \
    } while (0)
#endif

#endif /* DEBUG_H */
