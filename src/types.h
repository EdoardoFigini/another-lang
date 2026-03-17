#ifndef PROGRAM_H
#define PROGRAM_H
#include <stdint.h>

#include <ffi.h>

#include "arena.h"
#include "sb.h"
#include "slice.h"

#ifndef MAX_CALL_STACK
#define MAX_CALL_STACK 1024
#endif

#define MAX_LOC_VARS 4096

typedef enum {
  TOK_EOF = 256,
  TOK_IDENT,
  TOK_ARROW,
  TOK_COLCOL,
  TOK_EQEQ,
  TOK_GEQ,
  TOK_LEQ,
  TOK_INTLIT,
  TOK_REALLIT,
  TOK_STRLIT,
  TOK_RETURN,
  TOK_EXTERN,
  TOK_EXPORT,
  TOK_CONST,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_IMPL,
  TOK_INTERFACE,
} tok_kind_t;

typedef enum {
  KW_RETURN    = TOK_RETURN,
  KW_EXTERN    = TOK_EXTERN,
  KW_EXPORT    = TOK_EXPORT,
  KW_CONST     = TOK_CONST,
  KW_IF        = TOK_IF,
  KW_ELSE      = TOK_ELSE,
  KW_WHILE     = TOK_WHILE,
  KW_IMPL      = TOK_IMPL,
  KW_INTERFACE = TOK_INTERFACE,
} kw_kind_t;

typedef enum {
  TI_UNSIGNED = 1 << 0,
  TI_LONG     = 1 << 1,
  TI_REAL     = 1 << 2,
} lit_type_info_flags_t;

typedef struct {
  size_t line, col;
  slice_t line_view; 
  const char* path;
} loc_t;

typedef struct {
  tok_kind_t kind;
  lit_type_info_flags_t type_info;
  union {
    const char* s;
    double      r;
    uint64_t    u;
    int64_t     i;
  } as;
  slice_t view;
  loc_t loc;
} token_t;

typedef struct {
  token_t *items;
  size_t count;
  size_t capacity;
} tokenarr_t;

typedef struct {
  sb_t source;
  tokenarr_t tokens;
  arena_t arena;
  const char* path;
} tokenizer_t;

typedef struct _scope scope_t;

typedef struct {
  arena_t arena;
  tokenarr_t tokens;
  size_t current;
  scope_t* current_scope;
} parser_t;

static inline void parser_destroy(parser_t* p) {
  // tokens and source destroyed in tok_destroy
  arena_free(&p->arena);
}

typedef enum {
  AST_ROOT = 1,
  AST_EXPR,
  AST_STMT,
  AST_FUNC_DEF,
  AST_VAR_DEF,
  AST_ATTR_LIST,
  AST_TYPE,
  AST_BODY,
  AST_PARAM,
  AST_SIG,
  AST_IMPL,
  AST_IFACE,
} ast_node_kind_t;

#define AST_DEFAULT_FIELDS\
  ast_node_kind_t ast_kind; \
  loc_t loc

typedef struct {
  AST_DEFAULT_FIELDS;
} ast_node_t;

typedef enum {
  SPEC_NONE = 0,
  SPEC_EXTERN = 1 << 0,
  SPEC_EXPORT = 1 << 1,
  SPEC_CONST  = 1 << 2,
} spec_flags_t;

typedef struct _type type_t;

typedef enum {
  SYMB_VAR,
  SYMB_FUNC,
  SYMB_TYPE,
} symb_kind_t;

typedef enum {
  STO_GLOBAL,
  STO_LOCAL,
  STO_EXTERN,
  STO_EXPORT,
} symb_storage_t;

typedef struct {
  const char* name;
  symb_kind_t kind;
  symb_storage_t storage;
  type_t* type;
  uint32_t addr;
  bool addr_resolved;
} symbol_t;

struct _scope {
  const char* name;
  struct _scope* parent;
  // TODO: OPTIMIZE
  // transform into hashmap
  struct {
    symbol_t** items;
    size_t count;
    size_t capacity;
  } symbols;
};

typedef struct _ast_expr_t ast_expr_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  int array_depth;
  type_t* resolved_type;
} ast_type_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  ast_type_t* type;
  symbol_t* symbol;
} ast_param_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  struct {
    ast_param_t** items;
    size_t count;
    size_t capacity;
  } params;
  ast_type_t* ret;
  type_t* resolved_type;
} ast_sig_t;

typedef struct _ast_body_t ast_body_t;

struct _attribute {
  const char* key;
  const char* value;
}; 

typedef struct {
  AST_DEFAULT_FIELDS;
  struct {
    struct _attribute* items;
    size_t count;
    size_t capacity;
  } attrs;
} ast_attr_list_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  spec_flags_t flags;
  symbol_t* symbol;
  ast_sig_t* sig;
  scope_t* scope;
  ast_body_t* body;
  ast_attr_list_t* attributes;
} ast_func_def_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  spec_flags_t flags;
  symbol_t* symbol;
  ast_type_t* type;
  ast_expr_t* init;
  ast_attr_list_t* attributes;
} ast_var_def_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  scope_t* scope;
  ast_type_t* type;
  ast_type_t* interface;
  struct {
    ast_func_def_t** items;
    size_t count;
    size_t capacity;
  } methods;
} ast_impl_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  struct {
    ast_func_def_t** items;
    size_t count;
    size_t capacity;
  } methods;
} ast_iface_t;

typedef enum {
  OP_INVALID = TOK_EOF,
  OP_PLUS    = '+',
  OP_MINUS   = '-',
  OP_MULT    = '*',
  OP_DIV     = '/',
  OP_REM     = '%',
  OP_CALL    = '(',
  OP_ASSIGN  = '=',
  OP_EQ      = TOK_EQEQ,
  OP_LEQ     = TOK_LEQ,
  OP_GEQ     = TOK_GEQ,
  OP_LT      = '<',
  OP_GT      = '>',
  OP_MEMB    = '.',
  OP_SCOPE   = TOK_COLCOL,
} op_kind_t;

enum _bp {
  BP_NONE = 0,
  BP_ASSIGN,
  BP_EQ,
  BP_REL,
  BP_ADD,
  BP_MULT,
  BP_CALL,
  BP_ACCESS,
};

typedef enum {
  EXPR_SYMBOL = 0,
  EXPR_STRING,
  EXPR_NUMBER,
  EXPR_BINOP,
  EXPR_UNOP,
  EXPR_ACCESS,
  EXPR_FUNCALL,
  EXPR_SUBEXPR,
  EXPR_ASSIGNMENT,
} ast_expr_kind_t;

struct _ast_expr_t {
  AST_DEFAULT_FIELDS;
  ast_expr_kind_t kind;
  type_t const* type;
  bool is_const;
  union {
    const char* symbol;
    const char* s;
    struct {
      lit_type_info_flags_t ti;
      union {
        uint64_t u;
        int64_t i;
        double r;
      };
    } number;
    struct {
      ast_expr_t* lhs;
      ast_expr_t* rhs;
      op_kind_t op;
    } binop;
    struct {
      ast_expr_t* operand;
      op_kind_t op;
    } unop;
    struct {
      ast_expr_t* owner;
      const char* field;
      op_kind_t op;
    } access;
    struct { 
      ast_expr_t* callee;
      struct _args {
        ast_expr_t** items;
        size_t count;
        size_t capacity;
      } args;
    } funcall;
    struct {
      ast_expr_t* lhs;
      ast_expr_t* rhs;
    } assign;
    ast_expr_t* subexpr;
  } as;
};

typedef enum {
  STMT_EMPTY = 0,
  STMT_RET,
  STMT_EXPR,
  STMT_VAR_DEF,
  STMT_IF,
  STMT_WHILE,
} ast_stmt_kind_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  ast_stmt_kind_t kind;
  union {
    ast_expr_t* expression;
    ast_expr_t* retval;
    ast_var_def_t* var_def;
    struct _if_else {
      ast_expr_t* cond;
      scope_t* if_scope;
      ast_body_t* if_body;
      scope_t* else_scope;
      ast_body_t* else_body;
    } if_else;
    struct _while {
      ast_expr_t* cond;
      scope_t* scope;
      ast_body_t* body;
    } while_loop;
  } as;
} ast_stmt_t;

struct _ast_body_t {
  AST_DEFAULT_FIELDS;
  struct {
    ast_stmt_t** items;
    size_t count;
    size_t capacity;
  } stmts;
};

typedef struct {
  AST_DEFAULT_FIELDS;
  struct {
    ast_func_def_t** items;
    size_t count;
    size_t capacity;
  } func_defs;
  struct {
    ast_var_def_t** items;
    size_t count;
    size_t capacity;
  } var_defs;
  struct {
    ast_impl_t** items;
    size_t count;
    size_t capacity;
  } impls;
  struct {
    ast_iface_t** items;
    size_t count;
    size_t capacity;
  } interfaces;
  scope_t* scope;
} ast_root_t;

typedef enum {
  TYPE_NONE,

  // only 32 (normal - word) and 64 (long - double word)
  TYPE_I32,
  TYPE_I64,
  
  TYPE_U32,
  TYPE_U64,
  
  TYPE_F32,
  TYPE_F64,

  TYPE_BOOL,
  
  TYPE_CHAR,
  TYPE_STR,

  TYPE_ARRAY,
  
  TYPE_ADDR, // akin to void* in C
  
  TYPE_STRUCT,
  TYPE_MODULE,
  
  TYPE_ALIAS,

  TYPE_FUNC,
  TYPE_INTERFACE,

  TYPES_COUNT,
} type_kind_t;

typedef struct _method method_t;

typedef struct {
  const char* name;
  const type_t* type;
} interface_method_t;

struct _type {
  const char* name;
  size_t size;
  type_kind_t kind;
  union {
    struct {
      void* __todo;
      // TODO: fields
    } structure;
    struct {
      struct _type* inner;
      size_t size;
    } array;
    struct {
      struct _type* ret;
      struct {
        struct _type** items;
        size_t count;
        size_t capacity;
      } params;
    } func;
    struct {
      struct _type* target;
    } alias; 
    struct {
      const char* name;
      struct _i_methods {
        interface_method_t* items;
        size_t count;
        size_t capacity;
      } methods;
    } interface;
  } as;
  scope_t* scope;
  struct {
    const char** items;
    size_t count;
    size_t capacity;
  } impls;
};

struct _method {
  const type_t* owner;
  const char* name;
  const type_t* type;
  uint32_t addr;
};

typedef struct {
  arena_t arena;
  struct {
    type_t** items;
    size_t count;
    size_t capacity;
  } custom_types; // use for interning
  struct {
    type_t const* none;
    type_t const* i32;
    type_t const* i64;
    type_t const* u32;
    type_t const* u64;
    type_t const* f32;
    type_t const* f64;
    type_t const* boolean;
    type_t const* character;
    type_t const* str;
    type_t const* array;
    type_t const* addr;
  } builtins;
  scope_t* current_scope;
} typechecker_t;

typedef enum {
  RK_BOOL = 0,
  RK_U32,
  RK_I32,
  RK_U64,
  RK_I64,
  RK_F32,
  RK_F64,
} num_rank_t;

typedef enum {
  IMPL_BINOP,
  IMPL_FUNC
} impl_kind_t;

typedef struct {
  type_t* owner;
  impl_kind_t kind;
  union {
    struct {
      op_kind_t op;
      type_t* other;
    } binop;
    struct {
      void* __todo;
    } func;
  } as;
} i_method_t;

typedef struct {
  const char* name;
  size_t loc_vars;
} frame_t;


typedef enum {
  DK_STR = 0,
  DK_NUMBER
} data_kind_t;

typedef union {
  const char* s;
  struct {
    lit_type_info_flags_t ti;
    union {
      uint64_t u;
      int64_t i;
      double r;
    };
  } number;
} data_t;

typedef struct {
  data_kind_t kind;
  data_t as;
} constant_t;

typedef uint32_t instruction_t; enum {
  INST_NOP = 0,

  INST_PUSH,
  // INST_PUSHL, // long
  INST_POP,
  // INST_POPL,

  INST_LOAD,
  INST_LOADG,
  INST_LOADC,
  INST_STORE,
  INST_STOREG,

  INST_JMP,
  INST_JNZ,
  INST_JZ,
  INST_CALL,
  INST_HOSTCALL,
  INST_ICALL, // interface call
  INST_RET,

  INST_ADD,
  INST_SUB,
  INST_MULT,
  INST_DIVI,
  INST_DIVU,
  INST_REM,
  INST_ADDF,
  INST_SUBF,
  INST_MULTF,
  INST_DIVF,

  INST_EQ,
  INST_LEQ,
  INST_GEQ,
  INST_LT,
  INST_GT,

  INST_HALT,

  INST_COUNT,
};

typedef struct {
  instruction_t* items;
  size_t count;
  size_t capacity;
} instrarr_t;

typedef struct {
  instrarr_t code;
  struct _consts {
    constant_t* items;
    size_t count;
    size_t capacity;
  } constants;
  struct _glob {
    data_t* items;
    size_t count;
    size_t capacity;
  } globals;
  struct _externs {
    struct _extern{ 
      ffi_cif cif;
      const char* name;
      const char* lib;
      bool access_state;
    }* items;
    size_t count;
    size_t capacity;
  } externs;
  struct _patches {
    struct _patch {
      uint32_t addr;
      symbol_t* symbol; 
    }* items;
    size_t count;
    size_t capacity;
  } patches;
} program_t;

// runtime
typedef enum {
  OBJ_STR,
  OBJ_STRUCT,
} obj_kind_t;

typedef struct _obj {
  obj_kind_t kind;
  size_t refs;
  union {
    struct _obj_str {
      const char* data;
      size_t size;
    } str;
  } as;
} obj_t;

typedef struct {
  obj_t** items;
  size_t count;
  size_t capacity;
} obj_pool_t;

typedef uint32_t obj_handle_t;

typedef struct {
  uint32_t vars[MAX_LOC_VARS];
  instruction_t* ret_addr;
} virtual_frame_t;

typedef struct _task {
  instruction_t* code;
  const instruction_t* last_inst;
  arena_t arena;

  // probably useless: first objects in the OBJ pool are the constants
  // this is due to the way we load a new task
  struct _vm_consts {
    uint32_t* data;
    size_t count;
  } consts;

  uint32_t* stack;
  struct {
    // NOTE: could use an arena and instead of malloc'ing a new
    // frame each time I just rewind the allocator.
    virtual_frame_t* frames[MAX_CALL_STACK];
    size_t depth;
  } call_stack;

  struct {
    struct _extern* data;
    size_t count;
  } externs;

  obj_pool_t obj_pool;
  
  // TODO: remove reference onc I add each needed field in task struct 
  program_t* program;

  bool alive;

  struct _task* next;
  struct _task* prev;
} task_t;

typedef struct {
  uint32_t* sp;
  instruction_t* ip;

  bool halt;

  arena_t tasks_arena;
  task_t* tasks_head;
  task_t* active_task;

  void* host;
} vm_t;

// TODO: allow users to define handlers for exit codes 
typedef enum {
  VM_OK,
  VM_NEXT,
  VM_INVALID_OPCODE,
  VM_NO_MORE_INSTRUCTIONS,
  VM_STACK_OVERFLOW,
  VM_STACK_UNDERFLOW,
  VM_CALL_STACK_OVERFLOW,
  VM_UNDEFINED_EXTERN,
} vm_exitcode_t;

#endif
