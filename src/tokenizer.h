#ifndef TOK_H
#define TOK_H

#include <stdint.h>

#include "efh/ss.h"
#include "efh/arena.h"

#include "common.h"

typedef enum {
  TOK_INVALID = 0,
  TOK_EOF = 256,
  TOK_IDENT,
  TOK_ARROW,
  TOK_EQEQ,
  TOK_GEQ,
  TOK_LEQ,
  TOK_MINEQS,
  TOK_INTLIT,
  TOK_REALLIT,
  TOK_STRLIT,
  TOK_RETURN,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_MOD,
  TOK_STRUCT,
  TOK_NEW,
  TOK_IMPORT,
} tok_kind_t;

typedef enum {
  KW_RETURN    = TOK_RETURN,
  KW_IF        = TOK_IF,
  KW_ELSE      = TOK_ELSE,
  KW_WHILE     = TOK_WHILE,
  KW_MOD       = TOK_MOD,
  KW_STRUCT    = TOK_STRUCT,
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
  tok_kind_t kind;
  lit_type_info_flags_t type_info;
  union {
    const char* s;
    double      r;
    uint64_t    u;
    int64_t     i;
  } as;
  ss_t view;
  loc_t loc;
} token_t;

typedef struct {
  loc_t loc;
  ss_t src;
  arena_t* arena; // ptr to ast arena
} tokenizer_t;

tokenizer_t tok_init(ss_t src, const char* origin, arena_t* arena);
token_t tok_next(tokenizer_t* t);
token_t tok_peek(tokenizer_t* t);
token_t tok_peek_n(tokenizer_t* t, size_t n);
const char* tok_kind_str(arena_t* arena, tok_kind_t k);

#endif // TOK_H
