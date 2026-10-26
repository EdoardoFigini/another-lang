#ifndef PARSER_H
#define PARSER_H

#include "efh/arena.h"
#include "efh/vec.h"

#include "common.h"

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

typedef struct {
  AST_DEFAULT_FIELDS;
} ast_root_t;

typedef struct {
  arena_t arena;
  ast_node_t* root;
} ast_t;

typedef struct {
  ast_t* ast;
} parser_t;

#endif // PARSER_H
