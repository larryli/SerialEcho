/*
 * trace.h - Trace logging utility
 *
 * Provides TRACE_LOG macro for debug logging to trace.log file.
 * Enabled by defining ENABLE_TRACE=1 at compile time.
 */

#ifndef TRACE_H
#define TRACE_H

#ifdef ENABLE_TRACE

#include <windows.h>

void Trace_Init(void);
void Trace_Close(void);
void Trace_Write(const char *tag, const char *fmt, ...);

#define TRACE_INIT()    Trace_Init()
#define TRACE_CLOSE()   Trace_Close()
#define TRACE_LOG(tag, ...)  Trace_Write(tag, __VA_ARGS__)

#else /* !ENABLE_TRACE */

#define TRACE_INIT()
#define TRACE_CLOSE()
#define TRACE_LOG(tag, ...)

#endif /* ENABLE_TRACE */

#endif /* TRACE_H */
