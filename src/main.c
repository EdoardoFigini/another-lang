#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

#define SB_IMPLEMENTATION
#include "sb.h"
#define DA_IMPLEMENTATION
#include "da.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"

const char* source = 
  // "require raylib"
  "extern puts: (s: str) -> void;";

typedef enum {
  TOK_EOF = 256,
  TOK_IDENT,
  TOK_ARROW,
  TOK_EXTERN
} tok_kind_t;

const char* tok_keywords[] = {
  [TOK_EXTERN] = "extern",
};

typedef struct {
  tok_kind_t kind;
  const char* start;
  int len;
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
      case TOK_EXTERN: printf("%-20s", "TOK_EXTERN"); break;
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
      case TOK_EXTERN: return "extern";
      default: return "<INVALID TOKEN>";
    }
  }
}

int tok_tokenize(tokenizer_t* t) {
  token_t tok = { 0 };
  size_t line = 1;
  size_t col  = 1;

  char* p = t->source.items;
  while(*p) {
    if(*p == '\n') {
      line++;
      col = 1;
      continue;
    }
    if(isspace(*p)) { p++; col++; continue; }

    switch(*p) {
      case '(':
      case ',':
      case ')': 
      case ';': 
      case ':': 
        tok = (token_t){ .kind = *p, .start = p, .len = 1 }; 
        break;
      case '-':
        switch(*(p + 1)) {
          case '>': tok = (token_t){ .kind = TOK_ARROW, .start = p, .len = 2 }; break;
          // case '=': tok = (token_t){ .kind = TOK_DECASS }; break;
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
        } else {
          fprintf(stderr, "[ERROR]: Unrecognized token near %.*s\n", 20, p);
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

typedef struct {
  arena_t arena;
  tokenarr_t tokens;
  size_t current;
} parser_t;

typedef enum {
  SPEC_NONE = 0,
  SPEC_EXTERN = 1 << 0,
} spec_flags_t;

static inline token_t get_tok(parser_t* p) {
  if (p->current >= p->tokens.count) {
    fprintf(stderr, "[FATAL]: No more tokens.");
    abort();
  }
  return p->tokens.items[p->current];
}

static inline int peek(parser_t* p, tok_kind_t kind) {
  return get_tok(p).kind == kind; 
}

static inline int consume(parser_t* p, tok_kind_t kind) {
  token_t t = get_tok(p); 
  printf("consuming %s\n", tok_kind_str(&p->arena, t.kind));
  if (t.kind != kind) {
    fprintf(stderr, "[ERROR]: %zu:%zu: Expected %s, found %s\n", t.line, t.col, tok_kind_str(&p->arena, kind), tok_kind_str(&p->arena, t.kind));
    return 0;
  }
  return 1;
}

static inline void next(parser_t* p) {
  p->current++;
}

// specifiers = ( specifier )*
static inline spec_flags_t parse_specifiers(parser_t* p) {
  spec_flags_t flags = SPEC_NONE;

  for (;;) {
    spec_flags_t cur_flag = SPEC_NONE;

    token_t t = get_tok(p);

    switch(t.kind) {
      case TOK_EXTERN: cur_flag = SPEC_EXTERN; break;
      case TOK_EOF:
      case TOK_IDENT:
      case TOK_ARROW:
      default: return flags;
    }

    if (flags & cur_flag)
      fprintf(stderr, "[WARN]: %zu:%zu: Duplicate specifier `%.*s`, will be considered as one.\n", t.line, t.col, t.len, t.start);

    flags |= cur_flag;

    next(p);
  }

  return flags;
}

typedef enum {
  DECL_KIND_VAR,
  DECL_KIND_FUNC,
} ast_decl_kind;

typedef struct {
  const char* name;
} ast_type_t;

typedef struct {
  const char* name;
  ast_type_t type;
} ast_param_t;

typedef struct {
  struct {
    ast_param_t** items;
    size_t count;
    size_t capacity;
  } params;
  ast_type_t ret;
} ast_sig_t;

typedef struct {
  const char* name;
  spec_flags_t flags;
  ast_decl_kind kind; 
  union {
    ast_sig_t* sig;
    ast_type_t* type;
  };
} ast_decl_t;

// param = ident ':' ident
static inline ast_param_t* parse_param(parser_t* p) {
  ast_param_t* n = arena_alloc(&p->arena, sizeof(*n));

  if(!consume(p, TOK_IDENT)) return NULL;
  n->name = arena_strdup(&p->arena, get_tok(p).as.s);
  next(p);

  if(!consume(p, ':')) return NULL;
  next(p);

  if(!consume(p, TOK_IDENT)) return NULL;
  n->type.name = arena_strdup(&p->arena, get_tok(p).as.s);
  next(p);

  return n;
}

// sig = '(' [ param ( ',' param )+ ] ')' -> ident
static inline ast_sig_t* parse_signature(parser_t* p) {
  ast_sig_t* n = arena_alloc(&p->arena, sizeof(*n));

  if(!consume(p, '(')) return NULL;
  next(p);

  ast_param_t* param = parse_param(p);
  if(!param) return NULL;

  da_append(&n->params, param);

  if (peek(p, ')')) {
    if(!consume(p, ')')) return NULL;
    next(p);
  } else {
    while(peek(p, ',')) {
      if (!consume(p, ',')) return NULL;
      next(p);

      param = parse_param(p);
      if(!param) return NULL;

      da_append(&n->params, param);
    } 

    if(!consume(p, ')')) return NULL;
    next(p);
  }

  if(!consume(p, TOK_ARROW)) return NULL;
  next(p);
  if(!consume(p, TOK_IDENT)) return NULL;
  n->ret.name = arena_strdup(&p->arena, get_tok(p).as.s);
  next(p);

  return n;
}

// func_decl = sig
static inline ast_decl_t* parse_func_decl(parser_t* p, const char* name, spec_flags_t flags) {
  ast_decl_t *n = arena_alloc(&p->arena, sizeof(*n));
  n->flags = flags;
  n->name = arena_strdup(&p->arena, name);
  n->kind = DECL_KIND_FUNC;

  n->sig = parse_signature(p);

  if(!consume(p, ';')) return NULL;
  next(p);

  return n;
}

// tl_decl = specifiers ident ':' ( func_decl | var_decl )
static inline ast_decl_t* parse_top_level_declaration(parser_t* p) {
  spec_flags_t flags = parse_specifiers(p);
  
  if(!consume(p, TOK_IDENT)) return NULL;
  const char* name = get_tok(p).as.s; 
  next(p);
  if(!consume(p, ':')) return NULL;
  next(p);
  
  if(peek(p, '(')) return parse_func_decl(p, name, flags);
  else             return NULL;
}

typedef struct {
  ast_decl_t* top_level;
} ast_root_t;

static inline ast_root_t* parse(parser_t* p, tokenizer_t* t) {
  p->tokens = t->tokens;
  p->current = 0;

  ast_root_t *n = arena_alloc(&p->arena, sizeof(*n));

  n->top_level = parse_top_level_declaration(p);
  if (!n->top_level) return NULL;

  return n;
}

int main() {
  tokenizer_t tok = { 0 };
  sb_append(&tok.source, source);

  parser_t parser = { 0 };

  tok_tokenize(&tok);

  da_foreach(token_t, t, &tok.tokens) {
    tok_print(*t);
  }

  ast_root_t* root = parse(&parser, &tok);
  if(!root) return 1;

  return 0;
}
