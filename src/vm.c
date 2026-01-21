#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>

#include <ffi.h>

#include "arena.h"
#include "sb.h"
#include "da.h"

#include "macros.h"
#include "types.h"

#include "vm.h"

#define MAX_STACK 0x1000 

task_t* load_task(vm_t* vm, program_t* p) {
  task_t* t = malloc(sizeof(*t));
  memset(t, 0, sizeof(*t));
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

  // constants
  {
    t->consts.data  = arena_alloc(&t->consts.arena, sizeof(uint32_t) * p->constants.count);
    t->consts.count = p->constants.count;

    for (size_t i=0; i < p->constants.count; i++) {
      constant_t* c = &p->constants.items[i];
      switch(c->kind) {
        case DK_STR:
          obj_t* o = malloc(sizeof(*o));
          o->kind = OBJ_STR;
          o->as.str.data = arena_strdup(&t->consts.arena, c->as.s);
          o->as.str.size = strlen(c->as.s);
          o->refs = 1;

          t->consts.data[i] = t->obj_pool.count;

          da_append(&t->obj_pool, o);
          break;
        case DK_NUMBER:
          TODO("load_task - numeric constants");
          break;
        default:
          UNREACHABLE("load-taks");
      }
    }
  }
  DLLIST_ADD(t, vm->tasks_head);

  return t;
}

#define CURR_FRAME(vm) \
  (vm)->active_task->call_stack.frames[(vm)->active_task->call_stack.depth - 1]

static inline void __dbg_print_stack(FILE* stream, vm_t* vm) {
  fprintf(stream, "+------------+\n");
  for(int i = MIN(MAX_STACK, (int)(vm->sp - vm->active_task->stack)) - 1; i >= 0; i--) {
    fprintf(stream, "| 0x%08X |\n", vm->active_task->stack[i]);
  }
  fprintf(stream, "+------------+\n\n");
}

#define POP(vm, v) \
  do { \
    if ((vm)->sp <= (vm)->active_task->stack) return VM_STACK_UNDERFLOW; \
    (v) = *(--(vm)->sp);\
  } while(0);

#define PUSH(vm, v) \
  do {\
    if ((vm)->sp >= (vm)->active_task->stack + MAX_STACK) return VM_STACK_OVERFLOW; \
    *((vm)->sp++) = (v); \
  } while(0);

vm_exitcode_t exec(vm_t* vm) {
  task_t* task = vm->active_task;
  if (vm->ip < task->code || vm->ip > task->last_inst)
    return VM_NO_MORE_INSTRUCTIONS;

  uint32_t operand = 0;
  uint32_t a = 0;
  uint32_t b = 0;

  switch(*vm->ip) {
    case INST_NOP:
      vm->ip++;
      break;
    case INST_PUSH:
      operand = *(++vm->ip);
      PUSH(vm, operand);
      vm->ip++;
      break;
    case INST_POP:
      TODO("INST_POP");
      if (vm->sp <= task->stack) return VM_STACK_UNDERFLOW;
      break;
    case INST_LOAD:
      operand = *(++vm->ip);
      PUSH(vm, CURR_FRAME(vm)->vars[operand]);
      vm->ip++;
      break;
    case INST_LOADG:
      TODO("INST_LOADG");
      break;
    case INST_LOADC:
      operand = *(++vm->ip);
      PUSH(vm, task->consts.data[operand]); 
      vm->ip++;
      break;
    case INST_STORE:
      operand = *(++vm->ip);
      POP(vm, CURR_FRAME(vm)->vars[operand]);
      vm->ip++;
      break;
    case INST_STOREG:
      TODO("INST_STOREG");
      break;
    case INST_JMP:
      operand = *(vm->ip + 1);
      vm->ip += operand;
      break;
    case INST_JNZ:
      operand = *(vm->ip + 1);
      POP(vm, a);
      if (a != 0)
        vm->ip += operand;
      else
        vm->ip++;
      break;
    case INST_JZ:
      operand = *(vm->ip + 1);
      POP(vm, a);
      if (a == 0)
        vm->ip += operand;
      else
        vm->ip++;
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
      POP(vm, b);
      POP(vm, a);
      PUSH(vm, a + b);
      vm->ip++;
      break;
    case INST_SUB:
      POP(vm, b);
      POP(vm, a);
      PUSH(vm, a - b);
      vm->ip++;
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
    case INST_EQ:
      POP(vm, b);
      POP(vm, a);
      PUSH(vm, a == b);
      vm->ip++;
      break;
    case INST_LEQ:
      POP(vm, b);
      POP(vm, a);
      PUSH(vm, a <= b);
      vm->ip++;
      break;
    case INST_GEQ:
      POP(vm, b);
      POP(vm, a);
      PUSH(vm, a >= b);
      vm->ip++;
      break;
    case INST_LT:
      POP(vm, b);
      POP(vm, a);
      PUSH(vm, a < b);
      vm->ip++;
      break;
    case INST_GT:
      POP(vm, b);
      POP(vm, a);
      PUSH(vm, a > b);
      vm->ip++;
      break;

    case INST_COUNT:
    default:
      return VM_INVALID_OPCODE;
  }
  return VM_OK;
}

void set_active_task(vm_t* vm, task_t* t) {
  assert(t);
  vm->active_task = t;
}

// TODO: handle non-uint32_t args
// -> have bind() macro/function that populates the stack before run()
vm_exitcode_t run(vm_t* vm, size_t n_args, ...) {
  va_list args;

  task_t* active = vm->active_task;
  vm->ip = active->code;
  vm->sp = active->stack;
  vm->halt = false;

  active->call_stack.frames[0] = malloc(sizeof(virtual_frame_t));
  active->call_stack.depth++;

  va_start(args, n_args);
  for(size_t i=0; i < n_args; i++) {
    if (vm->sp >= active->stack + MAX_STACK) return VM_STACK_OVERFLOW;
    *(vm->sp++) = va_arg(args, uint32_t); 
  }
  va_end(args);

  __dbg_print_stack(stdout, vm);
  while(!vm->halt) {
    fprintf(stdout, "%p (0x%08lX) %d\n", vm->ip, (vm->ip - active->code), *vm->ip);
    vm_exitcode_t ec = exec(vm);
    __dbg_print_stack(stdout, vm);
    if (ec != VM_OK) return ec;
  }

  // TODO: release resources

  return VM_OK;
}
