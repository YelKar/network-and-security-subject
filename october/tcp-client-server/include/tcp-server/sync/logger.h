#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

static inline void LogMessageFormatted(const char* tag, const char* color, const char* fmt, ...)
{
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
}

#define MAKE_LOGGER(tag) static const char* const _LOGGER_TAG = (tag)

#define LOG(...) LogMessageFormatted(_LOGGER_TAG, "90", __VA_ARGS__)
#define LOG_ERR(...) LogMessageFormatted(_LOGGER_TAG, "31;1", __VA_ARGS__)
#define LOG_PERR(str) LOG_ERR("%s: %s", str, strerror(errno))
#define LOG_INFO(...) LogMessageFormatted(_LOGGER_TAG, "94;1", __VA_ARGS__)
