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
 
#if 0
const char* source = 
  "extern puts: (s: char[]) -> i32;\n"
  "extern floor: (f: f32) -> i32;\n"
  "\n"
  "pippo : i32 = 0;\n"
  "pluto : f32[];\n"
  "extern topolino : str;\n"
  "\n"
  "export main : (arg: i32) -> i32 {\n"
  // " puts(\"Hello World!\\n\".c_str());\n"
  " baz : str = \"Hello World!\\n\";\n"
  " baz = bar(0 <= 5, arg);\n"
  // " return 1 + mod::obj.foo(2, 8) / 3 - 4 * ( 5 + 6 * 7 ) % 9;\n"
  " return floor(1 / 3.0);\n" 
  // " return 1 / 3 - 4 * ( 5 + 6 * 7 ) % 9;\n"
  "}\n"
  "\n"
  "bar : (a: bool, b: i32) -> str {\n"
  " bar : str = \"bar\";\n"
  " return bar;\n"
  "}\n"
  ;
#endif

#if 0
const char* source =
  "export fib: (n : u32) -> u32 {\n"
  "  if (n < 2U) {\n"
  "    return n;\n"
  "  } else {\n"
  "    return fib(n - 1U) + fib(n - 2U);\n"
  "  }\n"
  "}\n"
  ;
#endif

const char* source = 
 "extern test: () -> none;\n"
 "\n"
 "main: () -> none {\n"
 "  test();\n"
 "}\n"
 ;

void test() {
  printf("Hello from host!\n");
}

int main() {
  program_t prog = { 0 };
  if(!compile(&prog, source)) {
    fprintf(stderr, "compilation failed!\n");
    return 1;
  }

  vm_t vm = { 0 };
  task_t* t = load_task(&vm, &prog);
  set_active_task(&vm, t);
  return run(&vm, 0);
}

#define SB_IMPLEMENTATION
#include "sb.h"
#define DA_IMPLEMENTATION
#include "da.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"
