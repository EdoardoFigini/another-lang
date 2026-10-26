#include "efh/ss.h"
#include "efh/arena.h"

#include "parser.h"
#include "pipeline.h"
#include "tokenizer.h"

static ast_node_t* make_node(ast_t* tree, ast_node_kind_t kind, loc_t loc) {
  return arena_push(&tree->arena, (ast_node_t){ .ast_kind = kind, .loc = loc });
}

void parse_mod(pipeline_t* p, ss_t src, const char* origin, ast_t* ast) {
  tokenizer_t t = tok_init(src, origin, ast->arena); 

  ast_root_t* root = make_node(ast, AST_ROOT, t.loc); 
}
