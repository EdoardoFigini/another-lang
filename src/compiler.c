#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include <ffi.h>

#include "sb.h"
#include "da.h"
#include "arena.h"

#include "types.h"
#include "macros.h"

#include "vm.h"

const char* tok_keywords[] = {
  [KW_RETURN] = "return",
  [KW_EXTERN] = "extern",
  [KW_EXPORT] = "export",
  [KW_CONST]  = "const",
  [KW_IF]     = "if",
  [KW_ELSE]   = "else",
};

void tok_print(token_t tok) {
  printf("%zu:%zu: ", tok.line, tok.col);
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
        return tok_keywords[k];
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
          fprintf(stderr, "[ERROR] %zu:%zu: Unknown escape sequence\n", line, col);
          return NULL;
      }

      escape = 0;
    } else {
      if      (**end == '\"') break;
      else if (**end == '\\') escape = 1;
      else if (**end == '\n') {
        fprintf(stderr, "[ERROR] %zu:%zu: Missing closing `\"`\n", line, col);
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

  // TODO: N-2 now gets tokenized as TOK_ID '-2', instead of
  // TOK_ID '-' '2', unless '-' and '2' are separated by a space
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
      case '%':
        tok = (token_t){ .kind = *p, .start = p, .len = 1 }; 
        break;
      case '>':
        switch(*(p + 1)) {
          case '=': tok = (token_t){ .kind = TOK_GEQ, .start = p, .len = 2 }; break;
          default: 
            tok = (token_t){ .kind = *p, .start = p, .len = 1 };
            break;
        }
        break;
      case '<':
        switch(*(p + 1)) {
          case '=': tok = (token_t){ .kind = TOK_LEQ, .start = p, .len = 2 }; break;
          default: 
            tok = (token_t){ .kind = *p, .start = p, .len = 1 };
            break;
        }
        break;
      case '=':
        switch(*(p + 1)) {
          case '=': tok = (token_t){ .kind = TOK_EQEQ, .start = p, .len = 2 }; break;
          default: 
            tok = (token_t){ .kind = *p, .start = p, .len = 1 };
            break;
        }
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
        if(isalpha(*p) || *p == '_') {
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
          for (; *end && (isalpha(*end) || isdigit(*end) || *end == '_'); end++);
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
          fprintf(stderr, "[ERROR] Lexer:\n  %zu:%zu Unrecognized token (%c, 0x%x)\n%.*s\n%*s^\n", line, col, *p, *p, line_length, line_start, (int)col - 1, "");
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

static inline token_t get_tok(parser_t* p) {
  if (p->current >= p->tokens.count) {
    fprintf(stderr, "[FATAL] Lexer\n  No more tokens.");
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

static inline void print_type(FILE* stream, const type_t* t);

static inline void set_symbol_address(symbol_t* s, uint32_t addr) {
  s->addr = addr;
  s->addr_resolved = true;
}

static inline void print_symbol_table(FILE* stream, scope_t* root);

// FIXME: dangling pointers
static inline symbol_t* make_symbol(scope_t* scope, symb_kind_t kind, symb_storage_t storage, const char* name, type_t* type) {
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

static inline int type_equals(type_t a, type_t b);

static inline symbol_t* resolve_symbol_local(scope_t* scope, const char* name, symb_kind_t kind) { 
  if (!scope) return NULL;

  // OPTIMIZE
  da_foreach(symbol_t, s, &scope->symbols) {
    if(strcmp(s->name, name) == 0 && s->kind == kind) return s;
  }
  return NULL;
}

static inline symbol_t* resolve_symbol_any(scope_t* scope, const char* name) {
  if (!scope) return NULL;

  // OPTIMIZE
  da_foreach(symbol_t, s, &scope->symbols) {
    if(strcmp(s->name, name) == 0) return s;
  }
  return resolve_symbol_any(scope->parent, name);
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

static inline bool tok_is_specifier(parser_t* p) {
  switch((kw_kind_t)get_tok(p).kind) {
    case KW_EXTERN:
    case KW_EXPORT:
    case KW_CONST:
      return true;
    case KW_RETURN:
    case KW_IF:
    case KW_ELSE:
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
      default: return flags;
    }

    if (flags & cur_flag)
      DIAGF(p, WARN, "Duplicate specifier `%.*s`, will be considered as one.\n", t.len, t.start);

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
  [OP_SCOPE]   = BP_ACCESS,
};

static inline ast_expr_t* parse_expr(parser_t* p, int bp);

static inline ast_expr_t* parse_primary_expr(parser_t* p) {
  ast_expr_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_EXPR;

  if (tok_is(p, TOK_IDENT)) {
    n->kind = EXPR_SYMBOL;

    if(!expect(p, TOK_IDENT)) return NULL;
    const char* name = arena_strdup(&p->arena, get_tok(p).as.s);
    next(p);

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
      case OP_SCOPE: 
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

static inline ast_type_t* parse_type(parser_t* p) {
  ast_type_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_TYPE;

  if(!expect_with_name(p, TOK_IDENT, "type")) return NULL;
  token_t tok = get_tok(p);
  next(p);

  n->name = arena_strdup(&p->arena, tok.as.s);
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

static inline ast_decl_t* parse_var_decl(parser_t* p, const char* name, spec_flags_t flags);
static inline ast_def_t* parse_var_def(parser_t* p, ast_decl_t* decl);
static inline ast_body_t* parse_body(parser_t* p);

// stmt = return [ expr ] ';' 
//      | if '(' expr ')' body [ else body ]
//      | ( 'specifier' )* ident ':' type [ '=' expr ] ';'
//      | expr ';'
//      | ';'
static inline ast_stmt_t* parse_stmt(parser_t* p) {
  ast_stmt_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_STMT;

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

    if(!expect(p, '(')) return NULL;
    next(p);
    ast_expr_t* expr = parse_expr(p, 0);
    if(!expr) return NULL;
    n->as.if_else.cond = expr;
    if(!expect(p, ')')) return NULL;
    next(p);

    enter_scope_new(p, arena_sprintf(&p->arena, "%s_if", p->current_scope->name));
    n->as.if_else.scope = p->current_scope;
    ast_body_t* if_body = parse_body(p);
    n->as.if_else.if_body = if_body;
    exit_scope(p);

    if(tok_is(p, TOK_ELSE)) {
      if(!expect(p, TOK_ELSE)) return NULL;
      next(p);

      enter_scope_new(p, arena_sprintf(&p->arena, "%s_else", p->current_scope->name));
      n->as.if_else.scope = p->current_scope;
      ast_body_t* else_body = parse_body(p);
      n->as.if_else.else_body = else_body;
      exit_scope(p);
    }
    // TODO: handle else if (elif)

    return n;

  } else if (tok_is(p, TOK_IDENT) || tok_is_specifier(p)) {
    size_t saved = p->current;
    spec_flags_t flags = 0;
    if(tok_is_specifier(p)) flags = parse_specifiers(p);

    if(!expect(p, TOK_IDENT)) return NULL;
    const char* name = get_tok(p).as.s;
    next(p);

    if(tok_is(p, ':')) {
      if(!expect(p, ':')) return NULL;
      next(p);
      n->as.var_def.name = arena_strdup(&p->arena, name);
      n->as.var_def.flags = flags;
      n->kind = STMT_VAR_DEF;

      // TODO:  type inference
      ast_type_t* t = parse_type(p);
      if(!t) return NULL;
      n->as.var_def.type = t;

      n->as.var_def.symbol = make_symbol(p->current_scope, SYMB_VAR, STO_LOCAL, n->as.var_def.name, NULL);

      if (tok_is(p, '=')) {
        n->as.var_def.initialized = true;
        if(!expect(p, '=')) return NULL;
        next(p);

        ast_expr_t* init = parse_expr(p, 0);
        if(!init) return NULL;
        n->as.var_def.init = init;
      }

      if(!expect(p,';')) return NULL;
      next(p);
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

  if(!expect(p, TOK_IDENT)) return NULL;
  n->name = arena_strdup(&p->arena, get_tok(p).as.s);
  next(p);

  if(!expect(p, ':')) return NULL;
  next(p);

  ast_type_t* t = parse_type(p);
  if(!t) return NULL;
  n->type = t;

  n->symbol = make_symbol(p->current_scope, SYMB_VAR, STO_LOCAL, n->name, NULL);

  return n;
}

// sig = '(' [ param ( ',' param )* ] ')' -> ident 
static inline ast_sig_t* parse_signature(parser_t* p) {
  ast_sig_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_SIG;

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

// func_def = func_decl body
static inline ast_def_t* parse_func_def(parser_t* p, ast_decl_t* decl) {
  // NOTE: skip until '{', assume declaration has been correctly parsed by
  // parse_func_decl in previous pass
  while(!tok_is(p, '{')) next(p);

  ast_def_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_FUNC_DEF;
  n->decl = decl;

  enter_scope(p, n->decl->as.fun.scope);

  ast_body_t* body = parse_body(p);
  if(!body) return NULL;

  n->body = body;

  exit_scope(p);

  return n;
}

// var_def = var_decl [ '=' expr ] ';'
static inline ast_def_t* parse_var_def(parser_t* p, ast_decl_t* decl) {
  while(!tok_is(p, '=') && !tok_is(p, ';')) next(p);

  ast_def_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_VAR_DEF;
  n->decl = decl;

  if(tok_is(p, '=')) {
    if(!expect(p, '=')) return NULL;
    next(p);
    ast_expr_t* init = parse_expr(p, 0);
    if(!init) return NULL;
    n->init = init;
  }

  if(!expect(p, ';')) return NULL;
  next(p);

  return n;
}

// func_decl = sig ( ';' | '{' )
static inline ast_decl_t* parse_func_decl(parser_t* p, const char* name, spec_flags_t flags) {
  ast_decl_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_FUNC_DECL;
  n->flags = flags;
  n->name = arena_strdup(&p->arena, name);
  n->kind = DECL_KIND_FUNC;
  n->tok_idx = p->current;

  symb_storage_t sto = STO_GLOBAL;
  if(flags & SPEC_EXTERN) sto = STO_EXTERN; 
  if(flags & SPEC_EXPORT) sto = STO_EXPORT; 

  n->symbol = make_symbol(p->current_scope, SYMB_FUNC, sto, n->name, NULL);

  enter_scope_new(p, arena_sprintf(&p->arena, "fun_%s", name));
  n->as.fun.scope = p->current_scope;

  n->as.fun.sig = parse_signature(p);
  if(!n->as.fun.sig) return NULL;

  if (tok_is(p, ';')) {
    if(!expect(p, ';')) return NULL;
    next(p);
  } else {
    if(!expect(p, '{')) return NULL;
    next(p);
    n->as.fun.has_body = true;
    // NOTE: delay body parsing to parse_func_decl
    for (int pars = 1; pars && !tok_is(p, TOK_EOF); next(p)) {
      if      (tok_is(p,'{')) pars++;
      else if (tok_is(p,'}')) pars--;
    }
  }

  exit_scope(p);

  return n;
}

static inline ast_decl_t* parse_var_decl(parser_t* p, const char* name, spec_flags_t flags) {
  ast_decl_t* n = arena_alloc(&p->arena, sizeof(*n));
  n->ast_kind = AST_VAR_DECL;
  n->flags = flags;
  n->name = arena_strdup(&p->arena, name);
  n->kind = DECL_KIND_VAR;
  n->tok_idx = p->current;

  symb_storage_t sto = STO_GLOBAL;
  if(flags & SPEC_EXTERN) sto = STO_EXTERN; 
  if(flags & SPEC_EXPORT) sto = STO_EXPORT;

  n->symbol = make_symbol(p->current_scope, SYMB_VAR, sto, n->name, NULL);

  // TODO:  type inference
  ast_type_t* type = parse_type(p);
  if(!type) return NULL;
  n->as.var.type = type;

  if(tok_is(p, '=')) n->as.var.initialized = true;
  while(!tok_is(p, ';')) next(p); // skip initialization if present

  if(!expect(p, ';')) return NULL;
  next(p);

  return n;
}

// root: ( ident ':' func_decl [ func_def ] )* 
//     | ident ':' var_def
static inline ast_root_t* parse(parser_t* p, tokenizer_t* t) {
  p->source = t->source;
  p->tokens = t->tokens;
  p->current = 0;
  p->pass = PASS_DECL;

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
      ast_decl_t* decl = parse_func_decl(p, name, flags);
      if (!decl) return NULL;

      da_append(&n->decls, decl);
    } else {
      ast_decl_t* decl = parse_var_decl(p, name, flags);
      if (!decl) return NULL;

      da_append(&n->decls, decl);
    }
  }

  p->current = 0;
  p->pass++;

  da_foreach(ast_decl_t*, decl, &n->decls) {
    if((*decl)->flags & SPEC_EXTERN) continue;

    ast_def_t* def = NULL;
    p->current = (*decl)->tok_idx;
    if ((*decl)->kind == DECL_KIND_FUNC) {
      def = parse_func_def(p, *decl);
    } else if ((*decl)->kind == DECL_KIND_VAR) {
      def = parse_var_def(p, *decl);
    } else {
      UNREACHABLE("parse definition");
    }

    if(!def) return NULL;

    da_append(&n->top_level, def);
  }

  return n;
}

static inline builtin_method_t* resolve_builtin_method(const struct _builtin_methods* methods, const type_t* owner, const char* name) {
  // OPTIMIZE
  da_foreach(builtin_method_t, m, methods) {
    if(owner == m->owner && strcmp(name, m->name) == 0) return m;
  }
  return NULL;
}

static inline void print_ast(FILE* stream, ast_node_t* n, int level) {
  switch(n->ast_kind) {
    case AST_ROOT:
      ast_root_t* root = (ast_root_t*)n;
      fprintf(stream, "%*s%s\n", level, "","AST_ROOT");
      fprintf(stream, "%*s%s\n", level + 2, "","DECLARATIONS:");
      da_foreach(ast_decl_t*, d, &root->decls) {
        print_ast(stream, (ast_node_t*)*d, level + 4);
      }
      fprintf(stream, "%*s%s\n", level + 2, "","DEFINITIONS:");
      da_foreach(ast_def_t*, d, &root->top_level) {
        print_ast(stream, (ast_node_t*)*d, level + 4);
      }
      break;
    case AST_EXPR:
      ast_expr_t* expr = (ast_expr_t*)n;
      fprintf(stream, "%*s%s", level, "", "AST_EXPR");
      switch(expr->kind) {
        case EXPR_SYMBOL:
          fprintf(stream, " (%s)\n", "EXPR_SYMBOL");
          fprintf(stream, "%*s%s\n", level + 2, "", expr->as.symbol);
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
          if(expr->as.access.op == OP_MEMB) {
            fprintf(stream, "%*sobject:\n", level + 2, "");
          } else if (expr->as.access.op == OP_SCOPE) {
            fprintf(stream, "%*smodule:\n", level + 2, "");
          } else {
            UNREACHABLE("print_ast: ACCESS op");
          }
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
          fprintf(stream, "%*sName: %s\n", level + 2, "", stmt->as.var_def.name);
          fprintf(stream, "%*sFlags: ", level + 2, "");
          if(stmt->as.var_def.flags & SPEC_EXTERN) fprintf(stream, "extern ");
          if(stmt->as.var_def.flags & SPEC_EXPORT) fprintf(stream, "export ");
          if(stmt->as.var_def.flags & SPEC_CONST)  fprintf(stream, "const ");
          fprintf(stream, "\n");
          if (stmt->as.var_def.initialized)
            print_ast(stream, (ast_node_t*)stmt->as.var_def.init, level + 2);
          break;
        case STMT_IF:
          TODO("print_ast: STMT_IF");
        default: break;
      }
      break;
    case AST_VAR_DECL:
      ast_decl_t* decl = (ast_decl_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_VAR_DECL");
      fprintf(stream, "%*sName: %s\n", level + 2, "", decl->name);
      fprintf(stream, "%*sFlags: ", level + 2, "");
      if(decl->flags & SPEC_EXTERN) fprintf(stream, "extern ");
      if(decl->flags & SPEC_EXPORT) fprintf(stream, "export ");
      if(decl->flags & SPEC_CONST)  fprintf(stream, "const ");
      fprintf(stream, "\n");
      break;
    case AST_VAR_DEF:
      fprintf(stream, "%*s%s\n", level, "", "AST_VAR_DEF");
      print_ast(stream, (ast_node_t*)((ast_def_t*)n)->decl, level + 2);
      if(((ast_def_t*)n)->decl->as.var.initialized)
        print_ast(stream, (ast_node_t*)((ast_def_t*)n)->init, level + 2);
      break;
    case AST_FUNC_DECL:
      decl = (ast_decl_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_FUNC_DECL");
      fprintf(stream, "%*sName: %s\n", level + 2, "", decl->name);
      fprintf(stream, "%*sFlags: ", level + 2, "");
      if(decl->flags & SPEC_EXTERN) fprintf(stream, "extern ");
      if(decl->flags & SPEC_EXPORT) fprintf(stream, "export ");
      if(decl->flags & SPEC_CONST)  fprintf(stream, "const ");
      fprintf(stream, "\n");
      print_ast(stream, (ast_node_t*)decl->as.fun.sig, level + 2);
      break;
    case AST_FUNC_DEF:
      ast_def_t* def = (ast_def_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_FUNC_DEF");
      print_ast(stream, (ast_node_t*)def->decl, level + 2);
      if(def->body)
        print_ast(stream, (ast_node_t*)def->body, level + 2);
      break;
    case AST_TYPE:
      fprintf(stream, "%*s%s\n", level, "", "AST_TYPE");
      fprintf(stream, "%*sName: %s\n", level + 2, "", ((ast_type_t*)n)->name);
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
      UNREACHABLE("print_ast %d", n->ast_kind);
  }
}

static inline void print_symbol_table_entries(FILE* stream, scope_t* scope) {
  if(!scope) return;
  da_foreach(symbol_t, s, &scope->symbols) {
    fprintf(stream, "%-10s %-10s ", s->name, scope->name);
    switch(s->storage) {
      case STO_LOCAL:
        fprintf(stream, "%-10s ", "scope"); break;
      case STO_GLOBAL:
        fprintf(stream, "%-10s ", "global"); break;
      case STO_EXTERN:
        fprintf(stream, "%-10s ", "extern"); break;
      case STO_EXPORT:
        fprintf(stream, "%-10s ", "export"); break;
      default: break;
    }
    fprintf(stream, "0x%08X ", s->addr);
    print_type(stream, s->type);
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
    case TYPE_MODULE:
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

static inline void print_type(FILE* stream, const type_t* t) {
  if(!t) UNREACHABLE("print_type: null type*");
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
    case TYPE_MODULE:
      fprintf(stream, "module %s", t->name);
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

static inline void typechecker_destroy(typechecker_t* t) {
  arena_free(&t->arena);
  da_free(t->custom_types);
}

static inline type_t* make_type(typechecker_t* t, type_kind_t k, const char* name, size_t size) {
  type_t* type = arena_alloc(&t->arena, sizeof(*type));
  type->kind = k;
  type->name = name;
  type->size = size;

  return type;
}

// rename to get_or_create_array_type_of
static inline type_t* get_or_create_array_of(typechecker_t* t, const type_t* type) {
  // OPTIMIZE
  da_foreach(type_t*, typ, &t->custom_types) {
    if ((*typ)->kind == TYPE_ARRAY && type_equals(*(*typ)->as.array.inner, *type)) return *typ;
  }

  type_t* typ = make_type(t, TYPE_ARRAY, NULL, 1);
  type_equals(*typ->as.array.inner, *(type_t*)type);
  da_append(&t->custom_types, typ);
  
  return typ; 
}

static inline type_t* get_or_create_func_type_of(typechecker_t* t, const type_t* const* params, size_t n_params, const type_t* ret_type) {
  // OPTIMIZE
  da_foreach(type_t*, typ, &t->custom_types) {
    if ((*typ)->kind == TYPE_FUNC) {
      if ((*typ)->as.func.params.count != n_params) continue;
      if (!type_equals(*(*typ)->as.func.ret, *ret_type)) continue;
      bool same = false;
      for (size_t i = 0; i < n_params && !same; i++) {
        same = type_equals(*(*typ)->as.func.params.items[i], *params[i]);
      }
      if (same)
        return *typ;
    }
  }

  type_t* type = make_type(t, TYPE_FUNC, NULL, 1); // which is the correct size?
  for(size_t i=0; i < n_params; i++) {
    da_append(&type->as.func.params, (type_t*)params[i]);
  }
  type->as.func.ret = (type_t*)ret_type;

  return type;
}

static inline type_t* resolve_type(typechecker_t* t, const char* name, int depth) {
  type_t* res = NULL;

  if (strcmp(name, t->builtins.none->name) == 0) 
    res = (type_t*)t->builtins.none;
  else if (strcmp(name, t->builtins.i32->name) == 0) 
    res = (type_t*)t->builtins.i32;
  else if (strcmp(name, t->builtins.i64->name) == 0) 
    res = (type_t*)t->builtins.i64;
  else if (strcmp(name, t->builtins.u32->name) == 0) 
    res = (type_t*)t->builtins.u32;
  else if (strcmp(name, t->builtins.u64->name) == 0) 
    res = (type_t*)t->builtins.u64;
  else if (strcmp(name, t->builtins.f32->name) == 0) 
    res = (type_t*)t->builtins.f32;
  else if (strcmp(name, t->builtins.f64->name) == 0) 
    res = (type_t*)t->builtins.f64;
  else if (strcmp(name, t->builtins.boolean->name) == 0) 
    res = (type_t*)t->builtins.boolean;
  else if (strcmp(name, t->builtins.character->name) == 0) 
    res = (type_t*)t->builtins.character;
  else if (strcmp(name, t->builtins.str->name) == 0) 
    res = (type_t*)t->builtins.str;
  else if (strcmp(name, t->builtins.addr->name) == 0) 
    res = (type_t*)t->builtins.addr;

  if (!res) {
    da_foreach(type_t*, type, &t->custom_types) {
      // OPTIMIZE
      if((*type)->name && strcmp((*type)->name, name) == 0) { res = *type; break; }
    }
  }

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
    case TYPE_NONE:
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

i_method_t* find_binop_impl(const type_t* owner, op_kind_t op, const type_t* other) {
  (void)owner;
  (void)op;
  (void)other;
  TODO("find_binop_impl");
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
    // TODO: look for compare interface implementation in left and right
    fprintf(stderr, "[ERROR] Typechecker\n  Cannot compare expression of type `");
    print_type(stderr, left);
    fprintf(stderr, "` with expression of type `");
    print_type(stderr, right);
    fprintf(stderr, "`.\n");
    return false;

  } else if (is_op_arithmetic(op)) {
    if (is_numeric(left) && is_numeric(right)) {
      const type_t* try_promotion = find_common_type(left, right);
      if (!try_promotion) {
        fprintf(stderr, "[ERROR] Typechecker\n  ");
        return false;
      }
      e->type = try_promotion;
      return true;
    }

    // TODO: is this actually the correct way? 
    i_method_t* impl = NULL;
    if      (left->kind  == TYPE_STRUCT) impl = find_binop_impl(left, op, right);
    else if (right->kind == TYPE_STRUCT) impl = find_binop_impl(right, op, left);
    if (!impl) {
      fprintf(stderr, "[ERROR] Typechecker\n  ");
      print_type(stderr, left);
      fprintf(stderr, " does not implement interface for `%s` operator and ", tok_kind_str(&t->arena, (tok_kind_t)op));
      print_type(stderr, right);
      fprintf(stderr, " operand");
      fprintf(stderr, "[ERROR] Typechecker\n  ");
      print_type(stderr, right);
      fprintf(stderr, " does not implement interface for `%s` operator and ", tok_kind_str(&t->arena, (tok_kind_t)op));
      print_type(stderr, left);
      fprintf(stderr, " operand");
      return false;
    }
    e->type = impl->owner;
    return true;
  }

  // TODO: unhardcode concat/extend operation (impl?)
  if (op == OP_PLUS && left->kind == TYPE_STR && right->kind == TYPE_STR) {
    e->type = t->builtins.str;
    return true;
  }
  if (op == OP_PLUS && left->kind == TYPE_ARRAY && type_equals(*left, *right)) {
    e->type = left;
    return true;
  }

  return false;
}

static inline bool typecheck_expr(typechecker_t* t, ast_expr_t* expr) {
  expr->is_const = false;
  switch(expr->kind) {
    case EXPR_SYMBOL:
      symbol_t* s = resolve_symbol_any(t->current_scope, expr->as.symbol); 
      if (!s) {
        fprintf(stderr, "[ERROR] Typechecker\n No symbol `%s` in current scope.\n", expr->as.symbol);
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
    case EXPR_ACCESS:
      if(!typecheck_expr(t, expr->as.access.owner)) return false;

      if (expr->as.access.op == OP_SCOPE) {
        if (expr->as.access.owner->type->kind != TYPE_MODULE) {
          fprintf(stderr, "[ERROR] Typechecker\n Owner is not a module.\n");
          return false;
        }
        TODO("typecheck_expr: EXPR_ACCESS - module fields");
      } else if (expr->as.access.op == OP_MEMB) {
        if (expr->as.access.owner->type->kind == TYPE_STRUCT) {
          TODO("typecheck_expr: EXPR_ACCESS - struct fields");
        } else if (expr->as.access.owner->type->kind == TYPE_STR) {
          if (strcmp(expr->as.access.field, "c_str") == 0) {  // c_str: () -> addr
                                                              // should probably have (self: str) -> addr
            // TODO: use typed ptrs instead of generic addr
            expr->type = get_or_create_func_type_of(t, NULL, 0, t->builtins.addr);
          }
        } else {
          fprintf(stderr, "[ERROR] Typechecker\n Owner is not a built-in object or structure.\n");
          return false;
        }
        return true;
      } else {
        UNREACHABLE("typecheck_expr: EXPR_ACCESS");
      }
      return true;
    case EXPR_FUNCALL:
      ast_expr_t* callee = expr->as.funcall.callee;
      if(!typecheck_expr(t, callee)) return false;

      if (callee->type->kind != TYPE_FUNC) {
        fprintf(stderr, "[ERROR] Typechecker\n  Attempting to call non-callable type ");
        print_type(stderr, callee->type);
        fprintf(stderr, ".\n");
        return false;
      }

      if (expr->as.funcall.args.count != callee->type->as.func.params.count) {
          fprintf(
            stderr, 
            "[ERROR] Typechecker\n  "
            "Mismatched number of arguments in function call: expected %zu, got %zu.\n",
            callee->type->as.func.params.count,
            expr->as.funcall.args.count
          );
          return false;
      }

      for(size_t i = 0; i < expr->as.funcall.args.count; i++) {
        ast_expr_t* arg = da_at(expr->as.funcall.args, i);
        if(!typecheck_expr(t, arg)) return false;
        type_t const* a = arg->type;
        type_t const* b = da_at(callee->type->as.func.params, i);
        if (!type_equals(*a, *b)) {
          // TODO: find way to retrieve argument name for better diagnostics
          fprintf(stderr, "[ERROR] Typechecker\n  Mismatched type for argument %zu in function call: expected ", i);
          print_type(stderr, b);
          fprintf(stderr, ", got ");
          print_type(stderr, a);
          fprintf(stderr, ".\n");
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
        fprintf(stderr, "[ERROR] Typechecker\n  Incompatible types when assigning expression of type `");
        print_type(stderr, expr->as.binop.rhs->type);
        fprintf(stderr, "` to variable of type `");
        print_type(stderr, expr->as.binop.lhs->type );
        fprintf(stderr, "`.\n");
        return false;
      }
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
    fprintf(stderr, "[ERROR] Typechecker\n  Undefined type `%s`.\n", type->name);
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
        fprintf(stderr, "[ERROR] Typechecker\n  Incompatible return type: have `");
        print_type(stderr, stmt->as.retval->type);
        fprintf(stderr, "`, expect `");
        print_type(stderr, ret_type);
        fprintf(stderr, "`.\n");
        return false; 
      }
      break;
    case STMT_EXPR:
      if(!typecheck_expr(t, stmt->as.expression)) return false;
      break;
    case STMT_VAR_DEF:
      if(!resolve_type_decl(t, stmt->as.var_def.type)) return false;
      stmt->as.var_def.symbol->type = stmt->as.var_def.type->resolved_type;

      if( stmt->as.var_def.flags & SPEC_EXTERN ) {
        fprintf(stderr, "[ERROR] Typechecker\n  Cannot define an extern local variable.\n");
        return false;
      }
      if( stmt->as.var_def.flags & SPEC_EXPORT ) {
        fprintf(stderr, "[ERROR] Typechecker\n  Cannot export a local variable.\n");
        return false;
      }

      // TODO: type inference
      da_foreach(symbol_t, symb, &t->current_scope->symbols) {
        if (
            symb != stmt->as.var_def.symbol && 
            symb->kind == SYMB_VAR &&
            strcmp(stmt->as.var_def.name, symb->name) == 0
        ) {
          fprintf(stderr, "[ERROR] Typechecker\n  Multiple definitions for variable `%s` in this scope", stmt->as.var_def.name);
          return false;
        }
        // NOTE: allow shadowing -> do not check in parent scope
        // TODO: maybe issue shadowing warning?
      }

      if(stmt->as.var_def.initialized) {
        if(!typecheck_expr(t, stmt->as.var_def.init)) return false;
        if(stmt->as.var_def.type->resolved_type != stmt->as.var_def.init->type) {
          fprintf(stderr, "[ERROR] Typechecker\n  Incompatible types when initializing type `");
          print_type(stderr, stmt->as.var_def.type->resolved_type);
          fprintf(stderr, "` using type `");
          print_type(stderr, stmt->as.var_def.init->type);
          fprintf(stderr, "`.\n");
          return false;
        }
      }
      break;
    case STMT_IF:
      struct _if_else* if_else = &stmt->as.if_else;
      if(!typecheck_expr(t, if_else->cond)) return false;
      if(!type_equals(*if_else->cond->type, *t->builtins.boolean)) {
        fprintf(stderr, "[ERROR] Typechecker\n  Incompatible type in condition: expected `");
        print_type(stderr, t->builtins.boolean);
        fprintf(stderr, "`, got `");
        print_type(stderr, if_else->cond->type);
        fprintf(stderr, "`.\n");
        return false;
      }

      t->current_scope = if_else->scope;
      da_foreach(ast_stmt_t*, stmt, &if_else->if_body->stmts) {
        if(!typecheck_stmt(t, *stmt, ret_type)) return false;
      }
      t->current_scope = t->current_scope->parent;

      if(if_else->else_body) {
        t->current_scope = if_else->scope;
        da_foreach(ast_stmt_t*, stmt, &if_else->else_body->stmts) {
          if(!typecheck_stmt(t, *stmt, ret_type)) return false;
        }
        t->current_scope = t->current_scope->parent;
      }
      break;
    default:
      UNREACHABLE("typecheck_stmt");
  }

  return true;
}

static inline bool resolve_func_decl(typechecker_t* t, ast_decl_t* decl) {
  // TODO: use get_or_create_func_type_of to avoid having duplicate types,
  // especially for very common signatures (() -> none, () -> bool, etc.)
  type_t* sig_type = make_type(t, TYPE_FUNC, "", 1);

  da_foreach(ast_param_t*, p, &decl->as.fun.sig->params) {
    if(!resolve_type_decl(t, (*p)->type)) return false;
    (*p)->symbol->type = (*p)->type->resolved_type;
    da_append(&sig_type->as.func.params, (*p)->type->resolved_type);
  }

  if(!resolve_type_decl(t, decl->as.fun.sig->ret)) return false;
  sig_type->as.func.ret = decl->as.fun.sig->ret->resolved_type;

  da_append(&t->custom_types, sig_type);
  decl->as.fun.sig->resolved_type = sig_type;
  decl->symbol->type = sig_type;

  return true;
}

static inline void init_typechecker(typechecker_t* t) {
  // init builtin types
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
}

static inline bool typecheck(typechecker_t* t, ast_root_t* root) {

  t->current_scope = root->scope;

  da_foreach(ast_decl_t*, d, &root->decls) {
    switch((*d)->kind) {
      case DECL_KIND_FUNC:
        if(!resolve_func_decl(t, *d)) return false;

        if(!(*d)->as.fun.has_body && !((*d)->flags & SPEC_EXTERN)) {
          fprintf(stderr, "[ERROR] Typechecker\n  Cannot define a non-extern function without a body.\n");
          return false;
        }
        if((*d)->as.fun.has_body && ((*d)->flags & SPEC_EXTERN)) {
          fprintf(stderr, "[ERROR] Typechecker\n  Cannot define an extern function with a body.\n");
          return false;
        }
        break;
      case DECL_KIND_VAR:
        if(!resolve_type_decl(t, (*d)->as.var.type)) return false;
        (*d)->symbol->type = (*d)->as.var.type->resolved_type;

        if((*d)->as.var.initialized && ((*d)->flags & SPEC_EXTERN)) {
          fprintf(stderr, "[ERROR] Typechecker\n  Cannot initialize an extern variable.\n");
          return false;
        }

        break;
      default:
        UNREACHABLE("typecheck");
    }
  }
  da_foreach(ast_def_t*, d, &root->top_level) {
    switch((*d)->decl->kind) {
      case DECL_KIND_FUNC:
        da_foreach(symbol_t, symb, &t->current_scope->symbols) {
          if (
              symb != (*d)->decl->symbol && 
              symb->kind == SYMB_FUNC &&
              strcmp((*d)->decl->name, symb->name) == 0 &&
              type_equals(*symb->type, *(*d)->decl->as.fun.sig->resolved_type)
          ) {
            fprintf(stderr, "[ERROR] Typechecker\n  Multiple definitions for function `%s` with type ", (*d)->decl->name);
            print_type(stderr, (*d)->decl->as.fun.sig->resolved_type);
            fprintf(stderr, " in this scope.\n");
            return false;
          }
        }
        // NOTE: allow shadowing -> do not check in parent scope
        // TODO: maybe issue shadowing warning?

        t->current_scope = (*d)->decl->as.fun.scope;
        da_foreach(ast_stmt_t*, stmt, &(*d)->body->stmts) {
          if(!typecheck_stmt(t, *stmt, (*d)->decl->as.fun.sig->ret->resolved_type)) return false;
        }
        t->current_scope = t->current_scope->parent;
        break;
      case DECL_KIND_VAR:
        da_foreach(symbol_t, symb, &t->current_scope->symbols) {
          if (
              symb != (*d)->decl->symbol && 
              symb->kind == SYMB_VAR &&
              strcmp((*d)->decl->name, symb->name) == 0
          ) {
            fprintf(stderr, "[ERROR] Typechecker\n  Multiple definitions for variable `%s` in this scope.\n", (*d)->decl->name);
            return false;
          }
          // NOTE: allow shadowing -> do not check in parent scope
          // TODO: maybe issue shadowing warning?
        }

        if((*d)->decl->as.var.initialized) {
          if(!typecheck_expr(t, (*d)->init)) return false;
          if(!(*d)->init->is_const) {
            fprintf(stderr, "[ERROR] Typechecker\n  Initializer is not a compile-time constant.\n");
            return false;
          }

          if(!type_equals(*(*d)->decl->as.var.type->resolved_type, *(*d)->init->type)) {
            fprintf(stderr, "[ERROR] Typechecker\n  Incompatible types when initializing type `");
            print_type(stderr, (*d)->decl->as.var.type->resolved_type);
            fprintf(stderr, "` using type `");
            print_type(stderr, (*d)->init->type);
            fprintf(stderr, "`.\n");
            return false;
          }
        }
        break;
      default:
        UNREACHABLE("typeckeck");
    }
  }
 
  return true;
}

static inline bool codegen_expr(program_t* p, scope_t* scope, ast_expr_t* e) {
  switch(e->kind) {
      case EXPR_SYMBOL: {
        // TODO: optimize -> resolve symbol only once in typechecker, store symbol_t in expr
        symbol_t* symbol = resolve_symbol(scope, e->as.symbol, SYMB_VAR);
        if (!symbol) {
          fprintf(stderr, "[ERROR] Codegen\n No variable `%s` in current scope.\n", e->as.symbol);
          return false;
        }
        switch (symbol->storage) {
          case STO_EXTERN:
            TODO("codegen_expr - load extern symbol");
          case STO_LOCAL:
            da_append(&p->code, INST_LOAD);
            da_append(&p->code, symbol->addr);
            break;
          case STO_EXPORT:
          case STO_GLOBAL:
            da_append(&p->code, INST_LOADG);
            da_append(&p->code, symbol->addr);
            break;
          default: 
            UNREACHABLE("codegen_expr");
        }
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

        // NOTE: if operators are objects, check if implements interface and 
        // emit icall instruction to operator implementation
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
          case OP_SCOPE:
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
      case EXPR_ACCESS:
        if (e->as.access.op == OP_SCOPE) {
          TODO("codegen_expr: EXPR_ACCESS - scope access");
        } else if (e->as.access.op == OP_MEMB) {
          if (e->as.access.owner->type->kind == TYPE_STR) {
            TODO("codegen_expr: EXPR_ACCESS - string");
          } else if (e->as.access.owner->type->kind == TYPE_STRUCT) {
            TODO("codegen_expr: EXPR_ACCESS - struct");
          } else {
            fprintf(stderr, "[ERROR] Codegen\n Owner is not a built-in or a structure.\n");
            return false;
          }
        }
        return true;
      case EXPR_FUNCALL:
        // SysV-style calling convention
        for(int i = e->as.funcall.args.count - 1; i >= 0; i--) {
          if(!codegen_expr(p, scope, e->as.funcall.args.items[i])) return false;
        }
        ast_expr_t* callee = e->as.funcall.callee;

        if(callee->kind == EXPR_SYMBOL) {
          // TODO: resolve symbol once in typechecker
          symbol_t* symbol = resolve_symbol(scope, callee->as.symbol, SYMB_FUNC);
          if (!symbol) {
            fprintf(stderr, "[ERROR] Codegen\n No function `%s` in current scope.\n", e->as.symbol);
            return false;
          }

          da_append(&p->code, symbol->storage == STO_EXTERN ? INST_HOSTCALL : INST_CALL);
          if (symbol->addr_resolved)
            da_append(&p->code, symbol->addr);
          else {
            da_append(&p->patches, ((struct _patch){ .symbol = symbol, .addr = p->code.count }));
            da_append(&p->code, 0);
          }
        } else if (callee->kind == EXPR_ACCESS) {
          if (callee->as.access.op == OP_SCOPE) {
            TODO("codegen_expr: EXPR_FUNCALL - callee EXPR_ACCESS scope access");
          } else if (callee->as.access.op == OP_MEMB) {
            ast_expr_t* owner = callee->as.access.owner;
            if(!codegen_expr(p, scope, owner)) return false;
            if (owner->type->kind == TYPE_STR) {
              builtin_method_t *bm = resolve_builtin_method(&p->bms, owner->type, callee->as.access.field);
              if (!bm) {
                fprintf(stderr, "[ERROR] Codegen\n No method `%s` in built-in type ", callee->as.access.field);
                print_type(stderr, owner->type);
                fprintf(stderr, ".\n");
                return false;
              }
              da_append(&p->code, INST_HOSTCALL);
              da_append(&p->code, bm->addr);
            } else if (owner->type->kind == TYPE_STRUCT) {
              da_append(&p->code, INST_ICALL);
              TODO("codegen_expr: EXPR_FUNCALL - callee EXPR_ACCESS struct");
            } else {
              fprintf(stderr, "[ERROR] Codegen\n Owner is not a built-in or a structure.\n");
              return false;
            }
          }
        } else {
          TODO("codegen_expr: EXPR_FUNCALL - callee not a symbol");
        }
        break;
      case EXPR_SUBEXPR:
        return codegen_expr(p, scope, e->as.subexpr);
      case EXPR_ASSIGNMENT:
        if(!codegen_expr(p, scope, e->as.assign.rhs)) return false;

        if(e->as.assign.lhs->kind == EXPR_SYMBOL) {
          // TODO: optimize -> resolve symbol only once in typechecker, store symbol_t in expr
          symbol_t* symbol = resolve_symbol(scope, e->as.assign.lhs->as.symbol, SYMB_VAR);
          if (!symbol) {
            fprintf(stderr, "[ERROR] Codegen\n No variable `%s` in current scope.\n", e->as.symbol);
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
            default: 
              UNREACHABLE("codegen_expr");
          }
        } else {
          print_ast(stderr, (ast_node_t*)e->as.assign.lhs, 0);
          TODO("codegen_expr - assignment: lhs not a symbol");
        }
        break;
      default:
        UNREACHABLE("codegen_expr");
  }

  return true;
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
    case STMT_VAR_DEF:
      if (f->loc_vars >= MAX_LOC_VARS) {
        fprintf(stderr, "[ERROR] Codegen\n  Too many local variables in function %s", f->name);
        return false;
      }
      symbol_t* symb = s->as.var_def.symbol;
      set_symbol_address(symb, f->loc_vars++);
      if (s->as.var_def.initialized) {
        if(!codegen_expr(p, scope, s->as.var_def.init)) return false;
        da_append(&p->code, INST_STORE);
        da_append(&p->code, symb->addr);
      }
      return true;
    case STMT_IF:
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
        if(!codegen_stmt(p, s->as.if_else.scope, f, *stmt)) return false;

      if(s->as.if_else.else_body) {
        da_append(&p->code, INST_JMP);
        da_append(&p->code, 0);
        j_to_end_offset = p->code.count - 1;
        p->code.items[j_to_else_offset] = p->code.count - j_to_else_offset + 1;

        da_foreach(ast_stmt_t*, stmt, &s->as.if_else.else_body->stmts)
          if(!codegen_stmt(p, s->as.if_else.scope, f, *stmt)) return false;
      }

      p->code.items[j_to_end_offset] = p->code.count - j_to_end_offset + 1;
      return true;
    default: 
      UNREACHABLE("codegen_stmt");
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
    case TYPE_STR:    return &ffi_type_uint32; // handle
    case TYPE_ADDR:   return &ffi_type_pointer;
    case TYPE_ARRAY:  return &ffi_type_uint32; // handle
    case TYPE_STRUCT: return &ffi_type_uint32; // handle
    case TYPE_MODULE: return NULL;
    case TYPE_ALIAS:  return type_to_ffi_type(*t.as.alias.target);
    case TYPE_FUNC:   return &ffi_type_pointer; 
    case TYPES_COUNT:
    default:
      UNREACHABLE("type_to_ffi_type - Invalid type");
 }
}

static inline bool codegen_func_decl(program_t* p, ast_decl_t* d) {
  ast_sig_t* sig = d->as.fun.sig;

  if (d->symbol->storage == STO_EXTERN) {
    // TODO: add platform layer for malloc
    ffi_type **param_types = malloc(sizeof(*param_types) * sig->params.count);
    for(size_t i=0; i < sig->params.count; i++)
      param_types[i] = type_to_ffi_type(*sig->params.items[i]->type->resolved_type);

    ffi_type* ret = type_to_ffi_type(*sig->ret->resolved_type);

    ffi_cif cif = { 0 };
    ffi_status status = ffi_prep_cif(
      &cif,
      FFI_DEFAULT_ABI,
      sig->params.count,
      ret,
      param_types
    );

    if (status != FFI_OK) {
      fprintf(stderr, "[ERROR] Codegen\n  Could not initialize FFI CIF\n");
      return false;
    }

    set_symbol_address(d->symbol, p->externs.count);
    // TODO: name is leaked
    da_append(&p->externs, ((struct _extern){ cif, strdup(d->name), false }));
  }

  return true;
}

static inline bool codegen_func_def(program_t* p, ast_def_t* d) {
  frame_t f = { 0 };

  // patch symbol table
  set_symbol_address(d->decl->symbol, p->code.count);

  // store parameters in local variables
  f.loc_vars = d->decl->as.fun.sig->params.count;
  // SysV-style calling convention
  for (size_t i = 0; i < f.loc_vars; i++) {
    da_append(&p->code, INST_STORE);
    da_append(&p->code, i);
  }

  da_foreach(ast_stmt_t*, stmt, &d->body->stmts)
    if(!codegen_stmt(p, d->decl->as.fun.scope, &f, *stmt)) return false;

  if(da_last(&p->code) != INST_RET)
    da_append(&p->code, INST_RET);

  return true;
}

static inline int codegen(program_t* p, ast_root_t* root) {
  da_append(&p->code, INST_HALT);
  
  da_foreach(ast_decl_t*, d, &root->decls) {
    switch((*d)->kind) {
      case DECL_KIND_FUNC:
        if(!codegen_func_decl(p, *d)) return false;
        break;
      case DECL_KIND_VAR:
        break; // do nothing
      default:
        UNREACHABLE("codegen");
    }
  }
  da_foreach(ast_def_t*, d, &root->top_level) {
    switch((*d)->decl->kind) {
      case DECL_KIND_FUNC:
        if(!codegen_func_def(p, *d)) return false;
        break;
      case DECL_KIND_VAR:
        data_t val = { 0 };
        if ((*d)->decl->as.var.initialized) {
          switch((*d)->init->kind) {
            case EXPR_STRING:
              val.number.u = p->constants.count;
              da_append(&p->constants, ((constant_t){ .kind = DK_STR, .as.s = (*d)->init->as.s }));
              break;
            case EXPR_NUMBER:
              val.number.u = (*d)->init->as.number.u;
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
        da_append(&p->globals, val);
        break;
      default:
        UNREACHABLE("codegen");
    }
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
    da_foreach(symbol_t, s, &root->symbols) {
      if(i == s->addr && s->kind == SYMB_FUNC && s->storage != STO_EXTERN)
        fprintf(stream, "function <%s>:\n", s->name);
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
        da_foreach(symbol_t, s, &root->symbols) {
          if(op == s->addr && s->kind == SYMB_FUNC && s->storage != STO_EXTERN) {
            fprintf(stream, "    ->    ");
            fprintf(stream, "<%s>", s->name);
          }
          break;
        }
        fprintf(stream, "\n");
        break;
      }
      case INST_HOSTCALL: {
        uint32_t op = p->code.items[++i];
        fprintf(stream, "  %-10s 0x%08X        ", "HOSTCALL", op);
        da_foreach(symbol_t, s, &root->symbols) {
          if(op == s->addr && s->kind == SYMB_FUNC && s->storage == STO_EXTERN) {
            fprintf(stream, "    ->    ");
            fprintf(stream, "<extern::%s>", s->name);
          }
          break;
        }
        fprintf(stream, "\n");
        break;
      }
      case INST_ICALL: {
        uint32_t op = p->code.items[++i];
        fprintf(stream, "  %-10s 0x%08X        \n", "ICALL", op);
        // TODO: get name from struct when implemented
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


      case INST_COUNT:
      default:
        fprintf(stream, "  %-10s\n", "<INVALID>"); break;
    }
  }
}

static inline bool init_builtin_methods(program_t* p, typechecker_t* t) {
  builtin_method_t bms[] = 
  {
    {
      .owner = t->builtins.str,
      .name = "c_str",
      // TODO: use typed ptrs instead of generic addr
      .type = get_or_create_func_type_of(t, NULL, 0, t->builtins.addr),
    },
    {
      .owner = t->builtins.str,
      .name = "length",
      .type = get_or_create_func_type_of(t, NULL, 0, t->builtins.i32),
    },
  };
  for (size_t j=0; j < sizeof(bms)/sizeof(*bms); j++) {
    size_t n_params = bms[j].type->as.func.params.count + 2;
    // TODO: add platform layer for malloc
    ffi_type **param_types = malloc(sizeof(*param_types) * n_params);
    size_t i = 0;
    param_types[i++] = &ffi_type_pointer; // vm_t* vm 
    param_types[i++] = &ffi_type_uint32;  // obj_handle_t (uint32_t) self
    for(; i < n_params; i++)
      param_types[i] = type_to_ffi_type(*bms[j].type->as.func.params.items[i]);

    ffi_type* ret = type_to_ffi_type(*bms[j].type);

    ffi_cif cif = { 0 };
    ffi_status status = ffi_prep_cif(
      &cif,
      FFI_DEFAULT_ABI,
      n_params,
      ret,
      param_types
    );

    if (status != FFI_OK) {
      fprintf(stderr, "[ERROR] Codegen\n  Could not initialize FFI CIF\n");
      return false;
    }

    // TODO: name is leaked
    assert(bms[j].owner->name);
    size_t len = snprintf(NULL, 0, "%s__%s", bms[j].owner->name, bms[j].name);
    char* name = malloc(len + 1);
    snprintf(name, len + 1, "%s__%s", bms[j].owner->name, bms[j].name);
    bms[j].addr = p->externs.count;
    da_append(&p->externs, ((struct _extern){ cif, name, true }));

    da_append(&t->builtin_methods, bms[j]);
  }

  return true;
}

bool compile(program_t* program, const char* source) {
  tokenizer_t tok = { 0 };
  sb_append(&tok.source, source);

  parser_t parser = { 0 };
  typechecker_t tc = { 0 };

  printf("%s\n\n", source);

  if(tok_tokenize(&tok)) return false;

  // da_foreach(token_t, t, &tok.tokens) {
  //   tok_print(*t);
  // }

  ast_root_t* root = parse(&parser, &tok);
  if(!root) return false;

  // print_ast(stdout, (ast_node_t*)root, 0);

  init_typechecker(&tc);
  if(!init_builtin_methods(program, &tc)) return false;
  if(!typecheck(&tc, root)) return false;

  // print_symbol_table(stdout, root->scope);

  program->bms = tc.builtin_methods;
  if(!codegen(program, root)) return false;

  print_disass(stdout, program, root->scope);

  tok_destroy(&tok);
  parser_destroy(&parser);
  typechecker_destroy(&tc);

  return true;
}
