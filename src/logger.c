
#include "logger.h"
#include <stdio.h>
#include <stdarg.h>

const char* LOG_LEVEL_CHARS = "EWIDV";
const char* LOG_LEVEL_COLORS[] = {
	"\x1b[31m",
	"\x1b[33m",
	"\x1b[32m",
	"\x1b[0m",
	"\x1b[34m",
};
int _log_level = INFO;
int _log_color = 1;

void set_log_level(int log_level) {
	if (log_level < 0) log_level = 0;
	if (log_level > 4) log_level = 4;
	_log_level = log_level;
}

void set_log_color(int log_color) {
	_log_color = log_color;
}

void Log(int log_level, const char* format, ...) {
	if (log_level < 0) log_level = 0;
	if (log_level > 4) log_level = 4;
	if (log_level > _log_level) return;
	if (_log_color) printf("%s", LOG_LEVEL_COLORS[log_level]);
	printf("[%c] ", LOG_LEVEL_CHARS[log_level]);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	if (_log_color) printf("\x1b[0m");
	fflush(stdout);
}
