/**
 * @file log.c
 * @author rxi, modified by Matt Murphy (matt@murphysys.com)
 * @brief Write log messages to the console and to a file.
 * @date 2022-06-06
 */

/*
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <lmda/platform/log.h>


struct LogState* _L;


static char const* level_strings[]
    = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };

#ifdef LOG_USE_COLOR
static char const* level_colors[] = { "\x1b[94m", "\x1b[36m",   "\x1b[32m",
                                      "\x1b[33m", "\x1b[1;31m", "\x1b[35m" };
#endif

void
lm_log_init(struct LogState* log)
{
  _L = log;
}

static void
stdout_callback(struct LogEvent* ev)
{
  char buf[16];
  buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';
#ifdef LOG_USE_COLOR
  fprintf(
      stderr,
      "%s %s%-5s\x1b[0m \x1b[3;97m%s:%d:\x1b[0m ",
      buf,
      level_colors[ev->level],
      level_strings[ev->level],
      ev->file,
      ev->line);
#else
  fprintf(
      stderr,
      "%s %-5s %s:%d: ",
      buf,
      level_strings[ev->level],
      ev->file,
      ev->line);
#endif
  vfprintf(stderr, ev->fmt, ev->ap);
  fprintf(stderr, "\n");
  fflush(stderr);
}


static void
file_callback(struct LogEvent* ev)
{
  char buf[64];
  buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
  fprintf(
      _L->file,
      "%s %-5s %s:%d: ",
      buf,
      level_strings[ev->level],
      ev->file,
      ev->line);
  vfprintf(_L->file, ev->fmt, ev->ap);
  fprintf(_L->file, "\n");
  fflush(_L->file);
}

char const*
log_level_string(int level)
{
  return level_strings[level];
}


void
log_set_level(int level)
{
  _L->level = level;
}


void
log_set_quiet(bool enable)
{
  _L->quiet = enable;
}


static void
init_event(struct LogEvent* ev)
{
  if (!ev->time) {
    time_t t = time(NULL);
    ev->time = localtime(&t);
  }
}



void
log_log(
  int level,
  char const* file,
  int line,
  char const* fmt,
  ...
) {
  struct LogEvent ev = {
    .fmt    = fmt,
    .file   = file,
    .line   = line,
    .level  = level,
  };

  if (!_L->quiet && level >= _L->level) {
    init_event(&ev);

    va_start(ev.ap, fmt);
    stdout_callback(&ev);
    va_end(ev.ap);

    va_start(ev.ap, fmt);
    file_callback(&ev);
    va_end(ev.ap);
  }
}



bool
lm_log_create_file()
{
  char const* logFileName     = LMDA_LOG_FILE_NAME;
  size_t const maxLogFileSize = 1024 * LMDA_LOG_MAX_KB;

  struct stat attr;
  stat(logFileName, &attr);
  _L->file = fopen(logFileName, (attr.st_size < maxLogFileSize) ? "at" : "wt");
  return (_L->file != NULL);
}