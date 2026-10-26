#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void LoggerLock(void)
{
	int saved_errno = errno;
	pthread_mutex_lock(&g_log_mutex);
	errno = saved_errno;
}

static inline void LoggerUnlock(void)
{
	pthread_mutex_unlock(&g_log_mutex);
}

static inline void LogMessageFormatted(const char* tag, const char* color, const char* fmt, ...)
{
	LoggerLock();

	char buffer[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	long tid = (long)syscall(SYS_gettid);

	if (color != NULL)
	{
		printf("\033[%sm[%s:%ld] %s\033[0m\n", color, tag, tid, buffer);
	}
	else
	{
		printf("[%s:%ld] %s\n", tag, tid, buffer);
	}
	fflush(stdout);

	LoggerUnlock();
}

#define MAKE_LOGGER(tag) static const char* const _LOGGER_TAG = (tag)

#define LOG(...) LogMessageFormatted(_LOGGER_TAG, "90", __VA_ARGS__)
#define LOG_ERR(...) LogMessageFormatted(_LOGGER_TAG, "31;1", __VA_ARGS__)
#define LOG_PERR(str) LOG_ERR("%s: %s", str, strerror(errno))
#define LOG_INFO(...) LogMessageFormatted(_LOGGER_TAG, "94;1", __VA_ARGS__)

#define SAFE_LOG(calling) do { LoggerLock(); calling; LoggerUnlock(); } while (0)
