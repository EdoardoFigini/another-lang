#ifndef VM_H
#define VM_H

#include "types.h"

#ifdef __linux__
#include <dlfcn.h>
#else
#error Unsupported platform
#endif

bool compile(program_t* program, const char* source);

task_t* load_task(vm_t*, program_t*);
void set_active_task(vm_t*, task_t*);
vm_exitcode_t run(vm_t*, size_t, ...);

#endif
