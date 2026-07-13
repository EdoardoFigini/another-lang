#ifndef PROGRAM_H
#define PROGRAM_H
#include <stdint.h>

#include <ffi.h>

#include "arena.h"
#include "vec.h"
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
  TOK_MOD,
  TOK_STRUCT,
  TOK_TYPE,
  TOK_NEW,
  TOK_IMPORT,
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
  KW_MOD       = TOK_MOD,
  KW_STRUCT    = TOK_STRUCT,
  KW_TYPE      = TOK_TYPE,
  KW_NEW       = TOK_NEW,
} kw_kind_t;

typedef enum {
  CT_IMPORT = TOK_IMPORT,
} comptime_kind_t;

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

typedef VEC(token_t) tokenarr_t;

typedef struct {
  slice_t source;
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


typedef enum {
  AST_ROOT = 1,
  AST_QN,
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
  AST_MOD,
  AST_STRUCT,
  AST_TYPEDEF,
  AST_IMPORT,
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
  STO_INSTANCE,
} symb_storage_t;

typedef union {
  type_t* type;
  // TODO: compile-time constants
} compile_time_value_t;

typedef struct {
  const char* name;
  symb_kind_t kind;
  symb_storage_t storage;
  type_t* type;
  compile_time_value_t value;
  uint32_t addr;
  bool addr_resolved;
} symbol_t;

struct _scope {
  const char* name;
  struct _scope* parent;
  // TODO: OPTIMIZE
  // transform into hashmap
  VEC(symbol_t*) symbols;
};

typedef struct _ast_expr_t ast_expr_t;

typedef struct _ast_qn {
  AST_DEFAULT_FIELDS;
  const char* name;
  struct _ast_qn* next;
} ast_qn_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  ast_qn_t* name;
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
  VEC(ast_param_t*) params;
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
  VEC(struct _attribute) attrs;
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
  VEC(ast_func_def_t*) methods;
} ast_impl_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  VEC(ast_func_def_t*) methods;
} ast_iface_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  VEC(ast_param_t*) fields;
  type_t* self_type;
} ast_struct_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  ast_type_t* alias;
} ast_typedef_t;

typedef struct _ast_root ast_root_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  ast_root_t* root;
  scope_t* scope;
} ast_mod_t;

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
  OP_NUMOF   = '#',
  OP_NOT     = '!',
  OP_INDEX   = '[',
} op_kind_t;

enum _bp {
  BP_NONE = 0,
  BP_ASSIGN,
  BP_EQ,
  BP_REL,
  BP_ADD,
  BP_MULT,
  BP_UNARY,
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
  EXPR_MKOBJ,
  EXPR_MKARR,
  EXPR_INDEX,
  EXPR_CAST,
} ast_expr_kind_t;

struct _ast_expr_t {
  AST_DEFAULT_FIELDS;
  ast_expr_kind_t kind;
  type_t const* type;
  bool is_const;
  union {
    struct {
      ast_qn_t* name;
      symbol_t* resolved_symbol;
    } symbol;
    const char* s;
    struct {
      lit_type_info_flags_t ti;
      // TODO: this should be in compile_time_constant_t
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
      symbol_t* field_symbol;
    } access;
    struct { 
      ast_expr_t* callee;
      VEC(ast_expr_t*) args;
    } funcall;
    struct {
      ast_expr_t* lhs;
      ast_expr_t* rhs;
    } assign;
    struct {
      ast_expr_t* type;
      VEC(struct _mkobj_field {
        const char* field;
        ast_expr_t* value;
        symbol_t* symbol;
      }) fields;
    } mkobj;
    struct {
      VEC(ast_expr_t*) elems;
      ast_expr_t* repeat;
    } mkarr;
    struct {
      ast_expr_t* expr;
      ast_expr_t* idx;
    } index;
    ast_expr_t* subexpr;
    struct {
      ast_expr_t* expr;
      // can be NULL in the case of implicit cast.
      // the target will be expression->type.
      ast_expr_t* target_type;
    } cast;
  } as;
  compile_time_value_t value;
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
  VEC(ast_stmt_t*) stmts;
};

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name; 
} ast_import_t; 


struct _ast_root {
  AST_DEFAULT_FIELDS;
  VEC(ast_func_def_t*) func_defs;
  VEC(ast_var_def_t*) var_defs;
  VEC(ast_impl_t*) impls;
  VEC(ast_iface_t*) interfaces;
  VEC(ast_mod_t*) submods;
  VEC(ast_struct_t*) structs;
  VEC(ast_typedef_t*) typedefs;
  VEC(ast_import_t*) imports;
  scope_t* scope;
};

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

  TYPE_TYPE,

  TYPES_COUNT,
} type_kind_t;

typedef struct _method method_t;

typedef struct {
  const char* name;
  const type_t* type;
} interface_method_t;

typedef struct {
  VEC(struct _type*) fields;
  size_t obj_size;
} type_structure_t;

typedef struct {
  struct _type* inner;
  size_t size;
} type_array_t;

typedef struct {
  struct _type* ret;
  VEC(struct _type*) params;
} type_func_t;

typedef struct {
  struct _type* target;
} type_alias_t;

typedef struct {
  const char* name;
  VEC(interface_method_t) methods;
} type_interface_t;

typedef struct {
  struct _type* of;
} type_type_t;

struct _type {
  const char* name;
  size_t size;
  type_kind_t kind;
  union {
    type_structure_t structure;
    type_array_t     array;
    type_func_t      func;
    type_alias_t     alias;
    type_interface_t interface;
    type_type_t      type;
  } as;
  scope_t* scope;
  // TODO: make hashmap<ast_qn_t*, bool>
  VEC(ast_qn_t*) impls;
};

struct _method {
  const type_t* owner;
  const char* name;
  const type_t* type;
  uint32_t addr;
};

typedef struct {
  arena_t arena;
  VEC(type_t*) custom_types; // use for interning
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
    type_t const* type;

    scope_t* scope;
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
  INST_DUP,
  INST_SWAP,

  INST_LOAD,   // load local var
  INST_LOADG,  // load global var
  INST_LOADC,  // load constant
  INST_LOADF,  // load field
  INST_LOADI,  // load item
  INST_STORE,  // store local var
  INST_STOREG, // store global var
  INST_STOREF, // store field
  INST_STOREI, // store item
 
  INST_JMP,
  INST_JNZ,
  INST_JZ,
  INST_CALL,
  INST_HOSTCALL,
  INST_RET,

  INST_FTOI,
  INST_ITOF,

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
  INST_LNOT,

  INST_MKOBJ,

  INST_HALT,

  INST_COUNT,
};

typedef VEC(instruction_t) instrarr_t;

typedef struct {
  arena_t arena;
  instrarr_t code;
  struct _consts {
    __VEC_HEADER__(constant_t);
  } constants;
  struct _glob {
    __VEC_HEADER__(uint32_t);
  } globals;
  struct _exports {
    __VEC_HEADER__(
      struct _export {
        const char* name;
        uint32_t addr;
      }
    );
  } exports;
  struct _externs {
    __VEC_HEADER__(
      struct _extern{ 
        ffi_cif cif;
        const char* name;
        const char* lib;
        bool access_state;
      }
    );
  } externs;
  struct _patches {
    __VEC_HEADER__(
      struct _patch {
        uint32_t addr;
        symbol_t* symbol; 
      }
    );
  } patches;
  VEC(const char*) imports;
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
    struct _obj_struct {
      uint32_t *fields;
      size_t n_fields;
    } structure;
  } as;
} obj_t;

typedef uint32_t obj_handle_t;

typedef struct _handle_node {
  obj_handle_t handle;
  struct _handle_node *next;
} handle_node_t;

typedef struct {
  __VEC_HEADER__(obj_t);
  handle_node_t* freelist;
} obj_pool_t;

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

  struct _task_globals {
    uint32_t* data;
    size_t count;
  } globals;

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

  struct {
    struct _export* data;
    size_t count;
  } exports;

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

  const int64_t max_stack;
} vm_t;

// TODO: allow users to define handlers for exit codes 
typedef enum {
  VM_OK,
  VM_NEXT,
  VM_INVALID_OPCODE,
  VM_INVALID_HANDLE,
  VM_NO_MORE_INSTRUCTIONS,
  VM_STACK_OVERFLOW,
  VM_STACK_UNDERFLOW,
  VM_CALL_STACK_OVERFLOW,
  VM_UNDEFINED_EXTERN,
  VM_UNDEFINED_EXPORT,
  VM_OUT_OF_RANGE,
} vm_exitcode_t;

#endif
