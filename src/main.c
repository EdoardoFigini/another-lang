#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#include <ffi.h>

#include "sb.h"
#include "da.h"
#include "arena.h"

#include "types.h"
#include "macros.h"

#include "vm.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "No input file.\n");
    fprintf(stderr, "USAGE:\n %s <file>\n", argv[0]);
    return 1;
  }

  program_t prog = { 0 };
  if(!compile_from_file(&prog, argv[1])) {
    fprintf(stderr, "compilation failed!\n");
    return 1;
  }

  task_t* t = load_task(&prog);
  set_active_task(t);
  return run(0);
}

#define SB_IMPLEMENTATION
#include "sb.h"
#define DA_IMPLEMENTATION
#include "da.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"
#define SLICE_IMPLEMENTATION
#include "slice.h"
