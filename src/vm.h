#ifndef VM_H
#define VM_H

#include "types.h"

bool compile(program_t* program, const char* source);

task_t* load_task(vm_t*, program_t*);
void set_active_task(vm_t*, task_t*);
vm_exitcode_t run(vm_t*);

#endif
