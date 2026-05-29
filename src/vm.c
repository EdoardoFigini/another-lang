#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>

#include <ffi.h>

#include "arena.h"
#include "sb.h"
#include "vec.h"

#include "macros.h"
#include "types.h"
#include "debug.h"

#include "vm.h"

#define MAX_STACK 0x1000 
#define HANDLE_INVALID ((obj_handle_t)-1)

// TODO: add platform dependent mutex to vm_t
static vm_t __vm = { .max_stack = MAX_STACK };

vm_t* get_vm_instance() {
  // TODO: make thread safe (should block)
  return &__vm;
}

task_t* get_active_task() {
  vm_t* vm = get_vm_instance();
  task_t* active = vm->active_task;
  if (!active) {
    fprintf(stderr, "\033[41;1m[FATAL]\033[0m No active task!\n"); 
    abort();
  }
  return active;
}

void set_active_task(task_t* t) {
  if (!t) {
    fprintf(stderr, "\033[31;1m[ERROR]\033[0m No task provided\n"); 
    return;
  }
  get_vm_instance()->active_task = t;
}

obj_t* get_obj(obj_handle_t h) {
  const vm_t* vm = get_vm_instance();
  if (h == HANDLE_INVALID) return NULL;
  return &vec_get(&vm->active_task->obj_pool, h);
}

static inline obj_handle_t make_obj_str(obj_pool_t* pool, const char* s) {
  obj_handle_t ret = HANDLE_INVALID;
  if (pool->freelist) {
    struct _handle_node* tmp = (pool->freelist = pool->freelist->next); 
    ret = tmp->handle;
    free(tmp);
  } else {
    ret = pool->count;
    vec_push(pool, (obj_t){ 0 });
  }

  vec_get(pool, ret).kind = OBJ_STR;
  vec_get(pool, ret).as.str.data = strdup(s);
  vec_get(pool, ret).as.str.size = strlen(s);
  vec_get(pool, ret).refs = 0;

  return ret;
}

FMT_PRINTF(2, 3) static inline obj_handle_t make_obj_strf(obj_pool_t* pool, const char* fmt, ...) {
  va_list args;

  obj_handle_t ret = HANDLE_INVALID;
  if (pool->freelist) {
    struct _handle_node* tmp = (pool->freelist = pool->freelist->next); 
    ret = tmp->handle;
    free(tmp);
  } else {
    ret = pool->count;
    vec_push(pool, (obj_t){ 0 });
  }

  va_start(args, fmt);
  int len = 0;
  char* s = NULL;
  len = vsnprintf(s, len    , fmt, args);
  s = malloc(len + 1);
  len = vsnprintf(s, len + 1, fmt, args);
  va_end(args);

  vec_get(pool, ret).kind = OBJ_STR;
  vec_get(pool, ret).as.str.data = s;
  vec_get(pool, ret).as.str.size = len;
  vec_get(pool, ret).refs = 0;

  return ret;
}

static inline obj_handle_t make_obj_struct(obj_pool_t* pool, size_t size) {
  obj_handle_t ret = HANDLE_INVALID;
  if (pool->freelist) {
    struct _handle_node* tmp = (pool->freelist = pool->freelist->next); 
    ret = tmp->handle;
    free(tmp);
  } else {
    ret = pool->count;
    vec_push(pool, (obj_t){ 0 });
  }

  vec_get(pool, ret).kind = OBJ_STRUCT;
  vec_get(pool, ret).as.structure.fields = malloc(sizeof(uint32_t) * size);
  vec_get(pool, ret).as.structure.n_fields = size;
  vec_get(pool, ret).refs = 0;

  return ret;
}

static inline void release_obj(obj_handle_t handle);

static inline void destroy_obj(obj_handle_t handle) {
  if (handle == HANDLE_INVALID) return;

  obj_pool_t* pool = &get_active_task()->obj_pool;

  obj_t* obj = &vec_get(pool, handle);
  switch(obj->kind) {
    case OBJ_STR:
      free((void*)obj->as.str.data);
      break;
    case OBJ_STRUCT:
      for (size_t i = 0; i < obj->as.structure.n_fields; i++)
        release_obj(obj->as.structure.fields[i]); 
      free(vec_get(pool, handle).as.structure.fields);
      break;
    default:
      UNREACHABLE("destroy - unknown obj kind");
  }
  memset(obj, 0, sizeof(obj_t));

  struct _handle_node* node = malloc(sizeof(*node));
  node->handle = handle;
  node->next = pool->freelist;
  pool->freelist = node; 
}

static inline void release_obj(obj_handle_t handle) {
  if (handle == HANDLE_INVALID) return;
  obj_pool_t pool = get_active_task()->obj_pool;
  if (--vec_get(&pool, handle).refs <= 0) destroy_obj(handle);
}

static inline obj_handle_t acquire_obj(obj_handle_t handle) {
  if (handle != HANDLE_INVALID)
    ++vec_get(&get_active_task()->obj_pool, handle).refs;
  return handle;
}

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

// BUILTIN FUNCTION

EXPORT int print(const char* s) {
  return puts(s);
}

EXPORT int str_cmp(const char* a, const char* b) {
  return strcmp(a, b);
}

// BUILTIN METHODS

EXPORT const char* str__c_str(obj_handle_t self) {
  acquire_obj(self);
  // TODO: duplicate data
  const char* ret = get_obj(self)->as.str.data; 
  release_obj(self);

  return ret;
}

EXPORT int str__length(obj_handle_t self) {
  acquire_obj(self);
  int ret = (int)get_obj(self)->as.str.size;
  release_obj(self);

  return ret;
}

EXPORT obj_handle_t str__concat(obj_handle_t self, obj_handle_t other) {
  obj_t* str_self  = get_obj(acquire_obj(self));
  obj_t* str_other = get_obj(acquire_obj(other));

  obj_handle_t h = make_obj_strf(
      &get_active_task()->obj_pool,
      "%.*s%.*s",
      (int)str_self->as.str.size,  str_self->as.str.data,
      (int)str_other->as.str.size, str_other->as.str.data
  );

  release_obj(self);
  release_obj(other);

  return acquire_obj(h); // stack acquires on return
}

EXPORT obj_handle_t i32__to_str(int32_t self) {
  obj_handle_t h = make_obj_strf(&get_active_task()->obj_pool, "%d", self);

  return acquire_obj(h); // stack acquires on return
}

EXPORT obj_handle_t u32__to_str(uint32_t self) {
  obj_handle_t h = make_obj_strf(&get_active_task()->obj_pool, "%u", self);

  return acquire_obj(h); // stack acquires on return
}

EXPORT obj_handle_t f32__to_str(float self) {
  obj_handle_t h = make_obj_strf(&get_active_task()->obj_pool, "%f", self);

  return acquire_obj(h); // stack acquires on return
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
  void* pages = VirtualAlloc(NULL, vec_data_size(&p->code), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if(!pages) return NULL;
  CopyMemory(pages, vec_data(&p->code), vec_data_size(&p->code));
  t->code = pages;
  t->last_inst = t->code + (vec_len(&p->code) - 1) * vec_elem_size(&p->code);
  DWORD old_prot = 0;
  VirtualProtect(t->code, (vec_len(&p->code) - 1) * vec_elem_size(&p->code), PAGE_READONLY, &old_prot);

  pages = VirtualAlloc(NULL, MAX_STACK, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if(!pages) return NULL;
  t->stack = pages;
#elif defined(__linux__)
  void* pages = mmap(NULL, vec_data_size(&p->code), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(!pages) return NULL;
  memcpy(pages, p->code.items, vec_data_size(&p->code));
  t->code = pages;
  t->last_inst = t->code + (vec_len(&p->code) - 1) * vec_elem_size(&p->code);
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
      constant_t* c = &vec_get(&p->constants, i);
      switch(c->kind) {
        case DK_STR:
          obj_handle_t h = make_obj_str(&t->obj_pool, c->as.s);
          ++vec_get(&t->obj_pool, h).refs;
          t->consts.data[i] = h;
          free((void*)c->as.s);
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
      t->externs.data[i].cif = vec_get(&p->externs, i).cif;
      t->externs.data[i].name = arena_strdup(&t->arena, vec_get(&p->externs, i).name);
      t->externs.data[i].lib = vec_get(&p->externs, i).lib ? arena_strdup(&t->arena, vec_get(&p->externs, i).lib) : NULL;
      t->externs.data[i].access_state = vec_get(&p->externs, i).access_state;

#ifdef _WIN32
      LoadLibraryA(vec_get(&p->externs, i).lib);
#endif
    }
  }

  // exports
  {
    t->exports.data = arena_alloc(&t->arena, sizeof(*t->exports.data) * p->exports.count);
    t->exports.count = p->exports.count;

    for (size_t i=0; i < p->exports.count; i++) {
      t->exports.data[i].name = arena_strdup(&t->arena, vec_get(&p->exports, i).name);
      t->exports.data[i].addr = vec_get(&p->exports, i).addr;
    }
  }

  // global vars
  {
    t->globals.data  = arena_alloc(&t->arena, sizeof(*t->globals.data) * p->globals.count);
    t->globals.count = p->globals.count;

    for (size_t i=0; i < t->globals.count; i++) {
      t->globals.data[i] = vec_get(&p->globals, i);
    }
  }

  DLLIST_ADD(t, vm->tasks_head);
  return t;
}

#define CURR_FRAME(vm) \
  (vm)->active_task->call_stack.frames[(vm)->active_task->call_stack.depth - 1]

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
      if (handle == HANDLE_INVALID) return VM_INVALID_HANDLE;
      obj_t* obj = get_obj(handle);
      PUSH(vm, obj->as.structure.fields[operand]);
      // release_obj(handle);
      vm->ip++;
      break;
    }
    case INST_LOADI: {
      POP(vm, a);
      obj_handle_t handle;
      POP(vm, handle);
      if (handle == HANDLE_INVALID) return VM_INVALID_HANDLE;
      obj_t* obj = get_obj(handle);
      if (a > obj->as.structure.n_fields) return VM_OUT_OF_RANGE;
      PUSH(vm, obj->as.structure.fields[a]);
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
      if (handle == HANDLE_INVALID) return VM_INVALID_HANDLE;
      obj_t* obj = get_obj(handle);
      POP(vm, obj->as.structure.fields[operand]);
      // release_obj(handle);
      vm->ip++;
      break;
    }
    case INST_STOREI: {
      POP(vm, a);
      obj_handle_t handle;
      POP(vm, handle);
      if (handle == HANDLE_INVALID) return VM_INVALID_HANDLE;
      obj_t* obj = get_obj(handle);
      if (a > obj->as.structure.n_fields) return VM_OUT_OF_RANGE;
      POP(vm, obj->as.structure.fields[a]);
      // release_obj(handle);
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
      memset(CURR_FRAME(vm)->vars, 0xFF, MAX_LOC_VARS * sizeof(*CURR_FRAME(vm)->vars));
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
    case INST_LNOT:
      POP(vm, a);
      PUSH(vm, !a);
      vm->ip++;
      break;
    case INST_MKOBJ:
      operand = *(++vm->ip);
      obj_handle_t h = make_obj_struct(&get_active_task()->obj_pool, operand);
      PUSH(vm, acquire_obj(h));
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

// TODO: handle non-uint32_t args
// -> have bind() macro/function that populates the stack before call()
vm_exitcode_t call(const char* fn, size_t n_args, ...) {
  va_list args;

  vm_t* vm = get_vm_instance();

  task_t* active = vm->active_task;
  vm->ip = active->code;
  vm->sp = active->stack;
  vm->halt = false;

  bool found = false;
  for (size_t i=0; !found && i < active->exports.count; i++) {
    if ((found = strcmp(fn, active->exports.data[i].name) == 0))
      vm->ip = active->code + active->exports.data[i].addr; 
  }
  if (!found) return VM_UNDEFINED_EXPORT;

  active->call_stack.frames[0] = malloc(sizeof(virtual_frame_t));
  memset(active->call_stack.frames[0]->vars, 0xFF, MAX_LOC_VARS * sizeof(*active->call_stack.frames[0]->vars));
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
    // fprintf(stdout, "%p (0x%08llX)\n", vm->ip, (vm->ip - active->code));
    // __dbg_print_disass_inst(stdout, vm->ip);
    ec = exec(vm);
    if (ec != VM_NEXT) break; 
    // __dbg_print_stack(stdout, vm);
  }

  // TODO: release resources

  return ec;
}
