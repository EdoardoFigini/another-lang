#include <assert.h>
#include <stdio.h>

#include "tokenizer.h"
#include "report.h"

#include "efh/ss.h"
#include "efh/arena.h"

// clang tests\tokenizer.c src\tokenizer.c src\report.c -Isrc -Ithird_party -o tok_test.exe && tok_test.exe
// TODO: integrate in nob.c build
int main() {
  arena_t arena = { 0 }; 

  {
    tokenizer_t t = tok_init(SS("print(\"Hello, World!\")"), NULL, &arena);

    // token_t tok = { 0 }; 
    // do {
    //   tok = tok_next(&t);
    //   fdiagf(stdout, DIAG_DEBUG, tok.loc, "%s - " PRI_SS, tok_kind_str(&arena, tok.kind), SS_FMT(tok.view));
    // } while (tok.kind != TOK_EOF && tok.kind != TOK_INVALID);
  }

  {
    tokenizer_t t = tok_init(SS("print(\"Hello, World!\")"), NULL, &arena);
    assert(tok_next(&t).kind == TOK_IDENT);
    assert(tok_next(&t).kind == '(');
    assert(tok_next(&t).kind == TOK_STRLIT);
    assert(tok_next(&t).kind == ')');
    assert(tok_next(&t).kind == TOK_EOF);
  }


  {
    tokenizer_t t = tok_init(SS("55"), NULL, &arena);
    assert(tok_next(&t).kind == TOK_INTLIT);
    assert(tok_next(&t).kind == TOK_EOF);
  }

  {
    tokenizer_t t = tok_init(SS("55."), NULL, &arena);
    assert(tok_next(&t).kind == TOK_REALLIT);
    assert(tok_next(&t).kind == TOK_EOF);
  }

  {
    tokenizer_t t = tok_init(SS("55f"), NULL, &arena);
    assert(tok_next(&t).kind == TOK_REALLIT);
    assert(tok_next(&t).kind == TOK_EOF);
  }

  {
    tokenizer_t t = tok_init(SS("hello"), NULL, &arena);
    assert(tok_next(&t).kind == TOK_IDENT);
    assert(tok_next(&t).kind == TOK_EOF);
  }

  {
    tokenizer_t t = tok_init(SS("->"), NULL, &arena);
    assert(tok_next(&t).kind == TOK_ARROW);
    assert(tok_next(&t).kind == TOK_EOF);
  }

  {
    tokenizer_t t = tok_init(SS("main :: () -> void { }"), NULL, &arena);
    assert(tok_next(&t).kind == TOK_IDENT);
    assert(tok_next(&t).kind == ':');
    assert(tok_next(&t).kind == ':');
    assert(tok_next(&t).kind == '(');
    assert(tok_next(&t).kind == ')');
    assert(tok_next(&t).kind == TOK_ARROW);
    assert(tok_next(&t).kind == TOK_IDENT);
    assert(tok_next(&t).kind == '{');
    assert(tok_next(&t).kind == '}');
    assert(tok_next(&t).kind == TOK_EOF);
  }

  printf("Tests passed\n");
}

#define SS_IMPLEMENTATION
#include "efh/ss.h"
#define ARENA_IMPLEMENTATION
#include "efh/arena.h"
#define SB_IMPLEMENTATION
#include "efh/sb.h"
