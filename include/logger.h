#ifndef __LOGGER_H__
#define __LOGGER_H__

#define ERROR 0
#define WARNING 1
#define INFO 2
#define DEBUG 3
#define VERBOSE 4

void Log(int log_level, const char* format, ...);
void set_log_level(int);
void set_log_color(int);

#define LOGE(format, ...) Log(ERROR, format, ##__VA_ARGS__)
#define LOGW(format, ...) Log(WARNING, format, ##__VA_ARGS__)
#define LOGI(format, ...) Log(INFO, format, ##__VA_ARGS__)
#define LOGD(format, ...) Log(DEBUG, format, ##__VA_ARGS__)
#define LOGV(format, ...) Log(VERBOSE, format, ##__VA_ARGS__)

// default info
#define SET_LOGE() set_log_level(ERROR)
#define SET_LOGW() set_log_level(WARNING)
#define SET_LOGI() set_log_level(INFO)
#define SET_LOGD() set_log_level(DEBUG)
#define SET_LOGV() set_log_level(VERBOSE)

// default on
#define SET_LOGCOLOR_OFF() set_log_color(0)
#define SET_LOGCOLOR_ON() set_log_color(1)

#endif