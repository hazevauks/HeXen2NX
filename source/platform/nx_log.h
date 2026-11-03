/* nx_log.h -- log file on the SD card, and nxlink when connected. */

#ifndef NX_LOG_H
#define NX_LOG_H

void NX_LogInit (void);
void NX_LogShutdown (void);
void NX_LogWrite (const char *text);
void NX_LogPrintf (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void NX_LogFlush (void);

#endif	/* NX_LOG_H */
