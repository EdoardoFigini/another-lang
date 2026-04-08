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

// TODO: add platform dependent mutex to vm_t
static vm_t __vm;

vm_t* get_vm_instance() {
  // TODO: make thread safe (should block)
  return &__vm;
}

obj_t* get_obj(obj_handle_t h) {
  const vm_t* vm = get_vm_instance();
  return da_at(vm->active_task->obj_pool, h);
}

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#define MAX_STACK 0x1000 

// BUILTIN FUNCTION

EXPORT int print(const char* s) {
  return puts(s);
}

EXPORT int str_cmp(const char* a, const char* b) {
  return strcmp(a, b);
}

// BUILTIN METHODS

EXPORT const char* str__c_str(obj_handle_t self) {
  // TODO: duplicate data
  return get_obj(self)->as.str.data; 
}

EXPORT int str__length(obj_handle_t self) {
  return (int)get_obj(self)->as.str.size; 
}

EXPORT obj_handle_t str__concat(obj_handle_t self, obj_handle_t other) {
  obj_t* str = get_obj(self);
  obj_t* str_other = get_obj(other);

  vm_t* vm = get_vm_instance();

  obj_t* o = malloc(sizeof(*o));
  o->kind = OBJ_STR;
  o->as.str.data = arena_sprintf(&vm->active_task->arena, "%s%s", str->as.str.data, str_other->as.str.data);
  o->as.str.size = strlen(o->as.str.data);
  o->refs = 1;

  da_append(&vm->active_task->obj_pool, o);

  return vm->active_task->obj_pool.count - 1;
}

EXPORT obj_handle_t i32__to_str(int32_t self) {
  vm_t* vm = get_vm_instance();

  obj_t* o = malloc(sizeof(*o));
  o->kind = OBJ_STR;
  o->as.str.data = arena_sprintf(&vm->active_task->arena, "%d", self);
  o->as.str.size = strlen(o->as.str.data);
  o->refs = 1;

  da_append(&vm->active_task->obj_pool, o);

  return vm->active_task->obj_pool.count - 1;
}

//////////////////////////////

static inline bool is_ffi_arg_32(ffi_type* t) {
  return t == &ffi_type_sint32 ||
         t == &ffi_type_uint32 ||
         t == &ffi_type_float ||
         t == &ffi_type_uchar ||
         t == &ffi_type_uint8;
}

task_t* load_task(program_t* p) {
  vm_t* vm = get_vm_instance();

  task_t* t = malloc(sizeof(*t));
  memset(t, 0, sizeof(*t));
  t->program = p;

#if defined(_WIN32)
  void* pages = VirtualAlloc(NULL, p->code.count * sizeof(*p->code.items), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if(!pages) return NULL;
  CopyMemory(pages, p->code.items, p->code.count * sizeof(*p->code.items));
  t->code = pages;
  t->last_inst = t->code + (p->code.count - 1) * sizeof(*p->code.items);
  DWORD old_prot = 0;
  VirtualProtect(t->code, (p->code.count - 1) * sizeof(*p->code.items), PAGE_READONLY, &old_prot);

  pages = VirtualAlloc(NULL, MAX_STACK, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if(!pages) return NULL;
  t->stack = pages;
#elif defined(__linux__)
  void* pages = mmap(NULL, p->code.count * sizeof(*p->code.items), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(!pages) return NULL;
  memcpy(pages, p->code.items, p->code.count * sizeof(*p->code.items));
  t->code = pages;
  t->last_inst = t->code + (p->code.count - 1) * sizeof(*p->code.items);
  // TODO: make code read-only

  pages = mmap(NULL, MAX_STACK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(!pages) return NULL;

  t->stack = pages;
#else
  TODO("load_task - other platform");
#endif

  // constants
  {
    t->consts.data  = arena_alloc(&t->arena, sizeof(uint32_t) * p->constants.count);
    t->consts.count = p->constants.count;

    for (size_t i=0; i < p->constants.count; i++) {
      constant_t* c = &p->constants.items[i];
      switch(c->kind) {
        case DK_STR:
          obj_t* o = malloc(sizeof(*o));
          o->kind = OBJ_STR;
          o->as.str.data = arena_strdup(&t->arena, c->as.s);
          o->as.str.size = strlen(c->as.s);
          o->refs = 1;

          free((void*)c->as.s);

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

  // externs
  // TODO: configurable lazy/eager externs loading
  {
    t->externs.data = arena_alloc(&t->arena, sizeof(*t->externs.data) * p->externs.count);
    t->externs.count = p->externs.count;

    for (size_t i=0; i < p->externs.count; i++) {
      t->externs.data[i].cif = p->externs.items[i].cif;
      t->externs.data[i].name = arena_strdup(&t->arena, p->externs.items[i].name);
      t->externs.data[i].lib = p->externs.items[i].lib ? arena_strdup(&t->arena, p->externs.items[i].lib) : NULL;
      t->externs.data[i].access_state = p->externs.items[i].access_state;

#ifdef _WIN32
      LoadLibraryA(p->externs.items[i].lib);
#endif
    }
  }

  // global vars
  {
    t->globals.data  = arena_alloc(&t->arena, sizeof(*t->globals.data) * p->globals.count);
    t->globals.count = p->globals.count;

    for (size_t i=0; i < t->globals.count; i++) {
      t->globals.data[i] = p->globals.items[i];
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
    (v) = *(__typeof__((v))*)(--(vm)->sp);\
  } while(0)

#define PUSH(vm, v) \
  do {\
    if ((vm)->sp >= (vm)->active_task->stack + MAX_STACK) return VM_STACK_OVERFLOW; \
    *((vm)->sp++) = (__typeof__(*(vm)->sp))(v); \
  } while(0)

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
    case INST_DUP:
      a = *(vm->sp - 1);
      PUSH(vm, a);
      vm->ip++;
      break;
    case INST_SWAP:
      POP(vm, a);
      POP(vm, b);
      PUSH(vm, a);
      PUSH(vm, b);
      vm->ip++;
      break;
    case INST_LOAD:
      operand = *(++vm->ip);
      PUSH(vm, CURR_FRAME(vm)->vars[operand]);
      vm->ip++;
      break;
    case INST_LOADG:
      operand = *(++vm->ip);
      PUSH(vm, task->globals.data[operand]);
      vm->ip++;
      break;
    case INST_LOADC:
      operand = *(++vm->ip);
      PUSH(vm, task->consts.data[operand]); 
      vm->ip++;
      break;
    case INST_LOADF: {
      operand = *(++vm->ip);
      obj_handle_t handle;
      POP(vm, handle);
      obj_t* obj = get_obj(handle);
      PUSH(vm, obj->as.structure.fields[operand]);
      vm->ip++;
      break;
    }
    case INST_STORE:
      operand = *(++vm->ip);
      POP(vm, CURR_FRAME(vm)->vars[operand]);
      vm->ip++;
      break;
    case INST_STOREG:
      TODO("INST_STOREG");
      break;
    case INST_STOREF: {
      operand = *(++vm->ip);
      obj_handle_t handle;
      POP(vm, handle);
      obj_t* obj = get_obj(handle);
      POP(vm, obj->as.structure.fields[operand]);
      vm->ip++;
      break;
    }
    case INST_JMP:
      operand = *(vm->ip + 1);
      vm->ip += (int32_t)operand;
      break;
    case INST_JNZ:
      operand = *(vm->ip + 1);
      POP(vm, a);
      if (a != 0)
        vm->ip += (int32_t)operand;
      else
        vm->ip += 2;
      break;
    case INST_JZ:
      operand = *(vm->ip + 1);
      POP(vm, a);
      if (a == 0)
        vm->ip += (int32_t)operand;
      else
        vm->ip += 2;
      break;
    case INST_CALL:
      operand = *(++vm->ip);
      if (vm->active_task->call_stack.depth >= MAX_CALL_STACK) return VM_CALL_STACK_OVERFLOW;
      vm->active_task->call_stack.frames[vm->active_task->call_stack.depth++] = malloc(sizeof(virtual_frame_t));
      CURR_FRAME(vm)->ret_addr = vm->ip + 1;
      vm->ip = vm->active_task->code + operand;
      break;
    case INST_HOSTCALL: {
      // TODO: how do I handle refernces and output arguments?
      operand = *(++vm->ip);
      uint64_t retval = 0; 
      if (operand >= vm->active_task->externs.count) return VM_UNDEFINED_EXTERN;
      struct _extern* ext = &vm->active_task->externs.data[operand]; 

#ifdef _WIN32
      HMODULE lib = GetModuleHandleA(ext->lib);
      void* func = GetProcAddress(lib, ext->name);
#else
      void* lib = dlopen(ext->lib, RTLD_LAZY);
      void* func = dlsym(lib, ext->name);
#endif

      if (!func) {
        fprintf(stderr, "Cannot resolve function `%s`\n", ext->name);
        return VM_UNDEFINED_EXTERN; 
      }

      uint64_t** args   = malloc(sizeof(*args) * ext->cif.nargs);
      uint64_t*  values = malloc(sizeof(*args) * ext->cif.nargs);
      for(size_t i=0; i < ext->cif.nargs; i++) {
        if(is_ffi_arg_32(ext->cif.arg_types[i])) {
          POP(vm, values[i]);
        } else {
          uint32_t hi = 0;
          uint32_t lo = 0;
          POP(vm, hi);
          POP(vm, lo);
          values[i] = (((uint64_t)hi << 32) | lo);
        }
      }

      for(size_t j=0; j < ext->cif.nargs; j++) args[j] = &values[j];

      ffi_call(&ext->cif, FFI_FN(func), &retval, (void**)args);

      if (ext->cif.rtype != &ffi_type_void) {
        if(is_ffi_arg_32(ext->cif.rtype)) {
          PUSH(vm, (uint32_t)(retval & (uint32_t)-1));
        } else {
          uint32_t hi = retval >> 32;
          uint32_t lo = retval & (uint32_t)-1;
          PUSH(vm, lo);
          PUSH(vm, hi);
        }
      }

      free(values);
      free(args);
      vm->ip++;
      break;
    }
    case INST_RET:
      vm->ip = CURR_FRAME(vm)->ret_addr;
      free(vm->active_task->call_stack.frames[--vm->active_task->call_stack.depth]);
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
    case INST_MKOBJ:
      operand = *(++vm->ip);

      obj_t* o = malloc(sizeof(*o));
      o->kind = OBJ_STRUCT;
      o->as.structure.fields = malloc(sizeof(*o->as.structure.fields) * operand);
      o->as.structure.n_fields = operand;
      o->refs = 1;

      da_append(&vm->active_task->obj_pool, o);

      PUSH(vm, vm->active_task->obj_pool.count - 1);
      vm->ip++;
      break;
    case INST_HALT:
      free(vm->active_task);
      vm->active_task = NULL;
      return VM_OK;

    case INST_COUNT:
    default:
      return VM_INVALID_OPCODE;
  }
  return VM_NEXT;
}

void set_active_task(task_t* t) {
  assert(t);
  get_vm_instance()->active_task = t;
}

// TODO: handle non-uint32_t args
// -> have bind() macro/function that populates the stack before run()
vm_exitcode_t run(size_t n_args, ...) {
  va_list args;

  vm_t* vm = get_vm_instance();

  task_t* active = vm->active_task;
  vm->ip = active->code + 1; // skip halt
  vm->sp = active->stack;
  vm->halt = false;

  active->call_stack.frames[0] = malloc(sizeof(virtual_frame_t));
  active->call_stack.depth++;
  active->call_stack.frames[0]->ret_addr = active->code;

  va_start(args, n_args);
  for(size_t i=0; i < n_args; i++) {
    if (vm->sp >= active->stack + MAX_STACK) return VM_STACK_OVERFLOW;
    *(vm->sp++) = va_arg(args, uint32_t); 
  }
  va_end(args);

  vm_exitcode_t ec = VM_NEXT;
  // __dbg_print_stack(stdout, vm);
  while(!vm->halt) {
    // fprintf(stdout, "%p (0x%08lX) %d\n", vm->ip, (vm->ip - active->code), *vm->ip);
    ec = exec(vm);
    if (ec != VM_NEXT) break; 
    // __dbg_print_stack(stdout, vm);
  }

  // TODO: release resources

  return ec;
}
