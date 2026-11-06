#include <stdlib.h>

#include "report.h"

#include "efh/sb.h"
#include "efh/ss.h"
#include "efh/arena.h"

arena_t diag_arena = { 0 };

const char* diag_lvl_txt[] = {
  [DIAG_DEBUG] = "[DEBUG]",
  [DIAG_INFO]  = "[INFO]",
  [DIAG_WARN]  = "[WARN]",
  [DIAG_ERROR] = "[ERROR]",
  [DIAG_FATAL] = "[FATAL]",
};

const char* diag_lvl_color[] = {
  [DIAG_DEBUG] = "\033[35;1m",
  [DIAG_INFO]  = "\033[34;1m",
  [DIAG_WARN]  = "\033[33;1m",
  [DIAG_ERROR] = "\033[31;1m",
  [DIAG_FATAL] = "\033[41;1m",
};

static inline void sb_vdiagf(sb_t* sb, diag_lvl_t lvl, loc_t loc, const char* fmt, va_list args) {
  sb_append(sb, diag_lvl_color[lvl]);
  sb_append(sb, diag_lvl_txt[lvl]);
  sb_append(sb, "\033[0m");

  sb_append(sb, " ");

  int n = vsnprintf(NULL, 0, fmt, args);
  // TODO: malloc layer?
  char* buf = malloc(n * sizeof(*buf));
  n = vsnprintf(buf, n + 1, fmt, args);

  sb_n_append(sb, buf, n);
  // free(buf);

  sb_appendf(sb, "\n");
  sb_appendf(sb, "%s:%zu:%zu", loc.path, loc.line, loc.col);
  sb_appendf(sb, "\n");
  sb_appendf(sb, "%6zu | ", loc.line);
  sb_appendf(sb, PRI_SS, SS_FMT(loc.line_view));
  sb_appendf(sb, "\n");
  sb_appendf(sb, "%*s | %*s^", 6, "", (int) loc.col - 1, "");
}

FMT_PRINTF(4, 5) void fdiagf(FILE* stream, diag_lvl_t lvl, loc_t loc, const char* fmt, ...) {
  va_list args;
  sb_t sb = { 0 };

  va_start(args, fmt);
  sb_vdiagf(&sb, lvl, loc, fmt, args);
  va_end(args);

  fprintf(stream, "%.*s\n", SB_FMT(sb));

  sb_free(&sb);
}
