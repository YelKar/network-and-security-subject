#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>

static sem_t* g_log_sem = SEM_FAILED;

static inline void InitLogger(void)
{
	sem_unlink("/server_logger_sem");
	g_log_sem = sem_open("/server_logger_sem", O_CREAT, 0644, 1);
}

static inline void DestroyLogger(void)
{
	if (g_log_sem != SEM_FAILED && g_log_sem != NULL)
	{
		sem_close(g_log_sem);
		sem_unlink("/server_logger_sem");
		g_log_sem = SEM_FAILED;
	}
}

static inline void LogMessageFormatted(const char* tag, const char* color, const char* fmt, ...)
{
	if (g_log_sem != SEM_FAILED && g_log_sem != NULL)
	{
		while (sem_wait(g_log_sem) == -1 && errno == EINTR)
		{
		}
	}

	va_list args;
	va_start(args, fmt);
	if (color != NULL)
	{
		printf("\033[%sm[%s] ", color, tag);
		vprintf(fmt, args);
		printf("\033[0m");
	}
	else
	{
		printf("[%s] ", tag);
		vprintf(fmt, args);
	}
	printf("\n");
	va_end(args);
	fflush(stdout);

	if (g_log_sem != SEM_FAILED && g_log_sem != NULL)
	{
		sem_post(g_log_sem);
	}
}

#define MAKE_LOGGER(tag) static const char* const _LOGGER_TAG = (tag)

#define LOG(...) LogMessageFormatted(_LOGGER_TAG, "90", __VA_ARGS__)
#define LOG_ERR(...) LogMessageFormatted(_LOGGER_TAG, "31;1", __VA_ARGS__)
#define LOG_PERR(str) LOG_ERR("%s: %s", str, strerror(errno))
#define LOG_INFO(...) LogMessageFormatted(_LOGGER_TAG, "94;1", __VA_ARGS__)
