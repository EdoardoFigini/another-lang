#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>

#include <ffi.h>

#define SB_IMPLEMENTATION
#include "sb.h"
#define DA_IMPLEMENTATION
#include "da.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"

#include "bytecode.h"

#define UNREACHABLE(fmt, ...) \
  do { \
    fprintf(stderr, "[FATAL]: UNREACHABLE\n  %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);\
    abort();\
  } while(0);

#define TODO(fmt, ...) \
  do { \
    fprintf(stderr, "[FATAL]: UNIMPLEMENTED\n  %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);\
    abort();\
  } while(0);

#define DIAGF(p, lvl, fmt, ...) \
  do { \
    char* line_start = (char*)(p)->tokens.items[(p)->current].start; \
    char* line_end   = (char*)(p)->tokens.items[(p)->current].start; \
    while(line_start != (p)->source.items && *(line_start-1) != '\n') line_start--; \
    while(line_end   != &da_last(&(p)->source) && *line_end != '\n')  line_end++;   \
    int line_len = line_end - line_start;\
    fprintf( \
        stderr, \
        "[" #lvl "]\n  %zu:%zu: " fmt "\n    %.*s\n    %*s^\n\n", \
        (p)->line, (p)->col, \
        ## __VA_ARGS__, \
        line_len, line_start, \
        (int)(p)->col-1, "" \
    );\
  } while(0);

const char* source = 
  "extern puts: (s: char[]) -> i32;\n"
  "\n"
  "export main : () -> i64 {\n"
  " puts(\"Hello World!\\n\");\n"
  " return 1 + 2 / 3 - 4 * ( 5l + 6 * 7 ) / 9L;\n"
  "}\n"
  ;

typedef enum {
  TOK_EOF = 256,
  TOK_IDENT,
  TOK_ARROW,
  TOK_COLCOL,
  TOK_INTLIT,
  TOK_REALLIT,
  TOK_STRLIT,
  TOK_RETURN,
  TOK_EXTERN,
  TOK_EXPORT,
} tok_kind_t;

typedef enum {
  KW_RETURN = TOK_RETURN,
  KW_EXTERN = TOK_EXTERN,
  KW_EXPORT = TOK_EXPORT,
} kw_kind_t;

const char* tok_keywords[] = {
  [KW_RETURN] = "return",
  [KW_EXTERN] = "extern",
  [KW_EXPORT] = "export",
};

typedef enum {
  TI_UNSIGNED = 1 << 0,
  TI_LONG     = 1 << 1,
  TI_REAL     = 1 << 2,
} lit_type_info_flags_t;

typedef struct {
  tok_kind_t kind;
  const char* start;
  int len;
  lit_type_info_flags_t type_info;
  union {
    const char* s;
    double      r;
    uint64_t    u;
    int64_t     i;
  } as;
  size_t line;
  size_t col;
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
} tokenizer_t;

void tok_print(token_t tok) {
  printf("%zu:%zu: ", tok.line, tok.col);
  if(tok.kind < 256) printf("%-20c", tok.kind);
  else {
    switch(tok.kind) {
      case TOK_EOF: printf("%-20s", "TOK_EOF"); break;
      case TOK_IDENT: printf("%-20s", "TOK_IDENT"); break;
      case TOK_ARROW: printf("%-20s", "TOK_ARROW"); break;
      case TOK_COLCOL: printf("%-20s", "TOK_COLCOL"); break;
      case TOK_INTLIT: printf("%-20s", "TOK_INTLIT"); break;
      case TOK_REALLIT: printf("%-20s", "TOK_REALLIT"); break;
      case TOK_STRLIT: printf("%-20s", "TOK_STRLIT"); break;
      case TOK_RETURN: printf("%-20s", "TOK_RETURN"); break;
      case TOK_EXTERN: printf("%-20s", "TOK_EXTERN"); break;
      case TOK_EXPORT: printf("%-20s", "TOK_EXPORT"); break;
      default: printf("%-20s", "<INVALID TOKEN>"); break;
    }
  }
  printf(": `%.*s`\n", tok.len, tok.start);
};

const char* tok_kind_str(arena_t* arena, tok_kind_t k) {
  if(k < 256) return arena_sprintf(arena, "`%c`", k);
  else {
    switch(k) {
      case TOK_EOF: return "EOF";
      case TOK_IDENT: return "identifier";
      case TOK_ARROW: return "->";
      case TOK_COLCOL: return "::";
      case TOK_INTLIT: return "integer literal";
      case TOK_REALLIT: return "real literal";
      case TOK_STRLIT: return "string literal";
      case TOK_RETURN: return "return";
      case TOK_EXTERN: return "extern";
      case TOK_EXPORT: return "export";
      default: return "<INVALID TOKEN>";
    }
  }
}

static inline const char* tok_string(tokenizer_t* t, char** end, size_t line, size_t col) {
  int escape = 0;

  sb_t sb = { 0 };

  while(*(++*end)) {
    if (escape) {
      switch(**end) {
        case 'a' : sb_n_append(&sb, "\a", 1); break;
        case 'b' : sb_n_append(&sb, "\b", 1); break;
        case 'f' : sb_n_append(&sb, "\f", 1); break;
        case 'n' : sb_n_append(&sb, "\n", 1); break;
        case 't' : sb_n_append(&sb, "\t", 1); break;
        case 'r' : sb_n_append(&sb, "\r", 1); break;
        case 'v' : sb_n_append(&sb, "\v", 1); break;
        case '\"': sb_n_append(&sb, "\"", 1); break;
        case '\\': sb_n_append(&sb, "\\", 1); break;
        default:
          fprintf(stderr, "[ERROR]: %zu:%zu: Unknown escape sequence\n", line, col);
          return NULL;
      }

      escape = 0;
    } else {
      if      (**end == '\"') break;
      else if (**end == '\\') escape = 1;
      else if (**end == '\n') {
        fprintf(stderr, "[ERROR]: %zu:%zu: Missing closing `\"`\n", line, col);
        return NULL;
      }
      else
        sb_appendf(&sb, "%c", **end);
    }
    col++;
  }

  const char* ret = arena_sprintf(&t->arena, "%.*s", (int)sb.count, sb.items);
  sb_free(&sb);

  (*end)++;
  return ret;
}

// TODO: support exp notation for reals 
static inline int tok_num_literal(tokenizer_t* t, char** end, size_t line, size_t col, token_t* tok) {
  int base = 10;
  int64_t integer = 0;
  double real = 0.0;
  int sign = 1;

  tok->start = *end;

  lit_type_info_flags_t type_info = 0;

  if (**end == '-') { sign = -1; (*end)++; }

  if (**end == '0') {
    switch (*(*end + 1)) {
      case 'x': base = 16; (*end)+=2; col+=2; break;
      case 'b': base = 2;  (*end)+=2; col+=2; break;
      case 'o': base = 8;  (*end)+=2; col+=2; break;
      default: break;
    }
  }

  while(**end) {
    int digit = 0;
    
    if      (**end >= '0' && **end <= '9')
      digit = **end - '0';
    
    else if ( base == 16 && **end >= 'A' && **end <= 'F')
      digit = **end - 'A' + 10;

    else if ( base == 16 && **end >= 'a' && **end <= 'f')
      digit = **end - 'a' + 10;
    
    else if (**end == '_')
      continue;
    
    else if ( **end == 'U' || **end == 'u' || **end == 'L' || **end == 'l' )
      break;
    
    else if ( base == 10 && (**end == 'F' || **end == 'f' ))
      break;
    
    else if (base == 10 && **end == '.') {
      type_info |= TI_REAL;
      break;
    }
    
    else break;

    integer *= base;
    integer += digit;

    (*end)++;
    col++;
  }

  if (type_info & TI_REAL) {
    real = (double)integer;

    int64_t power = 10;

    while(isdigit(*++(*end))) {
      col++;
      real += (**end - '0') / power;
      power *= 10;
    }
  }

  switch (**end) {
    case 'U':
    case 'u': type_info |= TI_UNSIGNED; (*end)++; break;
    case 'F':
    case 'f': type_info |= TI_REAL; (*end)++; break;
    case 'L':
    case 'l': type_info |= TI_LONG; (*end)++; break;
  }

  if (type_info & TI_REAL) {
    tok->as.r = sign * real;
  } 

  if (type_info & TI_UNSIGNED) {
    if (sign < 0) {
      fprintf(stderr, "[ERROR] %zu:%zu: Cannot declare a negative unsigned literal.\n", line, col);
      return 1;
    }

    if (type_info & TI_REAL) {
      fprintf(stderr, "[ERROR] %zu:%zu: Cannot declare an unsigned real literal.\n", line, col);
      return 1;
    }
    tok->as.u = integer;
  } else {
    tok->as.i = sign * integer;
  }

  tok->kind = type_info & TI_REAL ? TOK_REALLIT : TOK_INTLIT;
  tok->type_info = type_info;
  tok->len = (int)(*end - tok->start);

  return 0;
}

static inline int tok_tokenize(tokenizer_t* t) {
  token_t tok = { 0 };
  size_t line = 1;
  size_t col  = 1;

  char* p = t->source.items;
  while(*p && ((size_t)(p - t->source.items) < t->source.count)) {
    if(*p == '\n') {
      line++;
      p++;
      col = 1;
      continue;
    }
    if(isspace(*p)) { p++; col++; continue; }

    switch(*p) {
      case '(':
      case ')':
      case '[':
      case ']':
      case '{':
      case '}':
      case ',':
      case ';':
      case '+':
      case '*':
      case '/':
        tok = (token_t){ .kind = *p, .start = p, .len = 1 }; 
        break;
      case ':':
        switch(*(p + 1)) {
          case ':': tok = (token_t){ .kind = TOK_COLCOL, .start = p, .len = 2 }; break;
          default: 
            tok = (token_t){ .kind = *p, .start = p, .len = 1 };
            break;
        }
        break;
      case '.':
        if (isdigit(*(p + 1))) { 
          char* end = p;
          if(tok_num_literal(t, &end, line, col, &tok)) return 1;
          break;
        } 

        tok = (token_t){ .kind = *p, .start = p, .len = 1 };
        break;
      case '-':
        if (isdigit(*(p + 1))) { 
          char* end = p;
          if(tok_num_literal(t, &end, line, col, &tok)) return 1;
          break;
        }

        switch(*(p + 1)) {
          case '>': tok = (token_t){ .kind = TOK_ARROW, .start = p, .len = 2 }; break;
          // case '=': tok = (token_t){ .kind = TOK_MINEQS }; break;
          default: 
            tok = (token_t){ .kind = *p, .start = p, .len = 1 };
            break;
        }
        break;

      default:
        if(isalpha(*p)) {
          uint8_t kw = 0;
          for(size_t i=0; i < sizeof(tok_keywords)/sizeof(*tok_keywords); i++) {
            if (!tok_keywords[i]) continue;
            int kw_len = strlen(tok_keywords[i]);
            if(memcmp(p, tok_keywords[i], kw_len) == 0) {
              kw = 1;
              tok = (token_t){ .kind = i, .start = p, .len = kw_len };
              break;
            }
          }
          if(kw) break;

          char* end = p;
          for (; *end && (isalpha(*end) || isdigit(*end)); end++);
          tok = (token_t){
            .kind = TOK_IDENT,
            .len = (int)(end - p),
            .start = p,
            .as.s = arena_sprintf(&t->arena, "%.*s", (int)(end - p), p),
          };
        } else if (isdigit(*p)) {
          char* end = p;
          if(tok_num_literal(t, &end, line, col, &tok)) return 1;

        } else if (*p == '"') {
          char* end = p;
          const char* str = tok_string(t, &end, line, col);
          if (!str) return 1;
          tok = (token_t){ 
            .kind = TOK_STRLIT,
            .len = (int)(end - p),
            .start = p,
            .as.s = str
          };
        } else {
          char* line_start = p;
          char* line_end   = p;
          while(line_start != t->source.items && *(line_start-1) != '\n') line_start--;
          while(*line_end && *line_end != '\n') line_end++;
          int line_length = line_end - line_start;
          fprintf(stderr, "[ERROR]: %zu:%zu Unrecognized token (%c, 0x%x)\n%.*s\n%*s^\n", line, col, *p, *p, line_length, line_start, (int)col - 1, "");
          // TODO: continue instead of returning
          return 1;
        }
    }

    tok.line = line;
    tok.col  = col;
    da_append(&t->tokens, tok);

    p  += tok.len;
    col += tok.len;
  }

  da_append(&t->tokens, ((token_t){ .kind = TOK_EOF, .line = line, .col = col }));
  return 0;
}

void tok_destroy(tokenizer_t* t) {
  sb_free(&t->source);
  da_free(t->tokens);
  arena_free(&t->arena);
}

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
  
  TYPE_ALIAS,

  TYPE_FUNC,

  TYPES_COUNT,
} type_kind_t;

typedef struct _type {
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
  } as;
} type_t;

static inline void print_type(FILE* stream, type_t *type);

static inline int type_equals(type_t a, type_t b) {
  while (a.kind == TYPE_ALIAS) a = *a.as.alias.target;
  while (b.kind == TYPE_ALIAS) b = *a.as.alias.target;

  if (a.kind != b.kind) return 0;

  switch(a.kind) {
    case TYPE_NONE:
    case TYPE_I32:
    case TYPE_I64:
    case TYPE_U32:
    case TYPE_U64:
    case TYPE_F32:
    case TYPE_F64:
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_STR:
    case TYPE_ARRAY:
    case TYPE_ADDR:
      return 1;
    case TYPE_STRUCT:
      // TODO: compare fields
      return 1;
    case TYPE_FUNC:
      if (!type_equals(*a.as.func.ret, *b.as.func.ret)) return 0;
      if (a.as.func.params.count != b.as.func.params.count) return 0;
      for (size_t i=0; i < a.as.func.params.count; i++) {
        if (!type_equals(*a.as.func.params.items[i], *b.as.func.params.items[i])) return 0;
      }
      return 1;
    case TYPE_ALIAS:
    case TYPES_COUNT:
    default:
      UNREACHABLE("type_equals");
  }
}

typedef struct _scope scope_t;

typedef enum {
  PASS_DECL,
  PASS_STMTS,
} pass_t;

typedef struct {
  sb_t source;
  size_t line;
  size_t col;

  arena_t arena;
  tokenarr_t tokens;
  size_t current;
  scope_t* current_scope;
  struct {
    type_t* items;
    size_t count;
    size_t capacity;
  } types;
  pass_t pass;
} parser_t;

static inline type_t* get_or_create_array_of(parser_t* p, type_t* type) {
  da_foreach(type_t, t, &p->types) {
    if (t->kind == TYPE_ARRAY && t->as.array.inner == type) return t;
  }

  da_append(&p->types, ((type_t){ .kind = TYPE_ARRAY, .name = NULL, .size = 1, .as.array.inner = type }));
  
  return &da_last(&p->types);
}

static inline type_t* resolve_type(parser_t* p, const char* name, int depth) {
  type_t* type = NULL;

  da_foreach(type_t, t, &p->types) {
    // OPTIMIZE
    if(strcmp(t->name, name) == 0) { type = t; break; }
  }

  if (!type) return NULL;

  while(depth-- > 0) {
    type = get_or_create_array_of(p, type);
  }

  return type;
}

typedef enum {
  AST_ROOT = 1,
  AST_EXPR,
  AST_STMT,
  AST_VAR_DECL,
  AST_FUNC_DECL,
  AST_FUNC_DEF,
  AST_VAR_DEF,
  AST_FUNCALL,
  AST_TYPE,
  AST_BODY,
  AST_PARAM,
  AST_SIG,
} ast_node_kind_t;

#define AST_DEFAULT_FIELDS\
  ast_node_kind_t ast_kind; \

typedef struct {
  AST_DEFAULT_FIELDS;
} ast_node_t;

typedef enum {
  SPEC_NONE = 0,
  SPEC_EXTERN = 1 << 0,
  SPEC_EXPORT = 1 << 1,
} spec_flags_t;

static inline token_t get_tok(parser_t* p) {
  if (p->current >= p->tokens.count) {
    fprintf(stderr, "[FATAL]: No more tokens.");
    abort();
  }
  return p->tokens.items[p->current];
}

static inline int tok_is(parser_t* p, tok_kind_t kind) {
  return get_tok(p).kind == kind; 
}

static inline int expect_with_name(parser_t* p, tok_kind_t kind, const char* exp_name) {
  token_t t = get_tok(p);
  if (t.kind != kind) {
    DIAGF(p, ERROR, "Expected %s, found %s", exp_name, tok_kind_str(&p->arena, t.kind));
    abort();
    return 0;
  }
  return 1;
}

static inline int expect(parser_t* p, tok_kind_t kind) {
  return expect_with_name(p, kind, tok_kind_str(&p->arena, kind));
}

static inline void next(parser_t* p) {
  p->current++;
  p->line = get_tok(p).line;
  p->col = get_tok(p).col;
}

typedef enum {
  SYMB_VAR,
  SYMB_FUNC,
} symb_kind_t;

typedef enum {
  STO_SCOPE,
  STO_EXTERN,
  STO_EXPORT,
} symb_storage_t;

typedef struct {
  const char* name;
  symb_kind_t kind;
  symb_storage_t storage;
  type_t type;
  uint32_t addr;
} symbol_t;

struct _scope {
  const char* name;
  struct _scope* parent;
  // TODO: OPTIMIZE
  // transform into hashmap
  struct {
    symbol_t* items;
    size_t count;
    size_t capacity;
  } symbols;
};

static inline void print_symbol_table(FILE* stream, scope_t* root);

static inline symbol_t* make_symbol(scope_t* scope, symb_kind_t kind, symb_storage_t storage, const char* name, type_t type) {
  da_append(&scope->symbols, ((symbol_t){ .name = name, .kind = kind, .storage = storage, .type = type }));
  return &da_last(&scope->symbols);
}

static inline symbol_t* resolve_symbol_local_any(scope_t* scope, const char* name) { 
  if (!scope) return NULL;

  // OPTIMIZE
  da_foreach(symbol_t, s, &scope->symbols) {
    if(strcmp(s->name, name) == 0) return s;
  }
  return NULL;
}

static inline symbol_t* resolve_symbol_local(scope_t* scope, const char* name, symb_kind_t kind) { 
  if (!scope) return NULL;

  // OPTIMIZE
  da_foreach(symbol_t, s, &scope->symbols) {
    if(strcmp(s->name, name) == 0 && s->kind == kind) return s;
  }
  return NULL;
}

static inline symbol_t* resolve_symbol(scope_t* scope, const char* name, symb_kind_t kind) {
  if (!scope) return NULL;

  // OPTIMIZE
  da_foreach(symbol_t, s, &scope->symbols) {
    if(strcmp(s->name, name) == 0 && s->kind == kind) return s;
  }
  return resolve_symbol(scope->parent, name, kind);
}

static inline void enter_scope_new(parser_t* p, const char* name) {
  scope_t* parent = p->current_scope;
  p->current_scope = arena_alloc(&p->arena, sizeof(scope_t));
  p->current_scope->name = name;
  p->current_scope->parent = parent;
  // fprintf(stderr, "[DEBUG] New scope %s, son of %s.\n", name, p->current_scope->parent ? p->current_scope->parent->name : "noone");
}

static inline void enter_scope(parser_t* p, scope_t* s) {
  p->current_scope = s;
}

static inline void exit_scope(parser_t* p) {
  p->current_scope = p->current_scope->parent;
}

// specifiers = ( specifier )*
static inline spec_flags_t parse_specifiers(parser_t* p) {
  spec_flags_t flags = SPEC_NONE;

  for (;;) {
    spec_flags_t cur_flag = SPEC_NONE;

    token_t t = get_tok(p);

    switch((kw_kind_t)t.kind) {
      case KW_EXTERN: cur_flag = SPEC_EXTERN; break;
      case KW_EXPORT: cur_flag = SPEC_EXPORT; break;
      case KW_RETURN:
      default: return flags;
    }

    if (flags & cur_flag)
      DIAGF(p, WARN, "Duplicate specifier `%.*s`, will be considered as one.\n", t.len, t.start);

    flags |= cur_flag;

    next(p);
  }

  return flags;
}

typedef enum {
  DEF_KIND_VAR,
  DEF_KIND_FUNC,
} ast_def_kind;

typedef struct {
  AST_DEFAULT_FIELDS;
  type_t type;
} ast_type_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  ast_type_t* type;
} ast_param_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  struct {
    ast_param_t** items;
    size_t count;
    size_t capacity;
  } params;
  ast_type_t* ret;
  type_t type;
} ast_sig_t;

typedef struct _ast_body_t ast_body_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  spec_flags_t flags;
  ast_def_kind kind; 
  union {
    struct {
      ast_sig_t* sig;
      int has_body;
      scope_t* scope;
    } fun;
    struct {
      ast_type_t* type;
    } var;
  } as;
} ast_def_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  ast_def_t* def;
  ast_body_t* body;
} ast_decl_t;

typedef struct _ast_expr_t ast_expr_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  const char* name;
  struct {
    ast_expr_t** items;
    size_t count;
    size_t capacity;
  } args;
} ast_funcall_t;

typedef enum {
  EXPR_STRING = 0,
  EXPR_NUMBER,
  EXPR_BINOP,
  EXPR_UNOP,
  EXPR_SUBEXPR,
} ast_expr_kind_t;

typedef enum {
  OP_INVALID = TOK_EOF,
  OP_PLUS    = '+',
  OP_MINUS   = '-',
  OP_MULT    = '*',
  OP_DIV     = '/',
  OP_REM     = '%',
  OP_CALL    = '(',
  OP_MEMB    = '.',
  OP_SCOPE   = TOK_COLCOL,
} op_kind_t;

enum _bp {
  BP_NONE = 0,
  BP_ADD,
  BP_MULT,
  BP_CALL,
  BP_ACCESS,
};

const enum _bp expr_bp_table[] = {
  [OP_INVALID] = BP_NONE,
  [OP_PLUS]    = BP_ADD,
  [OP_MINUS]   = BP_ADD, 
  [OP_MULT]    = BP_MULT,
  [OP_DIV]     = BP_MULT,
  [OP_REM]     = BP_MULT,
  [OP_CALL]    = BP_CALL,
  [OP_MEMB]    = BP_ACCESS,
  [OP_SCOPE]   = BP_ACCESS,
};

struct _ast_expr_t {
  AST_DEFAULT_FIELDS;
  ast_expr_kind_t kind;
  union {
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
    ast_expr_t* subexpr;
    // ast_funcall_t* funcall;
  } as;
};

typedef enum {
  STMT_EMPTY = 0,
  STMT_RET,
  STMT_FUNCALL,
} ast_stmt_kind_t;

typedef struct {
  AST_DEFAULT_FIELDS;
  ast_stmt_kind_t kind;
  union {
    ast_funcall_t* func;
    ast_expr_t*    retval;
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

static inline ast_expr_t* parse_expr(parser_t* p, int bp);

// funcall = '(' [ arg ( ',' arg )* ] ')'
static inline ast_funcall_t* parse_funcall(parser_t* p, const char* name) {
  symbol_t* s = resolve_symbol(p->current_scope, name, SYMB_FUNC);
  if(!s) {
    DIAGF(p, ERROR, "Undeclared function `%s`.\n", name);
    return NULL;
  }

  ast_funcall_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_FUNCALL;

  if(!expect(p, '(')) return NULL;
  next(p);

  n->name = arena_strdup(&p->arena, name);

  if (!tok_is(p, ')')) {
    ast_expr_t* arg = parse_expr(p, 0);
    if(!arg) return NULL;
    da_append(&n->args, arg);

    while(tok_is(p, ',')) {
      if(!expect(p, ',')) return NULL;
      next(p);

      arg = parse_expr(p, 0);
      if(!arg) return NULL;
      da_append(&n->args, arg);
    }
  }
  if(!expect(p, ')')) return NULL;
  next(p);

  return n;
}

static inline ast_expr_t* parse_primary_expr(parser_t* p) {
  ast_expr_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_EXPR;

  if (tok_is(p, TOK_STRLIT)) {
    n->kind = EXPR_STRING;

    if(!expect(p, TOK_STRLIT)) return NULL;
    const char* str = arena_strdup(&p->arena, get_tok(p).as.s);
    next(p);
    
    if(!str) return NULL; 
    n->as.s = str;
  } else if (tok_is(p, TOK_INTLIT)) {
    n->kind = EXPR_NUMBER;

    if(!expect(p, TOK_INTLIT)) return NULL;
    
    lit_type_info_flags_t ti = get_tok(p).type_info;
    n->as.number.ti = ti;
    if (ti & TI_UNSIGNED) n->as.number.i = get_tok(p).as.i;
    else                  n->as.number.u = get_tok(p).as.u;

    next(p);
  } else if (tok_is(p, TOK_REALLIT)) {
    n->kind = EXPR_NUMBER;

    if(!expect(p, TOK_REALLIT)) return NULL;
    
    n->as.number.ti = get_tok(p).type_info;
    n->as.number.r  = get_tok(p).as.r;

    next(p);
  } else if (tok_is(p, '(')) {
    n->kind = EXPR_SUBEXPR;

    if(!expect(p, '(')) return NULL;
    next(p);

    ast_expr_t* subexpr = parse_expr(p, 0);
    if(!subexpr) return NULL;
    n->as.subexpr = subexpr;

    if(!expect(p, ')')) return NULL;
    next(p);
  }

  return n;
}

static inline ast_expr_t* parse_expr(parser_t* p, int bp) {
  ast_expr_t* lhs = parse_primary_expr(p);
  if(!lhs) return NULL;

  for(;;) {
    op_kind_t op = OP_INVALID;
    if (tok_is(p, '+'))      op = OP_PLUS;
    else if (tok_is(p, '-')) op = OP_MINUS;
    else if (tok_is(p, '*')) op = OP_MULT;
    else if (tok_is(p, '/')) op = OP_DIV;
    else return lhs; 

    int curr_bp = expr_bp_table[op];
    if (curr_bp < bp)
      return lhs;

    // NOTE: enum contains tokens
    if(!expect(p, (tok_kind_t)op)) return NULL;
    next(p);

    ast_expr_t* rhs = parse_expr(p, curr_bp + 1);
    if(!rhs) return NULL;

    ast_expr_t *n = arena_alloc(&p->arena, sizeof(*n));
    n->ast_kind = AST_EXPR;
    n->kind = EXPR_BINOP;
    n->as.binop.lhs = lhs;
    n->as.binop.rhs = rhs;
    n->as.binop.op = op;

    lhs = n;
  }
}

// stmt = return [ expr ] ';' 
//      | ident funcall ';'
//      | ident ':' [ type ] '=' expr ';'
//      | ';'
static inline ast_stmt_t* parse_stmt(parser_t* p) {
  ast_stmt_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_STMT;

  if(tok_is(p, ';')) {
    if(!expect(p,';')) return NULL;
    next(p);

    n->kind = STMT_EMPTY;
  } else if (tok_is(p, TOK_RETURN)) {
    if(!expect(p, TOK_RETURN)) return NULL;
    next(p);
    n->kind = STMT_RET;

    if (!tok_is(p, ';')) {
      ast_expr_t* retval = parse_expr(p, 0);
      if(!retval) return NULL;
      n->as.retval = retval;
    }
    if(!expect(p,';')) return NULL;
    next(p);
  } else if (tok_is(p, TOK_IDENT)) {
    if (!expect(p, TOK_IDENT)) return NULL;
    const char* name = get_tok(p).as.s; 
    next(p);

    if (tok_is(p, '(')) {
      ast_funcall_t* fc = parse_funcall(p, name);
      if(!fc) return NULL;
      n->kind = STMT_FUNCALL;
      n->as.func = fc;
    } else if (tok_is(p, ':')) {
      // TODO: parse assignment
    }
    if(!expect(p,';')) return NULL;
    next(p);
  }

  return n;
}

// body = '{' ( stmt )* '}'
static inline ast_body_t* parse_body(parser_t* p) {
  ast_body_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_BODY;

  if(!expect(p, '{')) return NULL;
  next(p);

  while(!tok_is(p, '}')) {
    ast_stmt_t* stmt = parse_stmt(p);
    if(!stmt) return NULL;
    
    da_append(&n->stmts, stmt);
  }

  if(!expect(p, '}')) return NULL;
  next(p);

  return n;
}

static inline ast_type_t* parse_type(parser_t* p) {
  ast_type_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_TYPE;

  if(!expect_with_name(p, TOK_IDENT, "type")) return NULL;
  token_t tok = get_tok(p);
  next(p);

  int array_depth = 0;
  while(tok_is(p, '[')) {
    if (!expect(p, '[')) return NULL;
    next(p);
    if (!expect(p, ']')) return NULL;
    next(p);
    array_depth++;
  }

  type_t* type = resolve_type(p, tok.as.s, array_depth);
  if(!type) {
    DIAGF(p, ERROR, "Undefined type `%s`.\n", tok.as.s);
    return NULL;
  }
  n->type = *type;

  return n;
}

// TODO: convert param to variable definition
// param = ident ':' ident
static inline ast_param_t* parse_param(parser_t* p) {
  ast_param_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_PARAM;

  if(!expect(p, TOK_IDENT)) return NULL;
  n->name = arena_strdup(&p->arena, get_tok(p).as.s);
  next(p);

  if(!expect(p, ':')) return NULL;
  next(p);

  ast_type_t* t = parse_type(p);
  if(!t) return NULL;
  n->type = t;

  make_symbol(p->current_scope, SYMB_VAR, STO_SCOPE, n->name, t->type);

  return n;
}

// sig = '(' [ param ( ',' param )* ] ')' -> ident 
static inline ast_sig_t* parse_signature(parser_t* p) {
  ast_sig_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_SIG;
  n->type.kind = TYPE_FUNC;

  if(!expect(p, '(')) return NULL;
  next(p);

  if (tok_is(p, ')')) {
    if(!expect(p, ')')) return NULL;
    next(p);
  } else { 
    ast_param_t* param = parse_param(p);
    if(!param) return NULL;

    da_append(&n->params, param);
    da_append(&n->type.as.func.params, &param->type->type);

    if (tok_is(p, ')')) {
      if(!expect(p, ')')) return NULL;
      next(p);
    } else {
      while(tok_is(p, ',')) {
        if (!expect(p, ',')) return NULL;
        next(p);

        param = parse_param(p);
        if(!param) return NULL;

        da_append(&n->params, param);
        da_append(&n->type.as.func.params, &param->type->type);
      } 

      if(!expect(p, ')')) return NULL;
      next(p);
    }
  }

  if(!expect(p, TOK_ARROW)) return NULL;
  next(p);
  ast_type_t* t = parse_type(p);
  if(!t) return NULL;
  n->ret = t;
  n->type.as.func.ret = &t->type;

  return n;
}

// func_decl = func_def body
static inline ast_decl_t* parse_func_decl(parser_t* p, ast_def_t* def) {
  // NOTE: skip until '{', assume definition has been correctly parsed by
  // parse_func_def in previous pass
  while(!tok_is(p, '{')) next(p);

  ast_decl_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_FUNC_DECL;
  n->def = def;

  enter_scope(p, n->def->as.fun.scope);

  ast_body_t* body = parse_body(p);
  if(!body) return NULL;

  n->body = body;

  exit_scope(p);

  return n;
}

// func_def = sig ( ';' | '{' )
static inline ast_def_t* parse_func_def(parser_t* p, const char* name, spec_flags_t flags) {
  ast_def_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_FUNC_DEF;
  n->flags = flags;
  n->name = arena_strdup(&p->arena, name);
  n->kind = DEF_KIND_FUNC;

  symb_storage_t sto = STO_SCOPE;
  if(flags & STO_EXTERN) sto = STO_EXTERN; 
  if(flags & STO_EXPORT) sto = STO_EXPORT; 

  symbol_t* symbol = make_symbol(p->current_scope, SYMB_FUNC, sto, n->name, (type_t){ 0 });

  enter_scope_new(p, arena_sprintf(&p->arena, "fun_%s", name));
  n->as.fun.scope = p->current_scope;

  n->as.fun.sig = parse_signature(p);
  if(!n->as.fun.sig) return NULL;

  symbol_t *s = resolve_symbol(p->current_scope, name, SYMB_FUNC);
  if(s && s != symbol && type_equals(n->as.fun.sig->type,  s->type)) {
    // TODO: use DIAGF macro -> print_type should print to sb_t
    fprintf(stderr, "[ERROR]: %zu:%zu: Function `%s : ", get_tok(p).line, get_tok(p).col, name);
    print_type(stderr, &s->type);
    fprintf(stderr, "` already declared in this scope.\n");
    return NULL;
  }

  symbol->type = n->as.fun.sig->type;

  if (tok_is(p, ';')) {
    if(!expect(p, ';')) return NULL;
    next(p);
  } else {
    if(!expect(p, '{')) return NULL;
    next(p);
    n->as.fun.has_body = 1;
    // NOTE: delay body parsing to parse_func_decl
    for (int pars = 1; pars && !tok_is(p, TOK_EOF); next(p)) {
      if      (tok_is(p,'{')) pars++;
      else if (tok_is(p,'}')) pars--;
    }
  }

  exit_scope(p);

  return n;
}

typedef struct {
  AST_DEFAULT_FIELDS;
  struct {
    ast_def_t** items;
    size_t count;
    size_t capacity;
  } defs;
  struct {
    ast_decl_t** items;
    size_t count;
    size_t capacity;
  } top_level;
  scope_t* scope;
} ast_root_t;

// TODO: 2-step compiler: first parse only declarations to build a symbol table,
// then parse the statements with the already filled symbols, having only to 
// patch the addresses after code generation
static inline ast_root_t* parse(parser_t* p, tokenizer_t* t) {
  p->source = t->source;
  p->tokens = t->tokens;
  p->current = 0;
  p->pass = PASS_DECL;

  // init types
  da_append(&p->types, ((type_t){ .kind = TYPE_NONE,  .name = "none", .size = 0 }));
  da_append(&p->types, ((type_t){ .kind = TYPE_I32,   .name = "i32",  .size = 1 }));
  da_append(&p->types, ((type_t){ .kind = TYPE_I64,   .name = "i64",  .size = 2 }));
  da_append(&p->types, ((type_t){ .kind = TYPE_U32,   .name = "u32",  .size = 1 }));
  da_append(&p->types, ((type_t){ .kind = TYPE_U64,   .name = "u64",  .size = 2 }));
  da_append(&p->types, ((type_t){ .kind = TYPE_U32,   .name = "f32",  .size = 1 }));
  da_append(&p->types, ((type_t){ .kind = TYPE_U64,   .name = "f64",  .size = 2 }));
  da_append(&p->types, ((type_t){ .kind = TYPE_CHAR,  .name = "char", .size = 1 }));
  da_append(&p->types, ((type_t){ .kind = TYPE_BOOL,  .name = "bool", .size = 1 }));
  da_append(&p->types, ((type_t){ .kind = TYPE_STR,   .name = "str",  .size = 1 }));
  da_append(&p->types, ((type_t){ .kind = TYPE_ADDR,  .name = "addr", .size = 1 }));

  enter_scope_new(p, "root");

  ast_root_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_ROOT;
  n->scope = p->current_scope;

  while(!tok_is(p, TOK_EOF)) {
    spec_flags_t flags = parse_specifiers(p);
    
    if(!expect(p, TOK_IDENT)) return NULL;
    const char* name = get_tok(p).as.s; 
    next(p);
    if(!expect(p, ':')) return NULL;
    next(p);

    if(tok_is(p, '(')) {
      ast_def_t* def = parse_func_def(p, name, flags);
      if (!def) return NULL;

      da_append(&n->defs, def);
    } else {
      // TODO: variables 
      TODO("variable definitions");
    }
  }

  p->current = 0;
  p->pass++;

  da_foreach(ast_def_t*, def, &n->defs) {
    if ((*def)->kind != DEF_KIND_FUNC) continue;
    if (!(*def)->as.fun.has_body) continue;

    ast_decl_t* decl = parse_func_decl(p, *def);
    if(!decl) return NULL;

    da_append(&n->top_level, decl);
  }

  return n;
}

static inline void print_ast(FILE* stream, ast_node_t* n, int level) {
  switch(n->ast_kind) {
    case AST_ROOT:
      ast_root_t* root = (ast_root_t*)n;
      fprintf(stream, "%*s%s\n", level, "","AST_ROOT");
      da_foreach(ast_decl_t*, d, &root->top_level) {
        print_ast(stream, (ast_node_t*)*d, level + 2);
      }
      break;
    case AST_EXPR:
      ast_expr_t* expr = (ast_expr_t*)n;
      fprintf(stream, "%*s%s", level, "", "AST_EXPR");
      switch(expr->kind) {
        case EXPR_STRING:
          fprintf(stream, " (%s)\n", "EXPR_STRING");
          fprintf(stream, "%*s%s\n", level + 2, "", expr->as.s);
          break;
        case EXPR_NUMBER:
          fprintf(stream, " (%s)\n", "EXPR_NUMBER");
          if (expr->as.number.ti & TI_REAL) {
            fprintf(stream, "%*s%lf\n", level + 2, "", expr->as.number.r);
          } else if (expr->as.number.ti & TI_UNSIGNED) {
            fprintf(stream, "%*s%lu (unsigned)\n", level + 2, "", expr->as.number.u);
          } else {
            fprintf(stream, "%*s%ld\n", level + 2, "", expr->as.number.i);
          }
          break;
        case EXPR_BINOP:
          fprintf(stream, " (%s)\n", "EXPR_BINOP");
          fprintf(stream, "%*sop: %c\n", level + 2, "", expr->as.binop.op);
          fprintf(stream, "%*slhs:\n", level + 2, "");
          print_ast(stream, (ast_node_t*)expr->as.binop.lhs, level + 2);
          fprintf(stream, "%*srhs:\n", level + 2, "");
          print_ast(stream, (ast_node_t*)expr->as.binop.rhs, level + 2);
          break;
        case EXPR_UNOP:
          fprintf(stream, " (%s)\n", "EXPR_UNOP");
          TODO("print_ast (EXPR_UNOP)");
          break;
        case EXPR_SUBEXPR:
          fprintf(stream, " (%s)\n", "EXPR_SUBEXPR");
          print_ast(stream, (ast_node_t*)expr->as.subexpr, level + 2);
          break;
        default: break;
      }
      break;
    case AST_STMT:
      ast_stmt_t* stmt = (ast_stmt_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_STMT");
      switch(stmt->kind) {
        case STMT_EMPTY: break;
        case STMT_RET:
          print_ast(stream, (ast_node_t*)stmt->as.retval, level + 2);
          break;
        case STMT_FUNCALL:
          print_ast(stream, (ast_node_t*)stmt->as.func, level + 2);
          break;
        default: break;
      }
      break;
    case AST_VAR_DECL:
      fprintf(stream, "%*s%s\n", level, "", "AST_VAR_DECL");
      break;
    case AST_VAR_DEF:
      fprintf(stream, "%*s%s\n", level, "", "AST_VAR_DEF");
      break;
    case AST_FUNC_DEF:
      ast_def_t* def = (ast_def_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_FUNC_DECL");
      fprintf(stream, "%*sName: %s\n", level + 2, "", def->name);
      fprintf(stream, "%*sFlags: ", level + 2, "");
      if(def->flags & SPEC_EXTERN) fprintf(stream, "extern ");
      if(def->flags & SPEC_EXPORT) fprintf(stream, "export ");
      fprintf(stream, "\n");
      print_ast(stream, (ast_node_t*)def->as.fun.sig, level + 2);
      break;
    case AST_FUNC_DECL:
      ast_decl_t* decl = (ast_decl_t*)n;
      print_ast(stream, (ast_node_t*)decl->def, level + 2);
      if(decl->body)
        print_ast(stream, (ast_node_t*)decl->body, level + 2);
      break;
    case AST_FUNCALL:
      ast_funcall_t* fun = (ast_funcall_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_FUNCALL");
      fprintf(stream, "%*sName: %s\n", level + 2, "", fun->name);
      fprintf(stream, "%*sArgs:\n", level + 2, "");
      da_foreach(ast_expr_t*, a, &fun->args) {
        print_ast(stream, (ast_node_t*)*a, level + 2);
      }
      break;
    case AST_TYPE:
      fprintf(stream, "%*s%s\n", level, "", "AST_TYPE");
      fprintf(stream, "%*sName: %s\n", level + 2, "", ((ast_type_t*)n)->type.name);
      break;
    case AST_BODY:
      fprintf(stream, "%*s%s\n", level, "", "AST_BODY");
      da_foreach(ast_stmt_t*, stmt, &((ast_body_t*)n)->stmts) {
        print_ast(stream, (ast_node_t*)*stmt, level + 2);
      }
      break;
    case AST_PARAM:
      ast_param_t* param = (ast_param_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_PARAM");
      fprintf(stream, "%*sName: %s\n", level + 2, "", param->name);
      fprintf(stream, "%*sType:\n", level + 2, "");
      print_ast(stream, (ast_node_t*)param->type, level + 2);
      break;
    case AST_SIG:
      ast_sig_t* sig = (ast_sig_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_SIG");
      fprintf(stream, "%*sParams:\n", level + 2, "");
      da_foreach(ast_param_t*, p, &sig->params) {
        print_ast(stream, (ast_node_t*)*p, level + 2);
      }
      fprintf(stream, "%*sRet type:\n", level + 2, "");
      print_ast(stream, (ast_node_t*)sig->ret, level + 2);
      break;
    default:
      UNREACHABLE("print_ast");
  }
}

static inline void print_type(FILE* stream, type_t* t) {
  switch(t->kind) {
    case TYPE_NONE:
    case TYPE_I32:
    case TYPE_I64:
    case TYPE_U32:
    case TYPE_U64:
    case TYPE_F32:
    case TYPE_F64:
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_STR:
    case TYPE_ADDR:
    case TYPE_ALIAS:
      fprintf(stream, "%s", t->name);
      break;
    case TYPE_STRUCT:
      fprintf(stream, "struct %s", t->name);
      break;
    case TYPE_ARRAY:
      print_type(stream, t->as.array.inner);
      fprintf(stream, "[]");
      break;
    case TYPE_FUNC:
      fprintf(stream, "(");
      da_foreach(type_t*, typ, &t->as.func.params) {
        print_type(stream, *typ);
        fprintf(stream, ",");
      }
      fprintf(stream, ") -> ");
      print_type(stream, t->as.func.ret);
    case TYPES_COUNT:
    default: break;
  }
}

static inline void print_symbol_table_entries(FILE* stream, scope_t* scope) {
  if(!scope) return;
  da_foreach(symbol_t, s, &scope->symbols) {
    fprintf(stream, "%-10s %-10s ", s->name, scope->name);
    switch(s->storage) {
      case STO_SCOPE:
        fprintf(stream, "%-10s ", "scope"); break;
      case STO_EXTERN:
        fprintf(stream, "%-10s ", "extern"); break;
      case STO_EXPORT:
        fprintf(stream, "%-10s ", "export"); break;
      default: break;
    }
    fprintf(stream, "0x%08X ", s->addr);
    print_type(stream, &s->type);
    fprintf(stream, "\n");
  }
  print_symbol_table_entries(stream, scope->parent);
}

static inline void print_symbol_table(FILE* stream, scope_t* scope) {
  if(!scope) return;
  fprintf(stream, "%s symbol Table:\n", scope->name);
  fprintf(stream, "%-10s %-10s %-10s %-10s %s\n", "name", "scope", "storage", "addr", "type");
  print_symbol_table_entries(stream, scope);
}

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

typedef struct {
  instrarr_t code;
  struct _consts {
    constant_t* items;
    size_t count;
    size_t capacity;
  } constants;
  struct _externs {
    ffi_cif* items;
    size_t count;
    size_t capacity;
  } externs;
} program_t;

static inline int compile_expr(program_t* p, scope_t* scope, ast_expr_t* e);

static inline int compile_funcall(program_t* p, scope_t* scope, ast_funcall_t* f) {
  symbol_t* symbol = resolve_symbol(scope, f->name, SYMB_FUNC);
  if (!symbol) {
    fprintf(stderr, "[ERROR]: Unresolved symbol `%s`", f->name);
    return 1;
  }

  da_foreach(ast_expr_t*, a, &f->args)
    compile_expr(p, scope, *a);

  da_append(&p->code, symbol->storage == STO_EXTERN ? INST_HOSTCALL : INST_CALL);
  da_append(&p->code, symbol->addr);

  return 0;
}

static inline int compile_expr(program_t* p, scope_t* scope, ast_expr_t* e) {
  (void)scope;
  
  switch(e->kind) {
      case EXPR_STRING:
        da_append(&p->code, INST_LOADC);
        da_append(&p->code, p->constants.count);\
        da_append(&p->constants, ((constant_t){ .kind = DK_STR, .as.s = e->as.s }));
        break;
      case EXPR_NUMBER:
        if(e->as.number.ti & TI_LONG) {
          da_append(&p->code, INST_PUSHL);
          da_append(&p->code, e->as.number.u >> 32);
          da_append(&p->code, e->as.number.u & (uint32_t)-1);
        } else {
          da_append(&p->code, INST_PUSH);
          da_append(&p->code, e->as.number.u & (uint32_t)-1);
        }
        break;
      case EXPR_BINOP:
        compile_expr(p, scope, e->as.binop.lhs);
        compile_expr(p, scope, e->as.binop.rhs);
        TODO("compile_expr (BINOP)");
        break;
      case EXPR_UNOP:
        TODO("compile_expr (UNOP)");
        break;
      case EXPR_SUBEXPR:
        return compile_expr(p, scope, e->as.subexpr);
      default:
        UNREACHABLE("compile_expr");
  }

  return 0;
}

static inline int compile_stmt(program_t* p, scope_t* scope, ast_stmt_t* s) {
  switch(s->kind) {
    case STMT_EMPTY: return 0;
    case STMT_FUNCALL: return compile_funcall(p, scope, s->as.func);
    case STMT_RET: {
      compile_expr(p, scope, s->as.retval);
      // TODO: function epilog -> destroy frame, clean stack
      da_append(&p->code, INST_RET);
      return 0;
    }
    default: return 1;
  }
}

static inline ffi_type* type_to_ffi_type(type_t t) {
  switch(t.kind) {
    case TYPE_NONE:   return &ffi_type_void;
    case TYPE_I32:    return &ffi_type_sint32;
    case TYPE_I64:    return &ffi_type_sint64;
    case TYPE_U32:    return &ffi_type_uint32;
    case TYPE_U64:    return &ffi_type_uint64;
    case TYPE_F32:    return &ffi_type_float;
    case TYPE_F64:    return &ffi_type_double;
    case TYPE_CHAR:   return &ffi_type_uchar;
    case TYPE_BOOL:   return &ffi_type_uint8;
    case TYPE_STR:    return NULL;
    case TYPE_ADDR:   return &ffi_type_pointer;
    case TYPE_ARRAY:  return &ffi_type_pointer;
    case TYPE_STRUCT: return NULL;
    case TYPE_ALIAS:  return type_to_ffi_type(*t.as.alias.target);
    case TYPE_FUNC:   return &ffi_type_pointer; 
    case TYPES_COUNT:
    default:
      UNREACHABLE("type_to_ffi_type - Invalid type");
 }
}

static inline int compile_func_def(program_t* p, ast_def_t* d) {
  symbol_t* symbol = resolve_symbol(d->as.fun.scope, d->name, SYMB_FUNC);
  if (!symbol) {
    fprintf(stderr, "[ERROR]: Unresolved symbol `%s`\n", d->name);
    return 1;
  }

  ast_sig_t* sig = d->as.fun.sig;

  if (d->flags & SPEC_EXTERN) {
    // TODO: add platform layer for malloc
    ffi_type **param_types = malloc(sizeof(*param_types) * sig->params.count);
    for(size_t i=0; i < sig->params.count; i++)
      param_types[i] = type_to_ffi_type(sig->params.items[i]->type->type);

    ffi_type* ret = type_to_ffi_type(sig->ret->type);

    ffi_cif cif = { 0 };
    ffi_status status = ffi_prep_cif(
      &cif,
      FFI_DEFAULT_ABI,
      sig->params.count,
      ret,
      param_types
    );

    if (status != FFI_OK) {
      fprintf(stderr, "[ERROR]: Could not initialize FFI CIF\n");
      return 1;
    }

    symbol->addr = p->externs.count; 
    da_append(&p->externs, cif);
  } else {
    // patch symbol table
    symbol->addr = p->code.count;

    // store parameters in local variables
    for (int i = sig->params.count; i > 0; i--) {
      da_append(&p->code, INST_STORE);
      da_append(&p->code, i);
    }

  }

  return 0;
}

static inline int compile_func_decl(program_t* p, ast_decl_t* d) {
  da_foreach(ast_stmt_t*, stmt, &d->body->stmts)
    if(compile_stmt(p, d->def->as.fun.scope, *stmt)) return 1;

  return 0;
}

static inline int compile(program_t* p, ast_root_t* root) {
  da_foreach(ast_def_t*, d, &root->defs) {
    switch((*d)->kind) {
      case DEF_KIND_FUNC:
        if(compile_func_def(p, *d)) return 1;
        break;
      case DEF_KIND_VAR:
        TODO("compile_var_def");
        abort();
      default:
        UNREACHABLE("compile");
    }
  }
  da_foreach(ast_decl_t*, d, &root->top_level) {
    switch((*d)->def->kind) {
      case DEF_KIND_FUNC:
        if(compile_func_decl(p, *d)) return 1;
        break;
      case DEF_KIND_VAR:
        TODO("compile_var_decl");
        abort();
      default:
        UNREACHABLE("compile");
    }
  }

  return 0;
}

static inline void print_data(FILE* stream, constant_t c) {
  switch(c.kind) {
    case DK_NUMBER:
      if (c.as.number.ti & TI_LONG) {
        if (c.as.number.ti & TI_REAL) {
          fprintf(stream, "%lf", c.as.number.r);
        } else if (c.as.number.ti & TI_UNSIGNED) {
          fprintf(stream, "%lu", c.as.number.u);
        } else {
          fprintf(stream, "%ld", c.as.number.i);
        }
      } else {
        if (c.as.number.ti & TI_REAL) {
          fprintf(stream, "%f", (float)c.as.number.r);
        } else if (c.as.number.ti & TI_UNSIGNED) {
          fprintf(stream, "%u", (uint32_t)c.as.number.u);
        } else {
          fprintf(stream, "%d", (int32_t)c.as.number.i);
        }
      }
      break;
    case DK_STR:
      char* cursor = (char*)c.as.s;
      fprintf(stream, "\"");
      while(*cursor) {
        switch(*cursor) {
          case '\a': fprintf(stream, "\\a"); break;
          case '\b': fprintf(stream, "\\b"); break;
          case '\f': fprintf(stream, "\\f"); break;
          case '\n': fprintf(stream, "\\n"); break;
          case '\t': fprintf(stream, "\\t"); break;
          case '\r': fprintf(stream, "\\r"); break;
          case '\v': fprintf(stream, "\\v"); break;
          default:
            fprintf(stream, "%c", *cursor);
            break;
        }
        cursor++;
      }
      fprintf(stream, "\"");
      break;
    default:
      UNREACHABLE("print_data");
  }
}

static inline void print_disass(FILE* stream, program_t* p, scope_t* root) {
  for (size_t i = 0; i < p->code.count; i++) {
    da_foreach(symbol_t, s, &root->symbols) {
      if(i == s->addr && s->kind == SYMB_FUNC && s->storage != STO_EXTERN)
        fprintf(stream, "function <%s>:\n", s->name);
    }
    fprintf(stream, "  0x%08lX", i);
    switch(p->code.items[i]) {
      case INST_NOP:
        fprintf(stream, "  %-10s\n", "NOP"); break;
      case INST_PUSH:
        fprintf(stream, "  %-10s 0x%08X\n", "PUSH", p->code.items[++i]); break;
      case INST_PUSHL:
        fprintf(stream, "  %-10s 0x%016lX\n", "PUSHL", ((uint64_t)p->code.items[++i] << 32) | p->code.items[++i]); break;
      case INST_POP:
        fprintf(stream, "  %-10s\n", "POP"); break;
      case INST_LOAD:
        fprintf(stream, "  %-10s 0x%08X\n", "LOAD", p->code.items[++i]); break;
      case INST_LOADG:
        fprintf(stream, "  %-10s 0x%08X\n", "LOADG", p->code.items[++i]); break;
      case INST_LOADC: {
        uint32_t op = p->code.items[++i];
        fprintf(stream, "  %-10s 0x%08X        ", "LOADC", op);
        fprintf(stream, "    ->    ");
        print_data(stream, p->constants.items[op]);
        fprintf(stream, "\n");
        break;
      }
      case INST_STORE:
        fprintf(stream, "  %-10s 0x%08X\n", "STORE", p->code.items[++i]); break;
      case INST_STOREG:
        fprintf(stream, "  %-10s 0x%08X\n", "STOREG", p->code.items[++i]); break;
      case INST_CALL: {
        uint32_t op = p->code.items[++i];
        fprintf(stream, "  %-10s 0x%08X        ", "CALL", op);
        da_foreach(symbol_t, s, &root->symbols) {
          if(op == s->addr && s->kind == SYMB_FUNC && s->storage != STO_EXTERN) {
            fprintf(stream, "    ->    ");
            fprintf(stream, "<%s>\n", s->name);
          }
        }
        break;
      }
      case INST_HOSTCALL: {
        uint32_t op = p->code.items[++i];
        fprintf(stream, "  %-10s 0x%08X        ", "HOSTCALL", op);
        da_foreach(symbol_t, s, &root->symbols) {
          if(op == s->addr && s->kind == SYMB_FUNC && s->storage == STO_EXTERN) {
            fprintf(stream, "    ->    ");
            fprintf(stream, "<extern::%s>\n", s->name);
          }
        }
        break;
      }
      case INST_RET:
        fprintf(stream, "  %-10s\n", "RET"); break;
      case INST_COUNT:
      default:
        UNREACHABLE("print_disass");
    }
  }
}

int main() {
  tokenizer_t tok = { 0 };
  sb_append(&tok.source, source);

  parser_t parser = { 0 };

  program_t program = { 0 };

  printf("%s\n\n", source);

  if(tok_tokenize(&tok)) return 1;

  // da_foreach(token_t, t, &tok.tokens) {
  //   tok_print(*t);
  // }

  ast_root_t* root = parse(&parser, &tok);
  if(!root) return 1;

  print_ast(stdout, (ast_node_t*)root, 0);

  // print_symbol_table(stdout, root->scope);

  compile(&program, root);

  print_disass(stdout, &program, root->scope);

  tok_destroy(&tok);

  return 0;
}
