/**
 * Copyright (c) 2020 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */


#ifndef LMDA_LOG_H
#define LMDA_LOG_H


/** @cond */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
/** @endcond */

struct tm;


/** \struct LogState
 * Used to persist the log output streams between hot-reloads.
 */
struct LogState {
  int level;
  bool quiet;
  FILE* file;
};

/** \struct LogEvent
 * Represents a log message with associated severity.
 */
struct LogEvent {
  va_list ap;
  char const* fmt;
  char const* file;
  struct tm* time;
  int line;
  int level;
};


enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };


#define log_trace(...)  log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...)  log_log(LOG_DEBUG,  __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)   log_log(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)   log_log(LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...)  log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...)  log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

char const* log_level_string(int level);
void log_set_level(int level);
void log_set_quiet(bool enable);

void log_log(
  int level,
  char const* file,
  int line,
  char const* fmt,
  ...
);

void lm_log_init(struct LogState* log);

bool lm_log_create_file();


#endif // LMDA_LOG_H