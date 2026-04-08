#ifndef VM_H
#define VM_H

#include "types.h"

#ifdef __linux__
#include <dlfcn.h>
#elif defined(_WIN32)
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#else
#error Unsupported platform
#endif

bool compile_from_file(program_t* program, const char* path);
bool compile_from_cstr(program_t* program, const char* source);

task_t* load_task(program_t*);
void set_active_task(task_t*);
vm_exitcode_t call(const char*, size_t, ...);

#endif
