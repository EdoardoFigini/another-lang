#ifndef REPORT_H
#define REPORT_H

#include <stdarg.h>
#include <stdio.h>

#include "efh/sb.h"

#include "common.h"
#include "macros.h"

typedef enum {
  DIAG_DEBUG = 0,
  DIAG_INFO,
  DIAG_WARN,
  DIAG_ERROR,
  DIAG_FATAL,
} diag_lvl_t;

FMT_PRINTF(4, 5) void fdiagf(FILE* stream, diag_lvl_t lvl, loc_t loc, const char* fmt, ...);

#endif // REPORT_H
