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
  "extern puts: (fmt: str) -> void;";

typedef enum {
  TOK_EOF = 256,
  TOK_IDENT,
  TOK_OPAR,
  TOK_CPAR,
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

  size_t current;
} tokenizer_t;

const char* tok_str(tokenizer_t* t, token_t* tok) {
  if(tok->kind < 256) return arena_sprintf(&t->arena, "'%c'", tok->kind);
  switch(tok->kind) {
    case TOK_EOF: return "TOK_EOF";
    case TOK_IDENT: return "TOK_IDENT";
    case TOK_OPAR: return "TOK_OPAR";
    case TOK_CPAR: return "TOK_CPAR";
    case TOK_ARROW: return "TOK_ARROW";
    case TOK_EXTERN: return "TOK_EXTERN";
    default: return "<INVALID TOKEN>";
  }
};

int tok_tokenize(tokenizer_t* t) {
  token_t tok = { 0 };

  char* p = t->source.items;
  while(*p) {
    if(isspace(*p)) { p++; continue; }

    switch(*p) {
      case '(':
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
              p += kw_len;
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

    p += tok.len;
    da_append(&t->tokens, tok);
  }

  return 0;
}

void tok_destroy(tokenizer_t* t) {
  sb_free(&t->source);
  da_free(t->tokens);
  arena_free(&t->arena);
}

int main() {
  tokenizer_t tok = { 0 };
  sb_append(&tok.source, source);

  tok_tokenize(&tok);

  da_foreach(token_t, t, &tok.tokens) {
    printf("%-20s: `%.*s`\n", tok_str(&tok, t), t->len, t->start);
  }

  return 0;
}
