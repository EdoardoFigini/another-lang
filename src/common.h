#ifndef COMMON_H
#define COMMON_H

#include "efh/ss.h"

typedef struct {
  size_t line, col;
  ss_t line_view; 
  const char* path;
} loc_t;

#endif // COMMON_H
