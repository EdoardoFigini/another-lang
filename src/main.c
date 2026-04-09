#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

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
  vm_exitcode_t res = call("main", 0);


  sb_t sb = { 0 };
  sb_appendf(&sb, "Exited. Cause: ");
  switch (res) {
    case VM_OK:                   sb_appendf(&sb, "OK");                   break;
    case VM_NEXT:                 sb_appendf(&sb, "NEXT");                 break;
    case VM_INVALID_OPCODE:       sb_appendf(&sb, "INVALID_OPCODE");       break;
    case VM_NO_MORE_INSTRUCTIONS: sb_appendf(&sb, "NO_MORE_INSTRUCTIONS"); break;
    case VM_STACK_OVERFLOW:       sb_appendf(&sb, "STACK_OVERFLOW");       break;
    case VM_STACK_UNDERFLOW:      sb_appendf(&sb, "STACK_UNDERFLOW");      break;
    case VM_CALL_STACK_OVERFLOW:  sb_appendf(&sb, "CALL_STACK_OVERFLOW");  break;
    case VM_UNDEFINED_EXTERN:     sb_appendf(&sb, "UNDEFINED_EXTERN");     break;
    case VM_UNDEFINED_EXPORT:     sb_appendf(&sb, "UNDEFINED_EXPORT");     break;
  }
  sb_appendf(&sb, ".\n");
  fprintf(stdout, "%.*s", SB_FMT(sb));
  return res;
}

#define SB_IMPLEMENTATION
#include "sb.h"
#define DA_IMPLEMENTATION
#include "da.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"
#define SLICE_IMPLEMENTATION
#include "slice.h"
