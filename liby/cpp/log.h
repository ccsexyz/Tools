#ifndef LOG_H
#define LOG_H


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>


void log_err(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_exit(int err, const char *fmt, ...);
static void log_doit(int errnoflag, int error, const char *fmt, va_list ap);
void log_quit(const char *fmt, ...);

#endif // LOG_H