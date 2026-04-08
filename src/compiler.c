#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdarg.h>

#include <ffi.h>

#include "sb.h"
#include "da.h"
#include "arena.h"
#include "slice.h"

#include "types.h"
#include "macros.h"

#include "vm.h"

const char* tok_keywords[] = {
  [KW_RETURN]    = "return",
  [KW_EXTERN]    = "extern",
  [KW_EXPORT]    = "export",
  [KW_CONST]     = "const",
  [KW_IF]        = "if",
  [KW_ELSE]      = "else",
  [KW_WHILE]     = "while",
  [KW_IMPL]      = "impl",
  [KW_INTERFACE] = "interface",
  [KW_MOD]       = "mod",
  [KW_STRUCT]    = "struct",
};

typedef enum {
  DIAG_DEBUG = 0,
  DIAG_INFO,
  DIAG_WARN,
  DIAG_ERROR,
  DIAG_FATAL,
} diag_lvl_t;

const char* diag_lvl_txt[] = {
  [DIAG_DEBUG] = "[DEBUG]",
  [DIAG_INFO]  = "[INFO]",
  [DIAG_WARN]  = "[WARN]",
  [DIAG_ERROR] = "[ERROR]",
  [DIAG_FATAL] = "[FATAL]",
};

const char* diag_lvl_color[] = {
  [DIAG_DEBUG] = "\033[35;1m",
  [DIAG_INFO]  = "\033[34;1m",
  [DIAG_WARN]  = "\033[33;1m",
  [DIAG_ERROR] = "\033[31;1m",
  [DIAG_FATAL] = "\033[41;1m",
};

void sb_vdiagf(sb_t* sb, diag_lvl_t lvl, loc_t loc, const char* fmt, va_list args) {
  sb_append(sb, diag_lvl_color[lvl]);
  sb_append(sb, diag_lvl_txt[lvl]);
  sb_append(sb, "\033[0m");

  sb_append(sb, " ");

  int n = vsnprintf(NULL, 0, fmt, args);
  // TODO: malloc layer?
  char* buf = malloc(n * sizeof(*buf));
  n = vsnprintf(buf, n + 1, fmt, args);

  sb_n_append(sb, buf, n);
  // free(buf);

  sb_appendf(sb, "\n");
  sb_appendf(sb, "%s:%zu:%zu", loc.path, loc.line, loc.col);
  sb_appendf(sb, "\n");
  sb_appendf(sb, "%6zu | ", loc.line);
  sb_appendf(sb, "%.*s", SLICE_FMT(loc.line_view));
  sb_appendf(sb, "\n");
  sb_appendf(sb, "%*s | %*s^", 6, "", loc.col - 1, "");
}

void fdiagf(FILE* stream, diag_lvl_t lvl, loc_t loc, const char* fmt, ...) {
  va_list args;
  sb_t sb = { 0 };

  va_start(args, fmt);
  sb_vdiagf(&sb, lvl, loc, fmt, args);
  va_end(args);

  fprintf(stream, "%.*s\n", SB_FMT(sb));

  sb_free(&sb);
}

void tok_print(token_t tok) {
  printf("%3zu:%3zu: ", tok.loc.line, tok.loc.col);
  if(tok.kind < 256) printf("%-20c", tok.kind);
  else {
    switch(tok.kind) {
      case TOK_EOF: printf("%-20s", "TOK_EOF"); break;
      case TOK_IDENT: printf("%-20s", "TOK_IDENT"); break;
      case TOK_ARROW: printf("%-20s", "TOK_ARROW"); break;
      case TOK_COLCOL: printf("%-20s", "TOK_COLCOL"); break;
      case TOK_EQEQ: printf("%-20s", "TOK_EQEQ"); break;
      case TOK_GEQ: printf("%-20s", "TOK_GEQ"); break;
      case TOK_LEQ: printf("%-20s", "TOK_LEQ"); break;
      case TOK_INTLIT: printf("%-20s", "TOK_INTLIT"); break;
      case TOK_REALLIT: printf("%-20s", "TOK_REALLIT"); break;
      case TOK_STRLIT: printf("%-20s", "TOK_STRLIT"); break;
      case TOK_RETURN: printf("%-20s", "TOK_RETURN"); break;
      case TOK_EXTERN: printf("%-20s", "TOK_EXTERN"); break;
      case TOK_EXPORT: printf("%-20s", "TOK_EXPORT"); break;
      case TOK_CONST: printf("%-20s", "TOK_CONST"); break;
      case TOK_IF: printf("%-20s", "TOK_IF"); break;
      case TOK_ELSE: printf("%-20s", "TOK_ELSE"); break;
      case TOK_WHILE: printf("%-20s", "TOK_WHILE"); break;
      case TOK_IMPL: printf("%-20s", "TOK_IMPL"); break;
      case TOK_INTERFACE: printf("%-20s", "TOK_INTERFACE"); break;
      case TOK_MOD: printf("%-20s", "TOK_MOD"); break;
      case TOK_STRUCT: printf("%-20s", "TOK_STRUCT"); break;
      default: printf("%-20s", "<INVALID TOKEN>"); break;
    }
  }
  fprintf(stderr, "%.*s\n", SLICE_FMT(tok.view));
};

const char* tok_kind_str(arena_t* arena, tok_kind_t k) {
  if(k < 256) return arena_sprintf(arena, "`%c`", k);
  else {
    switch(k) {
      case TOK_EOF: return "EOF";
      case TOK_IDENT: return "identifier";
      case TOK_ARROW: return "->";
      case TOK_COLCOL: return "::";
      case TOK_EQEQ: return "==";
      case TOK_GEQ: return ">=";
      case TOK_LEQ: return "<=";
      case TOK_INTLIT: return "integer literal";
      case TOK_REALLIT: return "real literal";
      case TOK_STRLIT: return "string literal";
      case TOK_RETURN:
      case TOK_EXTERN:
      case TOK_EXPORT:
      case TOK_CONST:
      case TOK_IF:
      case TOK_ELSE:
      case TOK_WHILE:
      case TOK_IMPL:
      case TOK_INTERFACE:
      case TOK_MOD:
      case TOK_STRUCT:
        return tok_keywords[k];
      default: return "<INVALID TOKEN>";
    }
  }
}

static inline const char* tok_string(tokenizer_t* t, char** end, loc_t loc) {
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
          fdiagf(stderr, DIAG_ERROR, loc, "Unknown escape sequence");
          return NULL;
      }

      escape = 0;
    } else {
      if      (**end == '\"') break;
      else if (**end == '\\') escape = 1;
      else if (**end == '\n') {
        fdiagf(stderr, DIAG_ERROR, loc, "Missing closing `\"`");
        return NULL;
      }
      else
        sb_appendf(&sb, "%c", **end);
    }
    loc.col++;
  }

  const char* ret = arena_sprintf(&t->arena, "%.*s", (int)sb.count, sb.items);
  sb_free(&sb);

  (*end)++;
  return ret;
}

// TODO: support exp notation for reals 
// TODO: fix real number tokenization: 69.str() should be TOK_INTLIT '.' TOK_IDENT '( ')', not TOK_REALLIT TOK_IDENT...
static inline int tok_num_literal(tokenizer_t* t, char** end, loc_t loc, token_t* tok) {
  int base = 10;
  int64_t integer = 0;
  double real = 0.0;
  int sign = 1;

  char* start = *end;

  lit_type_info_flags_t type_info = 0;

  // TODO: N-2 now gets tokenized as TOK_ID '-2', instead of
  // TOK_ID '-' '2', unless '-' and '2' are separated by a space
  if (**end == '-') { sign = -1; (*end)++; }

  if (**end == '0') {
    switch (*(*end + 1)) {
      case 'x': base = 16; (*end)+=2; loc.col+=2; break;
      case 'b': base = 2;  (*end)+=2; loc.col+=2; break;
      case 'o': base = 8;  (*end)+=2; loc.col+=2; break;
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
    loc.col++;
  }

  if (type_info & TI_REAL) {
    real = (double)integer;

    int64_t power = 10;

    while(isdigit(*++(*end))) {
      loc.col++;
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
      fdiagf(stderr, DIAG_ERROR, loc, "Cannot declare a negative unsigned literal.");
      return 1;
    }

    if (type_info & TI_REAL) {
      fdiagf(stderr, DIAG_ERROR, loc, "Cannot declare an unsigned real literal.");
      return 1;
    }
    tok->as.u = integer;
  } else {
    tok->as.i = sign * integer;
  }

  tok->kind = type_info & TI_REAL ? TOK_REALLIT : TOK_INTLIT;
  tok->type_info = type_info;
  tok->view = (slice_t){ .data = start, .size = (int)(*end - start)};

  return 0;
}

static inline int tok_tokenize(tokenizer_t* t) {
  token_t tok = { 0 };
  size_t line = 1;
  size_t col  = 1;
  slice_t line_view    = { 0 };
  slice_t src_view     = t->source;
  slice_t src_rem_view = t->source;

  line_view    = slice_sub(src_rem_view, 0, slice_index_of(src_view, '\n'));
  src_rem_view = slice_advance_after(src_rem_view, "\n");

  char* p = (char*)t->source.data;
  while(*p && ((size_t)(p - t->source.data) < t->source.size)) {
    // skip comments
    if(*p == '#') while(*p && *p++ != '\n');

    if(*p == '\n') {
      line_view    = slice_sub(src_rem_view, 0, slice_index_of(src_rem_view, '\n'));
      src_rem_view = slice_advance_after(src_rem_view, "\n");

      line++;
      p++;
      col = 1;
      continue;
    }
    if(isspace(*p)) { p++; col++; continue; }

    loc_t loc = (loc_t){
      .line      = line,
      .col       = col,
      .line_view = line_view,
      .path      = t->path,
    };

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
      case '%':
        tok = (token_t){ .kind = *p, .view = { .data = p, .size = 1 } }; 
        break;
      case '>':
        switch(*(p + 1)) {
          case '=': tok = (token_t){ .kind = TOK_GEQ, .view = { .data = p, .size = 2 } }; break;
          default: 
            tok = (token_t){ .kind = *p, .view = { .data = p, .size = 1 } };
            break;
        }
        break;
      case '<':
        switch(*(p + 1)) {
          case '=': tok = (token_t){ .kind = TOK_LEQ, .view = { .data = p, .size = 2 } }; break;
          default: 
            tok = (token_t){ .kind = *p, .view = { .data = p, .size = 1 } };
            break;
        }
        break;
      case '=':
        switch(*(p + 1)) {
          case '=': tok = (token_t){ .kind = TOK_EQEQ, .view = { .data = p, .size = 2 } }; break;
          default: 
            tok = (token_t){ .kind = *p, .view = { .data = p, .size = 1 } };
            break;
        }
        break;
      case ':':
        switch(*(p + 1)) {
          case ':': tok = (token_t){ .kind = TOK_COLCOL, .view = { .data = p, .size = 2 } }; break;
          default: 
            tok = (token_t){ .kind = *p, .view = { .data = p, .size = 1 } };
            break;
        }
        break;
      case '.':
        if (isdigit(*(p + 1))) { 
          char* end = p;
          if(tok_num_literal(t, &end, loc, &tok)) return 1;
          break;
        } 

        tok = (token_t){ .kind = *p, .view = { .data = p, .size = 1 } };
        break;
      case '-':
        if (isdigit(*(p + 1))) { 
          char* end = p;
          if(tok_num_literal(t, &end, loc, &tok)) return 1;
          break;
        }

        switch(*(p + 1)) {
          case '>': tok = (token_t){ .kind = TOK_ARROW, .view = { .data = p, .size = 2 } }; break;
          // case '=': tok = (token_t){ .kind = TOK_MINEQS }; break;
          default: 
            tok = (token_t){ .kind = *p, .view = { .data = p, .size = 1 } };
            break;
        }
        break;

      default:
        if(isalpha(*p) || *p == '_') {
          uint8_t kw = 0;
          for(size_t i=0; i < sizeof(tok_keywords)/sizeof(*tok_keywords); i++) {
            if (!tok_keywords[i]) continue;
            int kw_len = strlen(tok_keywords[i]);
            if(memcmp(p, tok_keywords[i], kw_len) == 0) {
              kw = 1;
              tok = (token_t){ .kind = i, .view = { .data = p, .size = kw_len } };
              break;
            }
          }
          if(kw) break;

          char* end = p;
          for (; *end && (isalpha(*end) || isdigit(*end) || *end == '_'); end++);
          tok = (token_t){
            .kind = TOK_IDENT,
            .view = { .data = p, .size = (int)(end - p) },
            .as.s = arena_sprintf(&t->arena, "%.*s", (int)(end - p), p),
          };
        } else if (isdigit(*p)) {
          char* end = p;
          if(tok_num_literal(t, &end, loc, &tok)) return 1;

        } else if (*p == '"') {
          char* end = p;
          const char* str = tok_string(t, &end, loc);
          if (!str) return 1;
          tok = (token_t){ 
            .kind = TOK_STRLIT,
            .view = { .data = p, .size = (int)(end - p) },
            .as.s = str
          };
        } else {
          fdiagf(stderr, DIAG_ERROR, loc, "Tokenizer: - Unrecognized token");
          // TODO: continue instead of returning
          return 1;
        }
    }

    tok.loc = loc; 
    da_append(&t->tokens, tok);

    p   += tok.view.size;
    col += tok.view.size;
  }

  da_append(&t->tokens, ((token_t){ .kind = TOK_EOF, .loc = { .line = line, .col = col, .line_view = line_view }}));
  return 0;
}

void tok_destroy(tokenizer_t* t) {
  da_free(t->tokens);
  arena_free(&t->arena);
}

static inline token_t get_tok(parser_t* p) {
  if (p->current >= p->tokens.count) {
    fdiagf(stderr, DIAG_FATAL, p->tokens.items[p->current].loc, "No more tokens.");
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
    fdiagf(stderr, DIAG_ERROR, t.loc, "Expected %s, found %s.", exp_name, tok_kind_str(&p->arena, t.kind));
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
}

static inline void print_type(FILE* stream, const type_t* t);
static inline void render_type(sb_t* sb, const type_t* t);
static inline const char* type_to_str(arena_t* a, const type_t* t);
static inline const char* qn_to_str(arena_t* a, const ast_qn_t* qn); 

static inline void set_symbol_address(symbol_t* s, uint32_t addr) {
  s->addr = addr;
  s->addr_resolved = true;
}

static inline void print_symbol_table(FILE* stream, scope_t* root);

static inline symbol_t* make_symbol(arena_t* arena, scope_t* scope, symb_kind_t kind, symb_storage_t storage, const char* name, type_t* type) {
  symbol_t* s = arena_alloc(arena, sizeof(*s));
  s->name = name;
  s->kind = kind;
  s->storage = storage;
  s->type = type;
  da_append(&scope->symbols, s);
  return s;
}

static inline symbol_t* make_method(arena_t* arena, const type_t* owner, const char* name, const type_t* type, symb_storage_t storage) {
  return make_symbol(arena, owner->scope, SYMB_FUNC, storage, name, (type_t*)type);
}

static inline symbol_t* resolve_symbol_local_any(scope_t* scope, ast_qn_t* name) { 
  if (!scope) return NULL;
  if (!name) return NULL;

  symbol_t* symb = NULL;
  // OPTIMIZE
  da_foreach(symbol_t*, s, &scope->symbols) {
    if(strcmp((*s)->name, name->name) == 0) { symb = *s; break; }
  }

  if (name->next && symb->kind != SYMB_TYPE) return NULL;
  if (name->next) return resolve_symbol_local_any(symb->type->scope, name->next);

  return symb;
}

static inline symbol_t* resolve_field(scope_t* scope, const char* name) {
  if (!scope) return NULL;
  if (!name) return NULL;

  da_foreach(symbol_t*, s, &scope->symbols) {
    if(strcmp((*s)->name, name) == 0) return *s;
  }
  
  return NULL;
}

static inline int type_equals(type_t a, type_t b);

static inline symbol_t* resolve_symbol_local(scope_t* scope, ast_qn_t* name, symb_kind_t kind) { 
  if (!scope) return NULL;
  if (!name) return NULL;

  symbol_t* symb = NULL;
  if (name->next) {
    // OPTIMIZE
    da_foreach(symbol_t*, s, &scope->symbols) {
      if(strcmp((*s)->name, name->name) == 0 && (*s)->kind == SYMB_TYPE) { symb = *s; break; }
    }

    if (!symb) return NULL;

    return resolve_symbol_local(symb->type->scope, name->next, kind);
  }

  // OPTIMIZE
  da_foreach(symbol_t*, s, &scope->symbols) {
    if(strcmp((*s)->name, name->name) == 0 && (*s)->kind == kind) return *s;
  }

  return NULL;
}

static inline symbol_t* resolve_qn_root(scope_t* scope, const char* name) {
  if (!scope) return NULL;
  if (!name) return NULL;

  da_foreach(symbol_t*, s, &scope->symbols) {
    if((*s)->name && strcmp((*s)->name, name) == 0) return *s;
  }

  return resolve_qn_root(scope->parent, name);
}

static inline symbol_t* resolve_symbol_any(scope_t* scope, ast_qn_t* name) {
  if (!scope) return NULL;
  if (!name) return NULL;

  symbol_t* symb = resolve_qn_root(scope, name->name);
  if (!symb) return NULL;

  if (name->next) return resolve_symbol_local_any(symb->type->scope, name->next);

  return symb;
}

static inline symbol_t* resolve_symbol(scope_t* scope, ast_qn_t* name, symb_kind_t kind) {
  if (!scope) return NULL;
  if (!name) return NULL;

  symbol_t* symb = resolve_qn_root(scope, name->name);
  if (!symb) return NULL;

  if (name->next) return resolve_symbol_local(symb->type->scope, name->next, kind);

  return symb->kind == kind ? symb : NULL;
}

static inline void enter_scope_new(typechecker_t* t, const char* name) {
  scope_t* parent = t->current_scope;
  t->current_scope = arena_alloc(&t->arena, sizeof(scope_t));
  t->current_scope->name = name;
  t->current_scope->parent = parent;
  // fprintf(stderr, "[DEBUG] New scope %s, son of %s.\n", name, t->current_scope->parent ? t->current_scope->parent->name : "noone");
}

static inline void enter_scope(typechecker_t* t, scope_t* s) {
  t->current_scope = s;
  // fprintf(stderr, "[DEBUG] Entering scope %s\n", s->name);
}

static inline void exit_scope(typechecker_t* t) {
  // fprintf(stderr, "[DEBUG] Exiting scope %s (entering %s)\n", t->current_scope->name, t->current_scope->parent->name);
  t->current_scope = t->current_scope->parent;
}

static inline const char* attributes_get(ast_attr_list_t* l, const char* key) {
  if(!l) return NULL;
  // OPTIMIZE
  da_foreach(struct _attribute, d, &l->attrs) {
    if(strcmp(d->key, key) == 0) return d->value;
  }
  return NULL;
}

static inline bool tok_is_specifier(parser_t* p) {
  switch((kw_kind_t)get_tok(p).kind) {
    case KW_EXTERN:
    case KW_EXPORT:
    case KW_CONST:
      return true;
    case KW_RETURN:
    case KW_IF:
    case KW_ELSE:
    case KW_WHILE:
    case KW_IMPL:
    case KW_INTERFACE:
    case KW_MOD:
    case KW_STRUCT:
    default: 
      return false;
  }
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
      case KW_CONST: cur_flag = SPEC_CONST; break;
      case KW_RETURN:
      case KW_IF:
      case KW_ELSE:
      case KW_WHILE:
      case KW_IMPL:
      case KW_INTERFACE:
      case KW_MOD:
      case KW_STRUCT:
      default: return flags;
    }

    if (flags & cur_flag)
      fdiagf(stderr, DIAG_WARN, t.loc, "Duplicate specifier `%.*s`, will be considered as one.\n", SLICE_FMT(t.view));

    flags |= cur_flag;

    next(p);
  }

  return flags;
}

const enum _bp expr_bp_table[] = {
  [OP_INVALID] = BP_NONE,
  [OP_EQ]      = BP_EQ, 
  [OP_LEQ]     = BP_REL,
  [OP_GEQ]     = BP_REL,
  [OP_LT]      = BP_REL, 
  [OP_GT]      = BP_REL, 
  [OP_PLUS]    = BP_ADD,
  [OP_MINUS]   = BP_ADD, 
  [OP_MULT]    = BP_MULT,
  [OP_DIV]     = BP_MULT,
  [OP_REM]     = BP_MULT,
  [OP_CALL]    = BP_CALL,
  [OP_ASSIGN]  = BP_ASSIGN,
  [OP_MEMB]    = BP_ACCESS,
};

static inline ast_qn_t* parse_qualified_name(parser_t* p) {
  if(!expect(p, TOK_IDENT)) return NULL;
  token_t tok = get_tok(p);
  next(p);
  
  ast_qn_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->name = arena_strdup(&p->arena, tok.as.s);

  if (tok_is(p, TOK_COLCOL)) {
    next(p);

    ast_qn_t* next = parse_qualified_name(p);
    if (!next) return NULL;
    n->next = next;
  }

  return n;
}

static inline ast_expr_t* parse_expr(parser_t* p, int bp);

static inline ast_expr_t* parse_primary_expr(parser_t* p) {
  ast_expr_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_EXPR;
  n->loc = get_tok(p).loc;

  if (tok_is(p, TOK_IDENT)) {
    n->kind = EXPR_SYMBOL;

    ast_qn_t* name = parse_qualified_name(p);
    if(!name) return NULL;
    n->as.symbol = name;

  } else if (tok_is(p, TOK_STRLIT)) {
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
  loc_t loc = get_tok(p).loc;

  for(;;) {
    op_kind_t op = OP_INVALID;
    ast_expr_kind_t kind = EXPR_SUBEXPR; // ok?

    // NOTE: enum contains tokens
    switch((op_kind_t)get_tok(p).kind) {
      case OP_PLUS:
      case OP_MINUS: 
      case OP_MULT: 
      case OP_DIV:
      case OP_REM:
      case OP_EQ:
      case OP_LEQ:
      case OP_GEQ:
      case OP_LT:
      case OP_GT:
        kind = EXPR_BINOP;
        break;
      case OP_CALL: 
        kind = EXPR_FUNCALL;
        break;
      case OP_ASSIGN:
        kind = EXPR_ASSIGNMENT;
        break;
      case OP_MEMB: 
        kind = EXPR_ACCESS;
        break;
      case OP_INVALID:
      default:
        return lhs;
    }
    op = (op_kind_t)get_tok(p).kind;

    int curr_bp = expr_bp_table[op];
    if (curr_bp < bp)
      return lhs;

    if(!expect(p, (tok_kind_t)op)) return NULL;
    next(p);

    switch(kind) {
      case EXPR_BINOP: {
        ast_expr_t* rhs = parse_expr(p, curr_bp + 1);
        if(!rhs) return NULL;

        ast_expr_t *n = arena_alloc(&p->arena, sizeof(*n));
        n->ast_kind = AST_EXPR;
        n->kind = kind;
        n->loc = loc;
        n->as.binop.lhs = lhs;
        n->as.binop.rhs = rhs;
        n->as.binop.op = op;

        lhs = n;
        break;
      }
      case EXPR_UNOP:
        TODO("parse_expr: EXPR_UNOP");
      case EXPR_ACCESS: {
        if (!expect(p, TOK_IDENT)) return NULL;

        ast_expr_t* n = arena_alloc(&p->arena, sizeof(*n));
        n->ast_kind = AST_EXPR;
        n->loc = get_tok(p).loc;
        n->kind = kind;

        n->as.access.owner = lhs;
        n->as.access.field = arena_strdup(&p->arena, get_tok(p).as.s);
        n->as.access.op    = op;

        next(p);

        lhs = n;
        break;
      }
      case EXPR_FUNCALL: {
        ast_expr_t* n = arena_alloc(&p->arena, sizeof(*n));
        n->ast_kind = AST_EXPR;
        n->loc = loc;
        n->kind = kind;

        if (!tok_is(p, ')')) {
          ast_expr_t* arg = parse_expr(p, 0);
          if(!arg) return NULL;
          da_append(&n->as.funcall.args, arg);

          while(tok_is(p, ',')) {
            if(!expect(p, ',')) return NULL;
            next(p);

            arg = parse_expr(p, 0);
            if(!arg) return NULL;
            da_append(&n->as.funcall.args, arg);
          }
        }
        if(!expect(p, ')')) return NULL;
        next(p);

        n->as.funcall.callee = lhs;

        lhs = n;
        break;
      }
      case EXPR_ASSIGNMENT: {
        ast_expr_t* rhs = parse_expr(p, curr_bp + 1);
        if(!rhs) return NULL;

        ast_expr_t *n = arena_alloc(&p->arena, sizeof(*n));
        n->ast_kind = AST_EXPR;
        n->loc = loc;
        n->kind = kind;
        n->as.assign.lhs = lhs;
        n->as.assign.rhs = rhs;

        lhs = n;
        break;
      }
      case EXPR_SYMBOL:
      case EXPR_STRING:
      case EXPR_NUMBER:
      case EXPR_SUBEXPR:
      default:
        UNREACHABLE("parse_expr");
    }
  }
}

// TODO: allow scope access (types defined in other modules)
static inline ast_type_t* parse_type(parser_t* p) {
  ast_type_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_TYPE;
  n->loc = get_tok(p).loc;

  ast_qn_t* name = parse_qualified_name(p);
  if(!name) return NULL;
  n->name = name;

  n->array_depth = 0;
  while(tok_is(p, '[')) {
    if (!expect(p, '[')) return NULL;
    next(p);
    if (!expect(p, ']')) return NULL;
    next(p);
    n->array_depth++;
  }

  return n;
}

static inline ast_func_def_t* parse_func_def(parser_t* p, loc_t loc, const char* name, spec_flags_t flags, ast_attr_list_t* attributes);
static inline ast_var_def_t* parse_var_def(parser_t* p, loc_t loc, const char* name, spec_flags_t flags, bool global, ast_attr_list_t* attributes);
static inline ast_body_t* parse_body(parser_t* p);

// stmt = return [ expr ] ';' 
//      | if '(' expr ')' body [ else body ]
//      | specifiers var_def
//      | expr ';'
//      | ';'
static inline ast_stmt_t* parse_stmt(parser_t* p) {
  ast_stmt_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_STMT;
  n->loc = get_tok(p).loc;

  if(tok_is(p, ';')) {
    if(!expect(p,';')) return NULL;
    next(p);

    n->kind = STMT_EMPTY;
    return n;
 
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
    return n;

  } else if (tok_is(p, TOK_IF)) {
    if(!expect(p, TOK_IF)) return NULL;
    next(p);
    n->kind = STMT_IF;

    ast_expr_t* expr = parse_expr(p, 0);
    if(!expr) return NULL;
    n->as.if_else.cond = expr;

    ast_body_t* if_body = parse_body(p);
    if(!if_body) return NULL;
    n->as.if_else.if_body = if_body;

    if(tok_is(p, TOK_ELSE)) {
      if(!expect(p, TOK_ELSE)) return NULL;
      next(p);

      ast_body_t* else_body = parse_body(p);
      if(!else_body) return NULL;
      n->as.if_else.else_body = else_body;
    }
    // TODO: handle else if (elif)

    return n;
  
  } else if (tok_is(p, TOK_WHILE)) {
    if(!expect(p, TOK_WHILE)) return NULL;
    next(p);
    n->kind = STMT_WHILE; 

    ast_expr_t* expr = parse_expr(p, 0);
    if(!expr) return NULL;
    n->as.while_loop.cond = expr;

    ast_body_t* body = parse_body(p);
    if(!body) return NULL;
    n->as.while_loop.body = body;

    return n;

  } else if (tok_is(p, TOK_IDENT) || tok_is_specifier(p)) {
    size_t saved = p->current;
    spec_flags_t flags = 0;
    if(tok_is_specifier(p)) flags = parse_specifiers(p);

    if(!expect(p, TOK_IDENT)) return NULL;
    const char* name = get_tok(p).as.s;
    next(p);

    if(tok_is(p, ':')) {
      loc_t loc = get_tok(p).loc;
      if(!expect(p, ':')) return NULL;
      next(p);

      n->kind = STMT_VAR_DEF;

      ast_var_def_t* var_def = parse_var_def(p, loc, name, flags, false, NULL);
      if(!var_def) return NULL;

      n->as.var_def = var_def;
      return n;
    }

    p->current = saved;
  }

  n->kind = STMT_EXPR;
  ast_expr_t* e = parse_expr(p, 0);
  if(!e) return NULL;
  n->as.expression = e; 
  if(!expect(p,';')) return NULL;
  // DIAGF(p, WARN, "Unused expression result");
  next(p);
  return n;
}

// body = '{' ( stmt )* '}'
static inline ast_body_t* parse_body(parser_t* p) {
  ast_body_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_BODY;
  n->loc = get_tok(p).loc;

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

// TODO: convert param to variable definition
// param = ident ':' ident
static inline ast_param_t* parse_param(parser_t* p) {
  ast_param_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_PARAM;
  n->loc = get_tok(p).loc;

  if(!expect(p, TOK_IDENT)) return NULL;
  n->name = arena_strdup(&p->arena, get_tok(p).as.s);
  next(p);

  if(!expect(p, ':')) return NULL;
  next(p);

  ast_type_t* t = parse_type(p);
  if(!t) return NULL;
  n->type = t;

  return n;
}

// sig = '(' [ param ( ',' param )* ] ')' -> ident 
static inline ast_sig_t* parse_signature(parser_t* p) {
  ast_sig_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_SIG;
  n->loc = get_tok(p).loc;

  if(!expect(p, '(')) return NULL;
  next(p);

  if (tok_is(p, ')')) {
    if(!expect(p, ')')) return NULL;
    next(p);
  } else { 
    ast_param_t* param = parse_param(p);
    if(!param) return NULL;

    da_append(&n->params, param);

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

  return n;
}

// func_def = type [ body ]
static inline ast_func_def_t* parse_func_def(parser_t* p, loc_t loc, const char* name, spec_flags_t flags, ast_attr_list_t* attributes) {
  ast_func_def_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_FUNC_DEF;
  n->flags = flags;
  n->name = arena_strdup(&p->arena, name);
  n->loc = loc;

  n->scope = p->current_scope;

  n->sig = parse_signature(p);
  if(!n->sig) return NULL;

  if (tok_is(p, ';')) {
    if(!expect(p, ';')) return NULL;
    next(p);
  } else {
    ast_body_t* body = parse_body(p);
    if(!body) return NULL;

    n->body = body;
  }

  n->attributes = attributes;

  return n;
}

// var_def = type [ '=' expr ] ';'
static inline ast_var_def_t* parse_var_def(parser_t* p, loc_t loc, const char* name, spec_flags_t flags, bool global, ast_attr_list_t* attributes) {
  ast_var_def_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_VAR_DEF;
  n->flags = flags;
  n->name = arena_strdup(&p->arena, name);
  n->loc = loc;

  symb_storage_t sto = global ? STO_GLOBAL : STO_LOCAL;
  if(flags & SPEC_EXTERN) sto = STO_EXTERN; 
  if(flags & SPEC_EXPORT) sto = STO_EXPORT;

  // TODO:  type inference
  ast_type_t* type = parse_type(p);
  if(!type) return NULL;
  n->type = type;

  if(tok_is(p, '=')) {
    if(!expect(p, '=')) return NULL;
    next(p);
    ast_expr_t* init = parse_expr(p, 0);
    if(!init) return NULL;
    n->init = init;
  }
  if(!expect(p, ';')) return NULL;
  next(p);

  n->attributes = attributes;

  return n;
}

// attr_list: '[' '[' ident '=' TOK_STRLIT ( ',' ident '=' TOK_STRLIT )* ']' ']'
static inline ast_attr_list_t* parse_attr_list(parser_t* p) {
  if(!expect(p, '[')) return NULL;
  next(p);
  if(!expect(p, '[')) return NULL;
  next(p);

  ast_attr_list_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_ATTR_LIST;
  n->loc = get_tok(p).loc;
  
  // NOTE: do not allow empty attribute list
  struct _attribute attr = { 0 };
  if(!expect(p, TOK_IDENT)) return NULL;
  attr.key = arena_strdup(&p->arena, get_tok(p).as.s);
  next(p);

  if(!expect(p, '=')) return NULL;
  next(p);

  if(!expect(p, TOK_STRLIT)) return NULL;
  attr.value = arena_strdup(&p->arena, get_tok(p).as.s);
  next(p);

  da_append(&n->attrs, attr);

  while(!tok_is(p, ']')) {
    if(!expect(p, ',')) return NULL;
    next(p);

    struct _attribute attr = { 0 };
    if(!expect(p, TOK_IDENT)) return NULL;
    attr.key = arena_strdup(&p->arena, get_tok(p).as.s);
    next(p);

    if(!expect(p, '=')) return NULL;
    next(p);

    if(!expect(p, TOK_STRLIT)) return NULL;
    attr.value = arena_strdup(&p->arena, get_tok(p).as.s);
    next(p);

    da_append(&n->attrs, attr);
  }

  if(!expect(p, ']')) return NULL;
  next(p);
  if(!expect(p, ']')) return NULL;
  next(p);

  return n;
}

static inline ast_impl_t* parse_impl(parser_t* p) {
  ast_impl_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_IMPL;
  n->loc = get_tok(p).loc;

  if(!expect(p, TOK_IMPL)) return NULL;
  next(p);

  ast_type_t* interface = parse_type(p);
  if(!interface) return NULL;
  n->interface = interface;

  if(!expect(p, ':')) return NULL;
  next(p);

  ast_type_t* type = parse_type(p);
  if(!type) return NULL;
  n->type = type;

  if(!expect(p, '{')) return NULL;
  next(p);
  
  while(!tok_is(p, '}')) {
    ast_attr_list_t* attributes = NULL;
    if(tok_is(p, '[')) {
      attributes = parse_attr_list(p);
      if(!attributes) return NULL;
    }

    spec_flags_t flags = parse_specifiers(p);

    loc_t loc = get_tok(p).loc;

    if(!expect(p, TOK_IDENT)) return NULL;
    const char* method_name = get_tok(p).as.s;
    next(p);

    if(!expect(p, ':')) return NULL;
    next(p);

    ast_func_def_t* def = parse_func_def(p, loc, method_name, flags, attributes);

    da_append(&n->methods, def);
  }

  if(!expect(p, '}')) return NULL;
  next(p);

  return n;
}

static inline ast_iface_t* parse_iface(parser_t* p, loc_t loc, const char* name) {
  if(!expect(p, TOK_INTERFACE)) return NULL;
  next(p);

  ast_iface_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_IFACE;
  n->name = arena_strdup(&p->arena, name);
  n->loc = loc;

  if(!expect(p, '{')) return NULL;
  next(p);

  while(!tok_is(p, '}')) {
    spec_flags_t flags = parse_specifiers(p);

    loc_t loc = get_tok(p).loc;

    if(!expect(p, TOK_IDENT)) return NULL;
    const char* method_name = get_tok(p).as.s;
    next(p);

    if(!expect(p, ':')) return NULL;
    next(p);

    ast_func_def_t* def = parse_func_def(p, loc, method_name, flags, NULL);

    da_append(&n->methods, def);
  }

  if(!expect(p, '}')) return NULL;
  next(p);

  return n;
}

static inline ast_struct_t* parse_struct(parser_t* p, loc_t loc, const char* name) {
  if (!expect(p, TOK_STRUCT)) return NULL;
  next(p);

  ast_struct_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_STRUCT;
  n->name = arena_strdup(&p->arena, name);
  n->loc = loc;

  if(!expect(p, '{')) return NULL;
  next(p);

  while(!tok_is(p, '}')) {
    ast_param_t* field = parse_param(p);
    if(!field) return NULL;

    if(!expect(p, ';')) return NULL;
    next(p);

    da_append(&n->fields, field);
  }

  if(!expect(p, '}')) return NULL;
  next(p);

  return n;
}

static inline ast_root_t* parse(parser_t* p);

static inline ast_mod_t* parse_mod(parser_t* p, loc_t loc, const char* name) {
  if(!expect(p, TOK_MOD)) return NULL;
  next(p);

  if(!expect(p, '{')) return NULL;
  next(p);

  ast_mod_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_MOD;
  n->loc = loc;
  n->name = arena_strdup(&p->arena, name);

  ast_root_t* root = parse(p);
  if(!root) return NULL;
  n->root = root;

  if(!expect(p, '}')) return NULL;
  next(p);

  return n;
}

// root: ( [ attr_list ] spec_flags ident ':' func_def )* 
//     | ( [ attr_list ] spec_flags ident ':' var_def )*
//     | ( ident ':' 'impl' ident '{' ( ident ':' func_decl func_def '}' )+ )*
static inline ast_root_t* parse(parser_t* p) {
  ast_root_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_ROOT;
  n->scope = p->current_scope;

  while(!tok_is(p, TOK_EOF) && !tok_is(p, '}')) {
    if (tok_is(p, TOK_IMPL)) {
      ast_impl_t* impl = parse_impl(p);
      if(!impl) return NULL;

      da_append(&n->impls, impl);
    } else {
      ast_attr_list_t* attributes = NULL;
      if(tok_is(p, '[')) {
        attributes = parse_attr_list(p);
        if(!attributes) return NULL;
      }

      spec_flags_t flags = parse_specifiers(p);

      loc_t loc = get_tok(p).loc;

      if(!expect(p, TOK_IDENT)) return NULL;
      const char* name = get_tok(p).as.s; 
      next(p);
      if(!expect(p, ':')) return NULL;
      next(p);

      if(tok_is(p, '(')) {
        ast_func_def_t* def = parse_func_def(p, loc, name, flags, attributes);
        if (!def) return NULL;

        da_append(&n->func_defs, def);
      } else if (tok_is(p, TOK_STRUCT)) {
        ast_struct_t* structure = parse_struct(p, loc, name);
        if(!structure) return NULL;

        da_append(&n->structs, structure);
      } else if (tok_is(p, TOK_INTERFACE)) {
        ast_iface_t* interface = parse_iface(p, loc, name);
        if(!interface) return NULL;

        da_append(&n->interfaces, interface);
      } else if (tok_is(p, TOK_MOD)) {
        ast_mod_t* submod = parse_mod(p, loc, name);
        if(!submod) return NULL;

        da_append(&n->submods, submod);
      } else {
        ast_var_def_t* def = parse_var_def(p, loc, name, flags, true, attributes);
        if (!def) return NULL;

        da_append(&n->var_defs, def);
      }
    }
  }

  return n;
}

static inline void parser_init(parser_t* p, const tokenizer_t* t) {
  p->tokens = t->tokens;
  p->current = 0;
}

static inline void print_ast(FILE* stream, ast_node_t* n, int level) {
  switch(n->ast_kind) {
    case AST_ROOT:
      ast_root_t* root = (ast_root_t*)n;
      fprintf(stream, "%*s%s\n", level, "","AST_ROOT");
      fprintf(stream, "%*s%s\n", level + 2, "","DEFINITIONS:");
      da_foreach(ast_func_def_t*, d, &root->func_defs) {
        print_ast(stream, (ast_node_t*)*d, level + 4);
      }
      da_foreach(ast_var_def_t*, d, &root->var_defs) {
        print_ast(stream, (ast_node_t*)*d, level + 4);
      }
      break;
    case AST_QN:
      ast_qn_t* qn = (ast_qn_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_QN");
      fprintf(stream, "%*sName: %s\n", level, "", qn->name);
      print_ast(stream, (ast_node_t*)qn->next, level + 2);
    case AST_EXPR:
      ast_expr_t* expr = (ast_expr_t*)n;
      fprintf(stream, "%*s%s", level, "", "AST_EXPR");
      switch(expr->kind) {
        case EXPR_SYMBOL:
          fprintf(stream, " (%s)\n", "EXPR_SYMBOL");
          print_ast(stream, (ast_node_t*)expr->as.symbol, level + 2);
          break;
        case EXPR_STRING:
          fprintf(stream, " (%s)\n", "EXPR_STRING");
          fprintf(stream, "%*s%s\n", level + 2, "", expr->as.s);
          break;
        case EXPR_NUMBER:
          fprintf(stream, " (%s)\n", "EXPR_NUMBER");
          if (expr->as.number.ti & TI_REAL) {
            fprintf(stream, "%*s%lf\n", level + 2, "", expr->as.number.r);
          } else if (expr->as.number.ti & TI_UNSIGNED) {
            fprintf(stream, "%*s%" PRIu64 " (unsigned)\n", level + 2, "", expr->as.number.u);
          } else {
            fprintf(stream, "%*s%" PRId64 "\n", level + 2, "", expr->as.number.i);
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
        case EXPR_ACCESS:
          fprintf(stream, " (%s)\n", "EXPR_ACCESS");
          fprintf(stream, "%*sobject:\n", level + 2, "");
          print_ast(stream, (ast_node_t*)expr->as.access.owner, level + 2);
          fprintf(stream, "%*sfield:\n", level + 2, "");
          fprintf(stream, "%*s%s\n", level + 2, "", expr->as.access.field);
          break;
        case EXPR_FUNCALL:
          fprintf(stream, " (%s)\n", "EXPR_FUNCALL");
          fprintf(stream, "%*scallee:\n", level + 2, "");
          print_ast(stream, (ast_node_t*)expr->as.funcall.callee, level + 2);
          fprintf(stream, "%*sArgs:\n", level + 2, "");
          da_foreach(ast_expr_t*, a, &expr->as.funcall.args) {
            print_ast(stream, (ast_node_t*)*a, level + 2);
          }
          break;
        case EXPR_SUBEXPR:
          fprintf(stream, " (%s)\n", "EXPR_SUBEXPR");
          print_ast(stream, (ast_node_t*)expr->as.subexpr, level + 2);
          break;
        case EXPR_ASSIGNMENT:
          fprintf(stream, " (%s)\n", "EXPR_ASSIGNMENT");
          fprintf(stream, "%*slhs:\n", level + 2, "");
          print_ast(stream, (ast_node_t*)expr->as.assign.lhs, level + 2);
          fprintf(stream, "%*srhs:\n", level + 2, "");
          print_ast(stream, (ast_node_t*)expr->as.assign.rhs, level + 2);
          break;
        // default: break;
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
        case STMT_EXPR:
          print_ast(stream, (ast_node_t*)stmt->as.expression, level + 2);
          break;
        case STMT_VAR_DEF:
          fprintf(stream, "%*s%s\n", level, "", "STMT_VAR_DEF");
          print_ast(stream, (ast_node_t*)stmt->as.var_def, level + 2);
          break;
        case STMT_IF:
          TODO("print_ast: STMT_IF");
        default: break;
      }
      break;
    case AST_VAR_DEF: {
      ast_var_def_t* def = (ast_var_def_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_VAR_DEF");
      fprintf(stream, "%*sName: %s\n", level + 2, "", def->name);
      fprintf(stream, "%*sFlags: ", level + 2, "");
      if(def->flags & SPEC_EXTERN) fprintf(stream, "extern ");
      if(def->flags & SPEC_EXPORT) fprintf(stream, "export ");
      if(def->flags & SPEC_CONST)  fprintf(stream, "const ");
      fprintf(stream, "\n");
      if(def->init)
        print_ast(stream, (ast_node_t*)def->init, level + 2);
      break;
    }
    case AST_FUNC_DEF: {
      ast_func_def_t* def = (ast_func_def_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_FUNC_DEF");
      fprintf(stream, "%*sName: %s\n", level + 2, "", def->name);
      fprintf(stream, "%*sFlags: ", level + 2, "");
      if(def->flags & SPEC_EXTERN) fprintf(stream, "extern ");
      if(def->flags & SPEC_EXPORT) fprintf(stream, "export ");
      if(def->flags & SPEC_CONST)  fprintf(stream, "const ");
      fprintf(stream, "\n");
      print_ast(stream, (ast_node_t*)def->sig, level + 2);
      if(def->body)
        print_ast(stream, (ast_node_t*)def->body, level + 2);
      break;
    }
    case AST_TYPE:
      fprintf(stream, "%*s%s\n", level, "", "AST_TYPE");
      fprintf(stream, "%*sName:", level + 2, "");
      print_ast(stream, (ast_node_t*)((ast_type_t*)n)->name, level + 2);
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
    case AST_IMPL:
      TODO("print_ast: AST_IMPL");
    default:
      UNREACHABLE("print_ast %d", n->ast_kind);
  }
}

static inline void print_symbol_table_entries(FILE* stream, scope_t* scope) {
  if(!scope) return;
  da_foreach(symbol_t*, s, &scope->symbols) {
    fprintf(stream, "%-30s %-30s ", (*s)->name, scope->name);
    switch((*s)->storage) {
      case STO_LOCAL:
        fprintf(stream, "%-10s ", "scope"); break;
      case STO_GLOBAL:
        fprintf(stream, "%-10s ", "global"); break;
      case STO_EXTERN:
        fprintf(stream, "%-10s ", "extern"); break;
      case STO_EXPORT:
        fprintf(stream, "%-10s ", "export"); break;
      case STO_INSTANCE:
        fprintf(stream, "%-10s ", "instance"); break;
      default: break;
    }
    fprintf(stream, "0x%08X ", (*s)->addr);
    print_type(stream, (*s)->type);
    fprintf(stream, "\n");
  }
  print_symbol_table_entries(stream, scope->parent);
}

static inline void print_symbol_table(FILE* stream, scope_t* scope) {
  if(!scope) return;
  fprintf(stream, "%s symbol Table:\n", scope->name);
  fprintf(stream, "%-30s %-30s %-10s %-10s %s\n", "name", "scope", "storage", "addr", "type");
  print_symbol_table_entries(stream, scope);
}

static inline int type_equals(type_t a, type_t b) {
  while (a.kind == TYPE_ALIAS) a = *a.as.alias.target;
  while (b.kind == TYPE_ALIAS) b = *b.as.alias.target;

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
    case TYPE_MODULE:
      return strcmp(a.name, b.name) == 0;
    case TYPE_FUNC:
      if (!type_equals(*a.as.func.ret, *b.as.func.ret)) return 0;
      if (a.as.func.params.count != b.as.func.params.count) return 0;
      bool same = true;
      for (size_t i=0; i < a.as.func.params.count && same; i++) {
        same = type_equals(*a.as.func.params.items[i], *b.as.func.params.items[i]);
      }
      return same;
    case TYPE_INTERFACE:
      return strcmp(a.name, b.name) == 0;
    case TYPE_TYPE:
      return type_equals(*a.as.type.of, *b.as.type.of);
    case TYPE_ALIAS:
    case TYPES_COUNT:
    default:
      UNREACHABLE("type_equals");
  }
}

static inline void render_type(sb_t* sb, const type_t* t) {
  if(!t) UNREACHABLE("render_type: null type*");
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
      sb_appendf(sb, "%s", t->name);
      break;
    case TYPE_STRUCT:
      sb_appendf(sb, "struct %s", t->name);
      break;
    case TYPE_MODULE:
      sb_appendf(sb, "module %s", t->name);
      break;
    case TYPE_ARRAY:
      render_type(sb, t->as.array.inner);
      sb_appendf(sb, "[]");
      break;
    case TYPE_FUNC:
      sb_appendf(sb, "(");
      da_foreach(type_t*, typ, &t->as.func.params) {
        render_type(sb, *typ);
        sb_appendf(sb, ",");
      }
      sb_appendf(sb, ") -> ");
      render_type(sb, t->as.func.ret);
      break;
    case TYPE_INTERFACE:
      sb_appendf(sb, "interface %s", t->name);
      break;
    case TYPE_TYPE:
      sb_appendf(sb, "type %s", t->name);
      break;
    case TYPES_COUNT:
    default: break;
  }
}

static inline const char* type_to_str(arena_t* a, const type_t* t) {
  sb_t sb = { 0 };

  render_type(&sb, t);
  sb_appendz(&sb, "");
  return arena_sprintf(a, "%.*s", SB_FMT(sb));
}

static inline void print_type(FILE* stream, const type_t* t) {
  sb_t sb = { 0 };
  render_type(&sb, t);
  fprintf(stream, "%.*s", SB_FMT(sb));
}

static inline const char* qn_to_str(arena_t* a, const ast_qn_t* qn) {
  sb_t sb = { 0 };

  sb_appendf(&sb, "%s", qn->name);
  while (qn->next) {
    sb_appendf(&sb, "::%s", qn->next->name);
    qn = qn->next;
  }

  return arena_sprintf(a, "%.*s", SB_FMT(sb));
}

static inline void typechecker_destroy(typechecker_t* t) {
  arena_free(&t->arena);
  da_free(t->custom_types);
}

static inline type_t* make_type(typechecker_t* t, type_kind_t k, const char* name, size_t size) {
  type_t* type = arena_alloc(&t->arena, sizeof(*type));
  type->kind = k;
  type->name = name;
  type->size = size;

  type_t* type_type = arena_alloc(&t->arena, sizeof(*type));
  type_type->kind = TYPE_TYPE;
  type_type->name = name;
  type_type->size = 0;
  type_type->as.type.of = type;

  if (name) {
    make_symbol(&t->arena, t->current_scope, SYMB_TYPE, STO_LOCAL, name, type_type);
    enter_scope_new(t, arena_sprintf(&t->arena, "type_%s", name));
    type->scope = t->current_scope;
    exit_scope(t);
  }
  type_type->scope = type->scope;
  
  return type;
}

static inline type_t* get_or_create_array_of(typechecker_t* t, const type_t* type) {
  // OPTIMIZE
  da_foreach(type_t*, typ, &t->custom_types) {
    if ((*typ)->kind == TYPE_ARRAY && type_equals(*(*typ)->as.array.inner, *type)) return *typ;
  }

  type_t* typ = make_type(t, TYPE_ARRAY, NULL, 1);
  typ->as.array.inner = (type_t*)type;
  da_append(&t->custom_types, typ);
  
  return typ; 
}

static inline type_t* get_or_create_func_type_from_arr(typechecker_t* t, const type_t* ret_type, size_t n_params, type_t** params) {
  // OPTIMIZE
  da_foreach(type_t*, typ, &t->custom_types) {
    if ((*typ)->kind != TYPE_FUNC) continue;
    if ((*typ)->as.func.params.count != n_params) continue;
    if (!type_equals(*(*typ)->as.func.ret, *ret_type)) continue;
    bool same = true;
    for (size_t i = 0; i < n_params && same; i++)
      same = type_equals(*(*typ)->as.func.params.items[i], *params[i]);
    if (same)
      return *typ;
  }

  type_t* type = make_type(t, TYPE_FUNC, NULL, 1); // which is the correct size?
  for(size_t i=0; i < n_params; i++)
    da_append(&type->as.func.params, params[i]);
  type->as.func.ret = (type_t*)ret_type;
  da_append(&t->custom_types, type);

  return type;
}


static inline type_t* get_or_create_func_type_of(typechecker_t* t, const type_t* ret_type, size_t n_params, ...) {
  va_list args; 

  struct {
    type_t** items;
    size_t count;
    size_t capacity;
  } params = { 0 };

  va_start(args, n_params);
  for(size_t i=0; i < n_params; i++) {
    da_append(&params, va_arg(args, type_t*));
  }
  va_end(args);

  return get_or_create_func_type_from_arr(t, ret_type, n_params, params.items);
}

static inline type_t* resolve_type(typechecker_t* t, ast_qn_t* name, int depth) {
  type_t* res = NULL;

  symbol_t* symbol = resolve_symbol(t->current_scope, name, SYMB_TYPE);
  if (!symbol) return NULL;

  if (!symbol->type) return NULL;
  res = symbol->type->as.type.of;
  if (!res) return NULL;

  while(depth-- > 0) {
    res = get_or_create_array_of(t, res);
  }

  return res;
}

static inline bool is_op_cmp(op_kind_t op) {
  return  op == OP_EQ  || 
          op == OP_LEQ ||
          op == OP_GEQ ||
          op == OP_LT  ||
          op == OP_GT;
}

static inline bool is_op_arithmetic(op_kind_t op) {
  return op == OP_PLUS  || 
         op == OP_MINUS ||
         op == OP_MULT  ||
         op == OP_DIV   ||
         op == OP_REM;
}

static inline num_rank_t get_type_rank(const type_t* t) {
  switch(t->kind) {
    case TYPE_BOOL: return RK_BOOL;
    case TYPE_U32:  return RK_U32; 
    case TYPE_I32:  return RK_I32; 
    case TYPE_I64:  return RK_I64; 
    case TYPE_ADDR:
    case TYPE_U64:  return RK_U64; 
    case TYPE_F32:  return RK_F32; 
    case TYPE_F64:  return RK_F64; 
    case TYPE_CHAR:
    case TYPE_STR:
    case TYPE_ARRAY:
    case TYPE_STRUCT:
    case TYPE_MODULE:
    case TYPE_ALIAS:
    case TYPE_FUNC:
    case TYPE_INTERFACE:
    case TYPE_NONE:
    case TYPE_TYPE:
    case TYPES_COUNT:
    default:
      UNREACHABLE("get_type_rank - not a number");
  }
}

static inline type_t const* find_common_type(const type_t* a, const type_t* b) {
  num_rank_t ra = get_type_rank(a);
  num_rank_t rb = get_type_rank(b);

  return ra >= rb ? a : b;
}

static inline bool is_numeric(const type_t* t) {
  if (t->kind == TYPE_ALIAS) return is_numeric(t->as.alias.target);
  return t->kind == TYPE_I32 ||
         t->kind == TYPE_I64 ||
         t->kind == TYPE_U32 ||
         t->kind == TYPE_U64 ||
         t->kind == TYPE_F32 ||
         t->kind == TYPE_F64 ||
         t->kind == TYPE_ADDR;
}

static inline symbol_t* find_binop_impl(const type_t* owner, op_kind_t op, const type_t* other) {
  const char* interface_name = NULL;
  const char* method_name = NULL;
  switch (op) {
    case OP_PLUS:
      interface_name = "Add";
      method_name = "add";
      break;
    case OP_MINUS:
      interface_name = "Sub";
      method_name = "sub";
      break;
    case OP_MULT:
      interface_name = "Mul";
      method_name = "mul";
      break;
    case OP_DIV:
      interface_name = "Div";
      method_name = "div";
      break;
    case OP_REM:
      interface_name = "Rem";
      method_name = "rem";
      break;
    case OP_EQ:
      interface_name = "Eq";
      method_name = "eq";
      break;
    case OP_LEQ:
      interface_name = "Cmp";
      method_name = "leq";
      break;
    case OP_GEQ:
      interface_name = "Cmp";
      method_name = "geq";
      break;
    case OP_LT:
      interface_name = "Cmp";
      method_name = "lt";
      break;
    case OP_GT:
      interface_name = "Cmp";
      method_name = "gt";
      break;
    case OP_CALL:
      interface_name = "Callable";
      method_name = "call";
      break;
    case OP_ASSIGN:
      interface_name = "Copiable";
      method_name = "copy";
      break;
    case OP_MEMB:
      interface_name = "Accessible";
      method_name = "access";
      break;
    case OP_INVALID:
    default:
      UNREACHABLE("find_binop_impl");
  }

  if (!interface_name || !method_name) return NULL;

  // TODO: unhardcode
  ast_qn_t iface = { .name = interface_name };
  ast_qn_t ops   = { .name = "ops", .next = &iface };
  ast_qn_t core  = { .name = "core", .next = &ops };
  symbol_t* iface_symb = resolve_symbol(owner->scope, &core, SYMB_TYPE);

  bool implements = false;
  da_foreach(ast_qn_t*, impl, &owner->impls) {
    if (iface_symb == resolve_symbol(owner->scope, *impl, SYMB_TYPE)) {
      implements = true;
      break;
    }
  }

  if (!implements) return NULL;

  symbol_t* method = NULL;
  da_foreach(symbol_t*, s, &owner->scope->symbols) {
    if(
        (*s)->kind == SYMB_FUNC &&
        strcmp((*s)->name, method_name) == 0 && 
        (*s)->type->as.func.params.count == 2 &&
        type_equals(*(*s)->type->as.func.params.items[0], *owner) &&
        type_equals(*(*s)->type->as.func.params.items[1], *other)
    )
      method = *s;
  }

  return method;
}

static inline bool resolve_type_binop(typechecker_t* t, ast_expr_t* e) {
  type_t const* left  = e->as.binop.lhs->type;
  type_t const* right = e->as.binop.rhs->type;
  op_kind_t op = e->as.binop.op;

  if (is_op_cmp(op)) {
    if (type_equals(*left, *right) && is_numeric(left)) {
      e->type = t->builtins.boolean; 
      return true;
    }
  } else if (is_op_arithmetic(op)) {
    if (is_numeric(left) && is_numeric(right)) {
      e->type = find_common_type(left, right);
      return true;
    }
  }

  symbol_t* impl = find_binop_impl(left, op, right);
  if (!impl) {
    fdiagf(
      stderr,
      DIAG_ERROR,
      e->loc,
      "`%s` does not implement interface method for %s operator and `%s` operand.",
      type_to_str(&t->arena, left),
      tok_kind_str(&t->arena, (tok_kind_t)op),
      type_to_str(&t->arena, right)
    );
    return false;
  }

  type_t* ret = impl->type->as.func.ret;
  if (strcmp(ret->name, "Self") == 0) ret = ret->as.alias.target;

  ast_expr_t* lhs = e->as.binop.lhs;
  ast_expr_t* rhs = e->as.binop.rhs;

  e->kind = EXPR_FUNCALL;
  e->as.funcall.args.items = NULL;
  e->as.funcall.args.count = 0;
  e->as.funcall.args.capacity = 0;
  da_append(&e->as.funcall.args, rhs);
  // crafting a new ast node on separate arena. bad idea?
  e->as.funcall.callee = arena_alloc(&t->arena, sizeof(ast_expr_t));
  e->as.funcall.callee->ast_kind = AST_EXPR;
  e->as.funcall.callee->kind = EXPR_ACCESS;
  e->as.funcall.callee->as.access.field = impl->name;
  e->as.funcall.callee->as.access.owner = lhs;
  e->as.funcall.callee->as.access.op = OP_MEMB;

  e->type = ret;

  return true;
}

static inline bool typecheck_expr(typechecker_t* t, ast_expr_t* expr) {
  expr->is_const = false;
  switch(expr->kind) {
    case EXPR_SYMBOL:
      symbol_t* s = resolve_symbol_any(t->current_scope, expr->as.symbol); 
      if (!s || !s->type) {
        fdiagf(stderr, DIAG_ERROR, expr->loc, "Undeclared symbol `%s`.", qn_to_str(&t->arena, expr->as.symbol));
        return false;
      }

      if (s->storage == STO_INSTANCE) {
        ast_qn_t* tmp = expr->as.symbol; 
        if (!tmp->next) UNREACHABLE("typecheck_expr");

        sb_t sb = { 0 };
        sb_appendf(&sb, "%s", tmp->name);
        while (tmp->next->next) {
          sb_appendf(&sb, "::%s", tmp->name);
          tmp = tmp->next;
        }

        fdiagf(
          stderr,
          DIAG_ERROR,
          expr->loc, 
          "Cannot access instance field `%s` statically through type `%.*s`. Try to instance an object (`obj: %.*s;`) and access the field from it (`obj.%s`).", 
          s->name,
          SB_FMT(sb),
          SB_FMT(sb),
          s->name
        );
        return false;
      }

      expr->type = s->type;
      return true;
    case EXPR_STRING:
      expr->type = t->builtins.str;
      expr->is_const = true;
      return true;
    case EXPR_NUMBER:
      if (expr->as.number.ti & TI_REAL) {
        if (expr->as.number.ti & TI_LONG)
          expr->type = t->builtins.f64;
        else
          expr->type = t->builtins.f32;
      } else if (expr->as.number.ti & TI_UNSIGNED) {
        if (expr->as.number.ti & TI_LONG)
          expr->type = t->builtins.u64;
        else
          expr->type = t->builtins.u32;
      } else {
        if (expr->as.number.ti & TI_LONG)
          expr->type = t->builtins.i64;
        else
          expr->type = t->builtins.i32;
      }
      expr->is_const = true;
      return true;
    case EXPR_BINOP:
      if(!typecheck_expr(t, expr->as.binop.lhs)) return false;
      if(!typecheck_expr(t, expr->as.binop.rhs)) return false;

      // TODO: constant folding to mark as constant
      return resolve_type_binop(t, expr);
    case EXPR_UNOP:
      TODO("typecheck_expr: EXPR_UNOP");
      break;
    case EXPR_ACCESS: {
      if(!typecheck_expr(t, expr->as.access.owner)) return false;

      symbol_t* s = resolve_field(expr->as.access.owner->type->scope, expr->as.access.field);
      if (!s) {
        fdiagf(
          stderr,
          DIAG_ERROR,
          expr->loc,
          "`%s` has no field named `%s`.",
          type_to_str(&t->arena, expr->as.access.owner->type),
          expr->as.access.field
        );
        return false;
      }

      if (expr->as.access.owner->type->kind == TYPE_TYPE) {
        fdiagf(
          stderr,
          DIAG_ERROR,
          expr->as.access.owner->loc,
          "Cannot access field `%s` of `%s`. "
          "Maybe you wanted to access its scope? (try using `::` operator instead of `.`).",
          expr->as.access.field,
          type_to_str(&t->arena, expr->as.access.owner->type)
        );
        return false;
      } 

      expr->type = s->type;
      return true;
    }
    case EXPR_FUNCALL:
      ast_expr_t* callee = expr->as.funcall.callee;
      if(!typecheck_expr(t, callee)) return false;

      if (callee->type->kind != TYPE_FUNC) {
        fdiagf(stderr, DIAG_ERROR, callee->loc, "Cannot call non-callable type `%s`", type_to_str(&t->arena, callee->type));
        return false;
      }

      if (expr->as.funcall.args.count + (callee->kind == EXPR_ACCESS && callee->as.access.op == OP_MEMB) != callee->type->as.func.params.count) {
          fdiagf(
            stderr, 
            DIAG_ERROR,
            expr->loc,
            "Mismatched number of arguments in function call: expected %zu, got %zu.",
            callee->type->as.func.params.count,
            expr->as.funcall.args.count + (callee->kind == EXPR_ACCESS && callee->as.access.op == OP_MEMB)
          );
          return false;
      }

      // NOTE: should never happen
      if (callee->kind == EXPR_ACCESS && callee->as.access.op == OP_MEMB) {
        if(!type_equals(*callee->as.access.owner->type, **callee->type->as.func.params.items)) {
          fdiagf(
            stderr,
            DIAG_ERROR,
            callee->loc,
            "Mismatched type for method: expected `%s`, got `%s`",
            type_to_str(&t->arena, *callee->type->as.func.params.items),
            type_to_str(&t->arena, callee->as.access.owner->type)
          );
          return false;
        }
      }

      for(size_t i = 0; i < expr->as.funcall.args.count; i++) {
        ast_expr_t* arg = da_at(expr->as.funcall.args, i);
        if(!typecheck_expr(t, arg)) return false;
        type_t const* a = arg->type;
        type_t const* b = da_at(callee->type->as.func.params, i + (callee->kind == EXPR_ACCESS && callee->as.access.op == OP_MEMB));
        if (!type_equals(*a, *b)) {
          // TODO: find way to retrieve argument name for better diagnostics
          fdiagf(
            stderr,
            DIAG_ERROR,
            arg->loc,
            "Mismatched type for argument %zu in function call: expected `%s`, got `%s`.", 
            i,
            type_to_str(&t->arena, b),
            type_to_str(&t->arena, a)
          );
          return false;
        }
      }

      expr->type = callee->type->as.func.ret;
      return true;
    case EXPR_SUBEXPR:
      if (!typecheck_expr(t, expr->as.subexpr)) return false;
      expr->type = expr->as.subexpr->type;
      expr->is_const = expr->as.subexpr->is_const;
      return true; 
    case EXPR_ASSIGNMENT:
      if(!typecheck_expr(t, expr->as.binop.lhs)) return false;
      if(!typecheck_expr(t, expr->as.binop.rhs)) return false;

      if(expr->as.binop.lhs->type != expr->as.binop.rhs->type) {
        fdiagf(
          stderr,
          DIAG_ERROR,
          expr->as.binop.rhs->loc,
          "Incompatible types when assigning expression of type `%s` to variable of type `%s`.",
          type_to_str(&t->arena, expr->as.binop.rhs->type),
          type_to_str(&t->arena, expr->as.binop.lhs->type )
        );
        return false;
      }

      if(
        expr->as.binop.lhs->kind != EXPR_ACCESS &&
        expr->as.binop.lhs->kind != EXPR_SYMBOL
      ) {
        fdiagf(stderr, DIAG_ERROR, expr->as.binop.lhs->loc, "Assignement only possible for lvalues.");
        return false;
      }

      // TODO: convert assignment of struct a = b to a.copy(b)
      // ensure Copyable interface for struct

      expr->type = t->builtins.none;
      return true;
    default:
      UNREACHABLE("typecheck_expr");
  }
  return true;
}

static inline bool resolve_type_decl(typechecker_t* t, ast_type_t* type) {
  type_t* res = resolve_type(t, type->name, type->array_depth);
  if(!res) {
    fdiagf(stderr, DIAG_ERROR, type->loc, " Undefined type `%s`.", qn_to_str(&t->arena, type->name));
    return false;
  }
  type->resolved_type = res;

  return true;
}

static inline bool typecheck_stmt(typechecker_t* t, ast_stmt_t* stmt, type_t* ret_type) {
  switch(stmt->kind) {
    case STMT_EMPTY: break;
    case STMT_RET:
      if(!typecheck_expr(t, stmt->as.retval)) return false;
      if(!type_equals(*stmt->as.retval->type, *ret_type)) {
        fdiagf(
          stderr,
          DIAG_ERROR,
          stmt->loc,
          "Incompatible return type: expected `%s`, got `%s`.",
          type_to_str(&t->arena, ret_type),
          type_to_str(&t->arena, stmt->as.retval->type)
        );
        return false; 
      }
      break;
    case STMT_EXPR:
      if(!typecheck_expr(t, stmt->as.expression)) return false;
      break;
    case STMT_VAR_DEF:
      if(!resolve_type_decl(t, stmt->as.var_def->type)) return false;
      stmt->as.var_def->symbol = make_symbol(&t->arena, t->current_scope, SYMB_VAR, STO_LOCAL, stmt->as.var_def->name, stmt->as.var_def->type->resolved_type);

      if( stmt->as.var_def->flags & SPEC_EXTERN ) {
        fdiagf(stderr, DIAG_ERROR, stmt->loc, "Cannot define an extern local variable.");
        return false;
      }
      if( stmt->as.var_def->flags & SPEC_EXPORT ) {
        fdiagf(stderr, DIAG_ERROR, stmt->loc, "Cannot export a local variable.n");
        return false;
      }

      // TODO: type inference
      da_foreach(symbol_t*, symb, &t->current_scope->symbols) {
        if (
            *symb != stmt->as.var_def->symbol && 
            (*symb)->kind == SYMB_VAR &&
            strcmp(stmt->as.var_def->name, (*symb)->name) == 0
        ) {
          fdiagf(stderr, DIAG_ERROR, stmt->loc, "Multiple definitions for variable `%s` in this scope", stmt->as.var_def->name);
          return false;
        }
        // NOTE: allow shadowing -> do not check in parent scope
        // TODO: maybe issue shadowing warning?
      }

      if(stmt->as.var_def->init) {
        if(!typecheck_expr(t, stmt->as.var_def->init)) return false;
        if(stmt->as.var_def->type->resolved_type != stmt->as.var_def->init->type) {
          fdiagf(
            stderr,
            DIAG_ERROR,
            stmt->loc,
            "Incompatible types when initializing type `%s` using type `%s`.",
            type_to_str(&t->arena, stmt->as.var_def->type->resolved_type),
            type_to_str(&t->arena, stmt->as.var_def->init->type)
          );
          return false;
        }
      }
      break;
    case STMT_IF:
      struct _if_else* if_else = &stmt->as.if_else;
      if(!typecheck_expr(t, if_else->cond)) return false;
      if(!type_equals(*if_else->cond->type, *t->builtins.boolean)) {
        fdiagf(
          stderr,
          DIAG_ERROR,
          if_else->cond->loc,
          "Incompatible type in condition: expected `%s`, got `%s`.",
          type_to_str(&t->arena, t->builtins.boolean),
          type_to_str(&t->arena, if_else->cond->type)
        );
        return false;
      }

      enter_scope_new(t, arena_sprintf(&t->arena, "%s_if", t->current_scope->name));
      if_else->if_scope = t->current_scope;
      da_foreach(ast_stmt_t*, stmt, &if_else->if_body->stmts) {
        if(!typecheck_stmt(t, *stmt, ret_type)) return false;
      }
      exit_scope(t);

      if(if_else->else_body) {
        enter_scope_new(t, arena_sprintf(&t->arena, "%s_else", t->current_scope->name));
        if_else->else_scope = t->current_scope;
        da_foreach(ast_stmt_t*, stmt, &if_else->else_body->stmts) {
          if(!typecheck_stmt(t, *stmt, ret_type)) return false;
        }
        exit_scope(t);
      }
      break;
    case STMT_WHILE:
      struct _while* wl = &stmt->as.while_loop;
      if(!typecheck_expr(t, wl->cond)) return false;
      if(!type_equals(*wl->cond->type, *t->builtins.boolean)) {
        fdiagf(
          stderr,
          DIAG_ERROR,
          wl->cond->loc,
          "Incompatible type in condition: expected `%s`, got `%s`.",
          type_to_str(&t->arena, t->builtins.boolean),
          type_to_str(&t->arena, wl->cond->type)
        );
        return false;
      }

      enter_scope_new(t, arena_sprintf(&t->arena, "%s_while", t->current_scope->name));
      wl->scope = t->current_scope;
      da_foreach(ast_stmt_t*, stmt, &wl->body->stmts) {
        if(!typecheck_stmt(t, *stmt, ret_type)) return false;
      }
      exit_scope(t);

      break;

    default:
      UNREACHABLE("typecheck_stmt");
  }

  return true;
}

static inline bool resolve_func_def(typechecker_t* t, ast_func_def_t* def) {
  struct {
    type_t** items;
    size_t count;
    size_t capacity;
  } params = { 0 };

  enter_scope_new(t, arena_sprintf(&t->arena, "fun_%s", def->name));

  if(!resolve_type_decl(t, def->sig->ret)) return false;
  da_foreach(ast_param_t*, p, &def->sig->params) {
    if(!resolve_type_decl(t, (*p)->type)) return false;
    symbol_t* param_sym = make_symbol(&t->arena, t->current_scope, SYMB_VAR, STO_LOCAL, (*p)->name, (*p)->type->resolved_type);
    param_sym->addr_resolved = true;
    param_sym->addr = params.count;

    da_append(&params, (*p)->type->resolved_type);
  }

  type_t* sig_type = get_or_create_func_type_from_arr(
    t, 
    def->sig->ret->resolved_type, 
    params.count,
    params.items
  );

  def->sig->resolved_type = sig_type;

  symb_storage_t sto = STO_LOCAL;
  if(def->flags & SPEC_EXTERN) sto = STO_EXTERN; 
  if(def->flags & SPEC_EXPORT) sto = STO_EXPORT; 

  def->scope = t->current_scope;
  exit_scope(t);

  def->symbol = make_symbol(&t->arena, t->current_scope, SYMB_FUNC, sto, def->name, def->sig->resolved_type);

  return true;
}

static inline bool typecheck_func(typechecker_t* t, ast_func_def_t* func) {
  if(!func->body && !(func->flags & SPEC_EXTERN)) {
    fdiagf(stderr, DIAG_ERROR, func->loc, "Cannot define a non-extern function without a body.");
    return false;
  }
  if(func->body && (func->flags & SPEC_EXTERN)) {
    fdiagf(stderr, DIAG_ERROR, func->loc, "Cannot define an extern function with a body.");
    return false;
  }

  da_foreach(symbol_t*, symb, &t->current_scope->symbols) {
    if (
        *symb != func->symbol && 
        (*symb)->kind == SYMB_FUNC &&
        strcmp(func->name, (*symb)->name) == 0 &&
        type_equals(*(*symb)->type, *func->sig->resolved_type)
    ) {
      fdiagf(
        stderr,
        DIAG_ERROR,
        func->loc,
        "Multiple definitions for function `%s` with type `%s` in this scope.",
        func->name,
        type_to_str(&t->arena, func->sig->resolved_type)
      );
      return false;
    }
  }
  // NOTE: allow shadowing -> do not check in parent scope
  // TODO: maybe issue shadowing warning?

  if (func->body) {
    enter_scope(t, func->scope);
    da_foreach(ast_stmt_t*, stmt, &func->body->stmts) {
      if(!typecheck_stmt(t, *stmt, func->sig->ret->resolved_type)) return false;
    }
    exit_scope(t);
  }

  return true;
}

static inline void typechecker_init(typechecker_t* t) {
  enter_scope_new(t, "__builtin_types");
  t->builtins.scope = t->current_scope;
  
  t->builtins.none      = make_type(t, TYPE_NONE, "none", 0);
  t->builtins.i32       = make_type(t, TYPE_I32,  "i32",  1);
  t->builtins.i64       = make_type(t, TYPE_I64,  "i64",  2);
  t->builtins.u32       = make_type(t, TYPE_U32,  "u32",  1);
  t->builtins.u64       = make_type(t, TYPE_U64,  "u64",  2);
  t->builtins.f32       = make_type(t, TYPE_F32,  "f32",  1);
  t->builtins.f64       = make_type(t, TYPE_F64,  "f64",  2);
  t->builtins.character = make_type(t, TYPE_CHAR, "char", 1);
  t->builtins.boolean   = make_type(t, TYPE_BOOL, "bool", 1);
  t->builtins.str       = make_type(t, TYPE_STR,  "str",  1);
  t->builtins.addr      = make_type(t, TYPE_ADDR, "addr", 1);

  enter_scope_new(t, "root");
}

// TODO: Make a function to check type->kind, otherwise aliases will fail checks
static inline bool typecheck(typechecker_t* t, ast_root_t* root) {
  root->scope = t->current_scope;

  // resolve types and symbols first

  da_foreach(ast_mod_t*, mod, &root->submods) {
    type_t* type = make_type(t, TYPE_MODULE, (*mod)->name, 0);

    enter_scope(t, type->scope);
    t->current_scope->parent = t->builtins.scope; 
    typecheck(t, (*mod)->root);
    t->current_scope = root->scope;
  }

  da_foreach(ast_struct_t*, s, &root->structs) {
    type_t* type = make_type(t, TYPE_STRUCT, (*s)->name, (*s)->fields.count);

    enter_scope(t, type->scope);
    da_foreach(ast_param_t*, f, &(*s)->fields) {
      if(!resolve_type_decl(t, (*f)->type)) return false;

      symbol_t* sym = make_symbol(&t->arena, t->current_scope, SYMB_VAR, STO_INSTANCE, (*f)->name, (*f)->type->resolved_type);
      if(!sym) {
        fdiagf(stderr, DIAG_FATAL, (*f)->loc, "Failed to create type");
        abort();
      }
      set_symbol_address(sym, da_indexof(&(*s)->fields, f));
      (*f)->symbol = sym;

      da_append(&type->as.structure.fields, (*f)->type->resolved_type);
    }
    exit_scope(t);
  }

  da_foreach(ast_func_def_t*, d, &root->func_defs)
    if(!resolve_func_def(t, *d)) return false;

  da_foreach(ast_iface_t*, iface, &root->interfaces) {
    type_t* self = make_type(t, TYPE_ALIAS, "Self", 0);
    self->as.alias.target = (type_t*)t->builtins.none;

    type_t* type = make_type(t, TYPE_INTERFACE, (*iface)->name, 0);

    enter_scope(t, type->scope);
    da_foreach(ast_func_def_t*, def, &(*iface)->methods) {
      if(!resolve_func_def(t, *def)) return false;

      if((*def)->body) {
        fdiagf(stderr, DIAG_ERROR, (*def)->loc, "Cannot define method `%s` with body in `%s` interface.", (*def)->name, (*iface)->name);
        return false;
      }

      if((*def)->flags) {
        fdiagf(stderr, DIAG_WARN, (*def)->loc, "Specifiers for method `%s` in `%s` interface will be ignored.", (*def)->name, (*iface)->name);
      }

      da_append(&type->as.interface.methods, ((interface_method_t){ .name = (*def)->name, .type = (*def)->sig->resolved_type }));
    }
    exit_scope(t);
  }

  da_foreach(ast_impl_t*, impl, &root->impls) {
    bool impl_self = false;
    {
      if ((*impl)->interface->name && !(*impl)->interface->name->next)
        impl_self = strcmp((*impl)->interface->name->name, "Self") == 0;
    }
    if(!impl_self && !resolve_type_decl(t, (*impl)->interface)) return false;
    if(!resolve_type_decl(t, (*impl)->type)) return false;

    type_t* type = (*impl)->type->resolved_type;

    scope_t* saved_scope = t->current_scope;
    enter_scope(t, type->scope);
    
    type_t* self = make_type(t, TYPE_ALIAS, "Self", 0);
    self->as.alias.target = type;
    self->scope = type->scope;

    da_foreach(ast_func_def_t*, method, &(*impl)->methods) {
      if(!impl_self) {
        bool found = false;
        da_foreach(interface_method_t, m, &(*impl)->interface->resolved_type->as.interface.methods) {
          if(strcmp(m->name, (*method)->name) == 0) { found = true; break;}
        }
        if(!found) {
          fdiagf(stderr, DIAG_ERROR, (*method)->loc, "Undeclared method `%s` in `%s` interface.", (*method)->name, (*impl)->interface->name);
          return false;
        }
      }
      if(!resolve_func_def(t, *method)) return false;

      if(
          (*method)->symbol->type->as.func.params.count < 1 || 
          !type_equals(*(*method)->symbol->type->as.func.params.items[0], *self)
      ) {
        fdiagf(stderr, DIAG_ERROR, (*method)->loc, "Method `%s` requires `Self` first argument.", (*method)->name);
        return false;
      }
    }

    t->current_scope = saved_scope;

    if (!impl_self) {
      da_foreach(interface_method_t, m, &(*impl)->interface->resolved_type->as.interface.methods) {
        // TODO: compare types, not just name

        // TODO: fix horrifying hack
        ast_qn_t tmp = (ast_qn_t){ .name = m->name };
        symbol_t* s = resolve_symbol_local(type->scope, &tmp, SYMB_FUNC);
        if(!s) {
          fdiagf(stderr, DIAG_ERROR, (*impl)->loc, "Missing required method `%s` in `%s` interface implementation.", m->name, (*impl)->interface->name);
          return false;
        }
      }
      da_append(&type->impls, (*impl)->interface->name);
    }
  }

  // then check bodies

  da_foreach(ast_impl_t*, impl, &root->impls) {
    type_t* type = (*impl)->type->resolved_type;
    scope_t* saved_scope = t->current_scope;
    enter_scope(t, type->scope);
    da_foreach(ast_func_def_t*, method, &(*impl)->methods)
      if(!typecheck_func(t, *method)) return false;
    t->current_scope = saved_scope;
  }

  da_foreach(ast_func_def_t*, d, &root->func_defs) {
    if(!typecheck_func(t, *d)) return false;
  }

  da_foreach(ast_var_def_t*, d, &root->var_defs) {
    if(!resolve_type_decl(t, (*d)->type)) return false;
    (*d)->symbol = make_symbol(&t->arena, t->current_scope, SYMB_VAR, STO_GLOBAL, (*d)->name, (*d)->type->resolved_type);

    if((*d)->init && ((*d)->flags & SPEC_EXTERN)) {
      fdiagf(stderr, DIAG_ERROR, (*d)->loc, "Cannot initialize an extern variable.");
      return false;
    }

    da_foreach(symbol_t*, symb, &t->current_scope->symbols) {
      if (
          *symb != (*d)->symbol && 
          (*symb)->kind == SYMB_VAR &&
          strcmp((*d)->name, (*symb)->name) == 0
      ) {
        fdiagf(stderr, DIAG_ERROR, (*d)->loc, "Multiple definitions for variable `%s` in this scope.", (*d)->name);
        return false;
      }
      // NOTE: allow shadowing -> do not check in parent scope
      // TODO: maybe issue shadowing warning?
    }

    if((*d)->init) {
      if(!typecheck_expr(t, (*d)->init)) return false;
      if(!(*d)->init->is_const) {
        fdiagf(stderr, DIAG_ERROR, (*d)->init->loc, "Initializer is not a compile-time constant.");
        return false;
      }

      if(!type_equals(*(*d)->type->resolved_type, *(*d)->init->type)) {
        fdiagf(
          stderr,
          DIAG_ERROR,
          (*d)->init->loc,
          "Incompatible types when initializing type `%s` using type `%s`.",
          type_to_str(&t->arena, (*d)->type->resolved_type),
          type_to_str(&t->arena, (*d)->init->type)
        );
        return false;
      }
    }
  }
 
  return true;
}

static inline void codegen_load_var(program_t* p, symbol_t* symbol) {
  switch (symbol->storage) {
    case STO_EXTERN:
      TODO("codegen_load_var - load extern symbol");
    case STO_LOCAL:
      da_append(&p->code, INST_LOAD);
      da_append(&p->code, symbol->addr);
      break;
    case STO_EXPORT:
    case STO_GLOBAL:
      da_append(&p->code, INST_LOADG);
      da_append(&p->code, symbol->addr);
      break;
    case STO_INSTANCE:
    default: 
      UNREACHABLE("codegen_load_var");
  }
  
  if (!symbol->addr_resolved)
    da_append(&p->patches, ((struct _patch){ .symbol = symbol, .addr = p->code.count - 1}));
}

static inline bool codegen_expr(program_t* p, scope_t* scope, ast_expr_t* e) {
  switch(e->kind) {
      case EXPR_SYMBOL: {
        // TODO: optimize -> resolve symbol only once in typechecker, store symbol_t in expr
        symbol_t* symbol = resolve_symbol(scope, e->as.symbol, SYMB_VAR);
        if (!symbol) {
          fdiagf(stderr, DIAG_ERROR, e->loc, "No variable `%s` in current scope.", e->as.symbol);
          return false;
        }
        codegen_load_var(p, symbol);
        break;
      }
      case EXPR_STRING:
        da_append(&p->code, INST_LOADC);
        da_append(&p->code, p->constants.count);
        // TODO: alloc on arena (make a 'program' arena that holds data that needs to be transfered
        // from compiler to vm. This arena will also hold any kind of ffi data)
        da_append(&p->constants, ((constant_t){ .kind = DK_STR, .as.s = strdup(e->as.s) }));
        break;
      case EXPR_NUMBER:
        if(e->as.number.ti & TI_LONG) {
          TODO("codegen_expr: handle long/wide data");
          // da_append(&p->code, INST_PUSHL);
          // da_append(&p->code, e->as.number.u >> 32);
          // da_append(&p->code, e->as.number.u & (uint32_t)-1);
        } else {
          da_append(&p->code, INST_PUSH);
          da_append(&p->code, e->as.number.u & (uint32_t)-1);
        }
        break;
      case EXPR_BINOP:
        // TODO: handle operator length, signdess and type (when implementing type checker)
        codegen_expr(p, scope, e->as.binop.lhs);
        codegen_expr(p, scope, e->as.binop.rhs);
        //TODO: implicit cast if either type is different from the result

        switch(e->as.binop.op){
          case OP_PLUS: da_append(&p->code, INST_ADD); break;
          case OP_MINUS: da_append(&p->code, INST_SUB); break;
          case OP_MULT: da_append(&p->code, INST_MULT); break;
          case OP_DIV: da_append(&p->code, INST_DIVI); break;
          case OP_REM: da_append(&p->code, INST_REM); break;
          case OP_EQ: da_append(&p->code, INST_EQ); break;
          case OP_LEQ: da_append(&p->code, INST_LEQ); break;
          case OP_GEQ: da_append(&p->code, INST_GEQ); break;
          case OP_LT: da_append(&p->code, INST_LT); break;
          case OP_GT: da_append(&p->code, INST_GT); break;
          case OP_MEMB:
          case OP_ASSIGN:
          case OP_CALL:
          case OP_INVALID:
          default:
            UNREACHABLE("codegen expr: Invalid binary operation: %d", e->as.binop.op);
        }
        break;
      case EXPR_UNOP:
        TODO("codegen_expr (UNOP)");
        break;
      case EXPR_ACCESS: {
        arena_t a = { 0 };
        // TODO: optimize -> resolve symbol only once in typechecker, store symbol_t in expr
        symbol_t* symbol = resolve_field(e->as.access.owner->type->scope, e->as.access.field);
        if (!symbol) {
          fdiagf(stderr, DIAG_ERROR, e->loc, "No symbol `%s` in `%s`.", e->as.symbol, type_to_str(&a, e->as.access.owner->type));
          arena_free(&a);
          return false;
        }
        if(!codegen_expr(p, scope, e->as.access.owner)) return false;
        da_append(&p->code, INST_LOADF);
        da_append(&p->code, symbol->addr);
        break;
      }
      case EXPR_FUNCALL:
        // SysV-style calling convention
        for(int i = e->as.funcall.args.count - 1; i >= 0; i--) {
          if(!codegen_expr(p, scope, e->as.funcall.args.items[i])) return false;
        }
        ast_expr_t* callee = e->as.funcall.callee;

        symbol_t* symbol = NULL;
        if(callee->kind == EXPR_SYMBOL) {
          // TODO: resolve symbol once in typechecker
          symbol = resolve_symbol(scope, callee->as.symbol, SYMB_FUNC);
          if (!symbol) {
            fdiagf(stderr, DIAG_ERROR, callee->loc, "No function `%s` in current scope.", callee->as.symbol);
            return false;
          }
        } else if (callee->kind == EXPR_ACCESS) {
          ast_expr_t* owner = callee->as.access.owner;

          if (callee->as.access.op == OP_MEMB)
            if(!codegen_expr(p, scope, owner)) return false;

          symbol = resolve_field(owner->type->scope, callee->as.access.field);
          if (!symbol) {
            // TODO: remove this abomination
            arena_t temp = { 0 };
            fdiagf(
              stderr,
              DIAG_ERROR,
              callee->loc,
              "No method `%s` in type `%s`.",
              callee->as.access.field,
              type_to_str(&temp, owner->type)
            );
            return false;
          }
        } else {
          TODO("codegen_expr: EXPR_FUNCALL - callee not a symbol");
        }

        da_append(&p->code, symbol->storage == STO_EXTERN ? INST_HOSTCALL : INST_CALL);
        if (symbol->addr_resolved)
          da_append(&p->code, symbol->addr);
        else {
          da_append(&p->patches, ((struct _patch){ .symbol = symbol, .addr = p->code.count }));
          da_append(&p->code, 0);
        }
        break;
      case EXPR_SUBEXPR:
        return codegen_expr(p, scope, e->as.subexpr);
      case EXPR_ASSIGNMENT: {
        if(!codegen_expr(p, scope, e->as.assign.rhs)) return false;

        ast_expr_t* lhs = e->as.assign.lhs;

        if(lhs->kind == EXPR_SYMBOL) {
          // TODO: optimize -> resolve symbol only once in typechecker, store symbol_t in expr
          symbol_t* symbol = resolve_symbol(scope, lhs->as.symbol, SYMB_VAR);
          if (!symbol) {
            fdiagf(stderr, DIAG_ERROR, e->loc, "No symbol `%s` in current scope.", e->as.symbol);
            return false;
          }
          switch (symbol->storage) {
            case STO_EXTERN:
              TODO("codegen_expr - store extern symbol");
            case STO_LOCAL:
              da_append(&p->code, INST_STORE);
              da_append(&p->code, symbol->addr);
              break;
            case STO_EXPORT:
            case STO_GLOBAL:
              da_append(&p->code, INST_STOREG);
              da_append(&p->code, symbol->addr);
              break;
            case STO_INSTANCE:
            default: 
              UNREACHABLE("codegen_expr");
          }
        } else if (lhs->kind == EXPR_ACCESS && lhs->as.access.op == OP_MEMB) {
          symbol_t* symbol = resolve_field(lhs->as.access.owner->type->scope, lhs->as.access.field);
          if (!symbol) {
            arena_t a;
            fdiagf(stderr, DIAG_ERROR, e->loc, "No symbol `%s` in `%s`.", lhs->as.access.field, type_to_str(&a, lhs->as.access.owner->type));
            arena_free(&a);
            return false;
          }
          if(!codegen_expr(p, scope, e->as.assign.lhs->as.access.owner)) return false;
          da_append(&p->code, INST_STOREF);
          da_append(&p->code, symbol->addr);
        } else {
          TODO("codegen_expr - assignment: unsupported lhs");
        }
        break;
      }
      default:
        UNREACHABLE("codegen_expr");
  }

  return true;
}

static inline void codegen_struct_instantiate(program_t* p, type_t* s) {
  da_append(&p->code, INST_MKOBJ);
  da_append(&p->code, s->size);

  for (size_t i = 0; i < s->as.structure.fields.count; i++) {
    if (s->as.structure.fields.items[i]->kind == TYPE_STRUCT) {
      da_append(&p->code, INST_DUP);

      codegen_struct_instantiate(p, s->as.structure.fields.items[i]);

      da_append(&p->code, INST_SWAP);

      da_append(&p->code, INST_STOREF);
      da_append(&p->code, i);
    }
  }
}

static inline bool codegen_stmt(program_t* p, scope_t* scope, frame_t* f, ast_stmt_t* s) {
  switch(s->kind) {
    case STMT_EMPTY: return true;
    case STMT_EXPR: return codegen_expr(p, scope, s->as.expression);
    case STMT_RET: {
      codegen_expr(p, scope, s->as.retval);
      // TODO: function epilog -> destroy frame, clean stack
      da_append(&p->code, INST_RET);
      return true;
    }
    case STMT_VAR_DEF: {
      if (f->loc_vars >= MAX_LOC_VARS) {
        fdiagf(stderr, DIAG_FATAL, s->loc, "Too many local variables in function %s", f->name);
        abort();
      }

      symbol_t* symb = s->as.var_def->symbol;
      set_symbol_address(symb, f->loc_vars++);
      
      // TODO: allocate all local variables at once at function call, function should
      // be aware of its local vars after typecheck pass
      if (s->as.var_def->type->resolved_type->kind == TYPE_STRUCT) {
        codegen_struct_instantiate(p, s->as.var_def->type->resolved_type);

        da_append(&p->code, INST_STORE);
        da_append(&p->code, symb->addr);
      }

      if (s->as.var_def->init) {
        if (s->as.var_def->type->resolved_type->kind == TYPE_STRUCT) {
          TODO("struct init/construct - new() method?");
        } else {
          if(!codegen_expr(p, scope, s->as.var_def->init)) return false;

          da_append(&p->code, INST_STORE);
          da_append(&p->code, symb->addr);
        }
      }
      return true;
    }
    case STMT_IF: {
      size_t j_to_end_offset = 0; 
      size_t j_to_else_offset = 0;

      if(!codegen_expr(p, scope, s->as.if_else.cond)) return false;
      da_append(&p->code, INST_JZ);
      da_append(&p->code, 0);
      if (s->as.if_else.else_body)
        j_to_else_offset = p->code.count - 1;
      else
        j_to_end_offset = p->code.count - 1;

      da_foreach(ast_stmt_t*, stmt, &s->as.if_else.if_body->stmts)
        if(!codegen_stmt(p, s->as.if_else.if_scope, f, *stmt)) return false;

      if(s->as.if_else.else_body) {
        da_append(&p->code, INST_JMP);
        da_append(&p->code, 0);
        j_to_end_offset = p->code.count - 1;
        p->code.items[j_to_else_offset] = p->code.count - j_to_else_offset + 1;

        da_foreach(ast_stmt_t*, stmt, &s->as.if_else.else_body->stmts)
          if(!codegen_stmt(p, s->as.if_else.else_scope, f, *stmt)) return false;
      }

      p->code.items[j_to_end_offset] = p->code.count - j_to_end_offset + 1;
      return true;
    }
    case STMT_WHILE: {
      size_t j_to_start = p->code.count - 1;
      size_t j_to_end_offset = 0;

      struct _while* wl = &s->as.while_loop;
      if(!codegen_expr(p, scope, wl->cond)) return false;

      da_append(&p->code, INST_JZ);
      da_append(&p->code, 0);
      j_to_end_offset = p->code.count - 1;

      da_foreach(ast_stmt_t*, stmt, &wl->body->stmts)
        if(!codegen_stmt(p, wl->scope, f, *stmt)) return false;

      int32_t rel_jmp = (p->code.count - 1) - j_to_start;
      da_append(&p->code, INST_JMP);
      da_append(&p->code, -rel_jmp);

      p->code.items[j_to_end_offset] = p->code.count - j_to_end_offset + 1;

      return true;
    }
    default: 
      UNREACHABLE("codegen_stmt");
  }
}

static inline ffi_type* type_to_ffi_type(type_t t) {
  switch(t.kind) {
    case TYPE_NONE:      return &ffi_type_void;
    case TYPE_I32:       return &ffi_type_sint32;
    case TYPE_U32:       return &ffi_type_uint32;
    case TYPE_I64:       return &ffi_type_sint64;
    case TYPE_U64:       return &ffi_type_uint64;
    case TYPE_F32:       return &ffi_type_float;
    case TYPE_F64:       return &ffi_type_double;
    case TYPE_CHAR:      return &ffi_type_uchar;
    case TYPE_BOOL:      return &ffi_type_uint8;
    case TYPE_STR:       return &ffi_type_uint32; // handle
    case TYPE_ADDR:      return &ffi_type_pointer;
    case TYPE_ARRAY:     return &ffi_type_uint32; // handle
    case TYPE_STRUCT:    return &ffi_type_uint32; // handle
    case TYPE_MODULE:    return NULL;
    case TYPE_INTERFACE: return NULL;
    case TYPE_TYPE:      return NULL;
    case TYPE_ALIAS:     return type_to_ffi_type(*t.as.alias.target);
    case TYPE_FUNC:      return &ffi_type_pointer; 
    case TYPES_COUNT:
    default:
      UNREACHABLE("type_to_ffi_type - Invalid type");
 }
}

static inline bool codegen_func_def(program_t* p, ast_func_def_t* d) {
  if (d->symbol->storage == STO_EXTERN) {
    type_t* sig = d->sig->resolved_type;

    struct {
      ffi_type** items;
      size_t count;
      size_t capacity;
    } param_types = { 0 };

    for(size_t i=0; i < sig->as.func.params.count; i++)
      da_append(&param_types, type_to_ffi_type(*sig->as.func.params.items[i]));

    ffi_type* ret = type_to_ffi_type(*sig->as.func.ret);

    ffi_cif cif = { 0 };
    ffi_status status = ffi_prep_cif(
      &cif,
      FFI_DEFAULT_ABI,
      param_types.count,
      ret,
      param_types.items
    );

    if (status != FFI_OK) {
      fdiagf(stderr, DIAG_FATAL, d->loc, "Could not initialize FFI CIF");
      abort();
    }

    set_symbol_address(d->symbol, p->externs.count);

    const char* name = attributes_get(d->attributes, "name");
    if(!name) name = d->name;
    // TODO: name is leaked
    name = strdup(name);

    const char* lib = attributes_get(d->attributes, "lib");
    if(lib)
      // TODO: lib is leaked
      lib = strdup(lib);

    da_append(&p->externs, ((struct _extern){ 
      .cif = cif,
      .name = name,
      .lib = lib,
    }));
  } else {
    set_symbol_address(d->symbol, p->code.count);

    frame_t f = { 0 };

    // store parameters in local variables
    f.loc_vars = d->sig->params.count;
    // SysV-style calling convention
    for (size_t i = 0; i < f.loc_vars; i++) {
      da_append(&p->code, INST_STORE);
      da_append(&p->code, i);
    }

    da_foreach(ast_stmt_t*, stmt, &d->body->stmts)
      if(!codegen_stmt(p, d->scope, &f, *stmt)) return false;

    if(da_last(&p->code) != INST_RET)
      da_append(&p->code, INST_RET);
  }

  return true;
}

static inline int codegen(program_t* p, ast_root_t* root) {
  da_append(&p->code, INST_HALT);

  da_foreach(ast_func_def_t*, d, &root->func_defs) {
    if(!codegen_func_def(p, *d)) return false;
  }

  da_foreach(ast_var_def_t*, d, &root->var_defs) {
    uint32_t val = 0;
    if ((*d)->init) {
      switch((*d)->init->kind) {
        case EXPR_STRING:
          val = p->constants.count;
          da_append(&p->constants, ((constant_t){ .kind = DK_STR, .as.s = strdup((*d)->init->as.s) }));
          break;
        case EXPR_NUMBER:
          val = (*d)->init->as.number.u;
          break;
        case EXPR_SYMBOL:
        case EXPR_BINOP:
        case EXPR_UNOP:
        case EXPR_ACCESS:
        case EXPR_FUNCALL:
        case EXPR_SUBEXPR:
        case EXPR_ASSIGNMENT:
        default:
          UNREACHABLE("codegen - var initialization (messed up typechecker?)");
      }
    }
    set_symbol_address((*d)->symbol, p->globals.count);

    da_append(&p->globals, val);
  }
  da_foreach(ast_impl_t*, impl, &root->impls) {
    da_foreach(ast_func_def_t*, d, &(*impl)->methods) {
      if(!codegen_func_def(p, *d)) return false;
    }
  }

  da_foreach(ast_mod_t*, mod, &root->submods) {
    if(!codegen(p, (*mod)->root)) return false;
  }

  // patch addresses
  da_foreach(struct _patch, patch, &p->patches) {
    p->code.items[patch->addr] = patch->symbol->addr;
  }

  return true;
}

static inline void print_data(FILE* stream, constant_t c) {
  switch(c.kind) {
    case DK_NUMBER:
      if (c.as.number.ti & TI_LONG) {
        if (c.as.number.ti & TI_REAL) {
          fprintf(stream, "%lf", c.as.number.r);
        } else if (c.as.number.ti & TI_UNSIGNED) {
          fprintf(stream, "%" PRIu64, c.as.number.u);
        } else {
          fprintf(stream, "%" PRId64, c.as.number.i);
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
    da_foreach(symbol_t*, s, &root->symbols) {
      if(i == (*s)->addr && (*s)->kind == SYMB_FUNC && (*s)->storage != STO_EXTERN)
        fprintf(stream, "function <%s>:\n", (*s)->name);
    }
    fprintf(stream, "  0x%08" PRIx64, i);
    switch(p->code.items[i]) {
      case INST_NOP:
        fprintf(stream, "  %-10s\n", "NOP"); break;
      case INST_PUSH:
        fprintf(stream, "  %-10s 0x%08X\n", "PUSH", p->code.items[++i]); break;
      // case INST_PUSHL:
      //   fprintf(stream, "  %-10s 0x%016lX\n", "PUSHL", ((uint64_t)p->code.items[++i] << 32) | p->code.items[++i]); break;
      case INST_POP:
        fprintf(stream, "  %-10s\n", "POP"); break;
      case INST_DUP:
        fprintf(stream, "  %-10s\n", "DUP"); break;
      case INST_SWAP:
        fprintf(stream, "  %-10s\n", "SWAP"); break;
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
      case INST_LOADF:
        fprintf(stream, "  %-10s 0x%08X\n", "LOADF", p->code.items[++i]); break;
      case INST_STORE:
        fprintf(stream, "  %-10s 0x%08X\n", "STORE", p->code.items[++i]); break;
      case INST_STOREG:
        fprintf(stream, "  %-10s 0x%08X\n", "STOREG", p->code.items[++i]); break;
      case INST_STOREF:
        fprintf(stream, "  %-10s 0x%08X\n", "STOREF", p->code.items[++i]); break;
      case INST_JMP: {
        uint32_t op = p->code.items[++i];
        fprintf(stream, "  %-10s 0x%08X        \n", "JMP", op);
        break;
      }
      case INST_JNZ: {
        uint32_t op = p->code.items[++i];
        fprintf(stream, "  %-10s 0x%08X        \n", "JNZ", op);
        break;
      }
      case INST_JZ: {
        uint32_t op = p->code.items[++i];
        fprintf(stream, "  %-10s 0x%08X        \n", "JZ", op);
        break;
      }
      case INST_CALL: {
        uint32_t op = p->code.items[++i];
        fprintf(stream, "  %-10s 0x%08X        ", "CALL", op);
        da_foreach(symbol_t*, s, &root->symbols) {
          if(op == (*s)->addr && (*s)->kind == SYMB_FUNC && (*s)->storage != STO_EXTERN) {
            fprintf(stream, "    ->    ");
            fprintf(stream, "<%s>", (*s)->name);
          }
          break;
        }
        fprintf(stream, "\n");
        break;
      }
      case INST_HOSTCALL: {
        uint32_t op = p->code.items[++i];
        fprintf(stream, "  %-10s 0x%08X        ", "HOSTCALL", op);
        da_foreach(symbol_t*, s, &root->symbols) {
          if(op == (*s)->addr && (*s)->kind == SYMB_FUNC && (*s)->storage == STO_EXTERN) {
            fprintf(stream, "    ->    ");
            fprintf(stream, "<extern::%s>", (*s)->name);
          }
          break;
        }
        fprintf(stream, "\n");
        break;
      }
      case INST_RET:
        fprintf(stream, "  %-10s\n", "RET"); break;
      case INST_ADD:
        fprintf(stream, "  %-10s\n", "ADD"); break;
      case INST_SUB:
        fprintf(stream, "  %-10s\n", "SUB"); break;
      case INST_MULT:
        fprintf(stream, "  %-10s\n", "MULT"); break;
      case INST_DIVI:
        fprintf(stream, "  %-10s\n", "DIVI"); break;
      case INST_DIVU:
        fprintf(stream, "  %-10s\n", "DIVU"); break;
      case INST_REM:
        fprintf(stream, "  %-10s\n", "REM"); break;
      case INST_ADDF:
        fprintf(stream, "  %-10s\n", "ADDF"); break;
      case INST_SUBF:
        fprintf(stream, "  %-10s\n", "SUBF"); break;
      case INST_MULTF:
        fprintf(stream, "  %-10s\n", "MULTF"); break;
      case INST_DIVF:
        fprintf(stream, "  %-10s\n", "DIVF"); break;
       case INST_EQ:
        fprintf(stream, "  %-10s\n", "EQ"); break;
       case INST_LEQ:
        fprintf(stream, "  %-10s\n", "LEQ"); break;
       case INST_GEQ:
        fprintf(stream, "  %-10s\n", "GEQ"); break;
       case INST_LT:
        fprintf(stream, "  %-10s\n", "LT"); break;
       case INST_GT:
        fprintf(stream, "  %-10s\n", "GT"); break;
       case INST_HALT:
        fprintf(stream, "  %-10s\n", "HALT"); break;

      case INST_MKOBJ:
        fprintf(stream, "  %-10s 0x%08X\n", "MKOBJ", p->code.items[++i]); break;

      case INST_COUNT:
      default:
        fprintf(stream, "  %-10s\n", "<INVALID>"); break;
    }
  }
}

static inline void add_method_to_interface(type_t* interface, interface_method_t method) {
  da_append(&interface->as.interface.methods, method);
}

static inline type_t* make_interface_with_methods(typechecker_t* t, const char* name, size_t n_methods, ...) {
  va_list args;

  type_t* interface = make_type(t, TYPE_INTERFACE, name, 0);

  va_start(args, n_methods);

  for(size_t i=0; i<n_methods; i++) {
    da_append(&interface->as.interface.methods, va_arg(args, interface_method_t));
  }

  va_end(args);

  return interface;
}

static inline type_t* method_owner(symbol_t* s) {
  return (*s->type->as.func.params.items);
}

// TODO: save slice_t instead of sb_t in tokenizer
static inline bool compile(program_t* program, const char* path, const sb_t* source) {
  tokenizer_t tok = { 0 };
  tok.source = slice_from_sb(*source);
  tok.path = path;

  parser_t parser = { 0 };
  typechecker_t tc = { 0 };

  printf("%.*s\n\n", SB_FMT(*source));

  if(tok_tokenize(&tok)) return false;
  fprintf(stdout, "%s%s\033[0m Tokenization OK.\n", diag_lvl_color[DIAG_DEBUG], diag_lvl_txt[DIAG_DEBUG]);

  // da_foreach(token_t, t, &tok.tokens) {
  //   tok_print(*t);
  // }

  parser_init(&parser, &tok);
  ast_root_t* root = parse(&parser);
  if(!root) return false;
  fprintf(stdout, "%s%s\033[0m Parsing OK.\n", diag_lvl_color[DIAG_DEBUG], diag_lvl_txt[DIAG_DEBUG]);

  // print_ast(stdout, (ast_node_t*)root, 0);

  typechecker_init(&tc);
  if(!typecheck(&tc, root)) return false;
  fprintf(stdout, "%s%s\033[0m Typechecker OK.\n", diag_lvl_color[DIAG_DEBUG], diag_lvl_txt[DIAG_DEBUG]);

  if(!codegen(program, root)) return false;
  fprintf(stdout, "%s%s\033[0m Codegen OK.\n", diag_lvl_color[DIAG_DEBUG], diag_lvl_txt[DIAG_DEBUG]);

  // print_symbol_table(stdout, root->scope);

  print_disass(stdout, program, root->scope);

  tok_destroy(&tok);
  parser_destroy(&parser);
  typechecker_destroy(&tc);

  return true;
}

bool compile_from_file(program_t* program, const char* path) {
  sb_t sb = { 0 };
  if (sb_read_file(path, &sb) < 0) {
    fprintf(stdout, "%s%s\033[0m Could Not Open file `%s`.\n", diag_lvl_color[DIAG_FATAL], diag_lvl_txt[DIAG_FATAL], path);
    return false;
  }
  sb_appendz(&sb, "");
  bool res = compile(program, path, &sb);
  sb_free(&sb);
  return res;
}

bool compile_from_cstr(program_t* program, const char* source) {
  sb_t sb = { 0 };
  sb_appendz(&sb, source);
  bool res = compile(program, NULL, &sb);
  sb_free(&sb);
  return res;
}
