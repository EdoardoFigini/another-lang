#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include <ffi.h>

#include "arena.h"
#include "sb.h"

#include "macros.h"
#include "types.h"

#include "vm.h"

#define MAX_STACK 0x1000 

task_t* load_task(vm_t* vm, program_t* p) {
  // cannot deallocate single taks in arena, might incur in high memory usage
  task_t* t = arena_alloc(&vm->tasks_arena, sizeof(*t));
  t->program = p;
#if defined(_WIN32)
  TODO("load_task - WIN32");
#elif defined(__linux__)
  void* pages = mmap(NULL, p->code.count * sizeof(*p->code.items), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(!pages) return NULL;
  memcpy(pages, p->code.items, p->code.count * sizeof(*p->code.items));
  t->code = pages;
  t->last_inst = t->code + (p->code.count - 1) * sizeof(*p->code.items);

  pages = mmap(NULL, MAX_STACK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(!pages) return NULL;

  t->stack = pages;
#else
  TODO("load_task - other platform");
#endif

  t->next = vm->tasks_head;
  vm->tasks_head = t;

  if (t->next)
    t->next->prev = t;
  t->prev = NULL;

  return t;
}

vm_exitcode_t exec(vm_t* vm) {
  if (vm->ip < vm->active_task->code || vm->ip > vm->active_task->last_inst)
    return VM_NO_MORE_INSTRUCTIONS;

  switch(*vm->ip) {
    case INST_NOP:
      vm->ip++;
      break;
    case INST_PUSH:
      uint32_t operand = *(++vm->ip);
      *vm->sp++ = operand;
      vm->ip++;
      break;
    case INST_POP:
      TODO("INST_POP");
      break;
    case INST_LOAD:
      TODO("INST_LOAD");
      break;
    case INST_LOADG:
      TODO("INST_LOADG");
      break;
    case INST_LOADC:
      TODO("INST_LOADC");
      break;
    case INST_STORE:
      TODO("INST_STORE");
      break;
    case INST_STOREG:
      TODO("INST_STOREG");
      break;
    case INST_CALL:
      TODO("INST_CALL");
      break;
    case INST_HOSTCALL:
      TODO("INST_HOSTCALL");
      break;
    case INST_ICALL:
      TODO("INST_ICALL");
      break;
    case INST_RET:
      TODO("INST_RET");
      break;
    case INST_ADD:
      TODO("INST_ADD");
      break;
    case INST_SUB:
      TODO("INST_SUB");
      break;
    case INST_MULT:
      TODO("INST_MULT");
      break;
    case INST_DIVI:
      TODO("INST_DIVI");
      break;
    case INST_DIVU:
      TODO("INST_DIVU");
      break;
    case INST_REM:
      TODO("INST_REM");
      break;
    case INST_ADDF:
      TODO("INST_ADDF");
      break;
    case INST_SUBF:
      TODO("INST_SUBF");
      break;
    case INST_MULTF:
      TODO("INST_MULTF");
      break;
    case INST_DIVF:
      TODO("INST_DIVF");
      break;
    case INST_COUNT:
      TODO("INST_COUNT");
      break;
  }
  return VM_OK;
}

void set_active_task(vm_t* vm, task_t* t) {
  assert(t);
  vm->active_task = t;
}

vm_exitcode_t run(vm_t* vm) {
  vm->ip = vm->active_task->code;
  vm->sp = vm->active_task->stack;
  vm->halt = false;

  while(!vm->halt) {
    vm_exitcode_t ec = exec(vm);
    if (ec != VM_OK) return ec;
  }

  // TODO: release resources

  return VM_OK;
}
