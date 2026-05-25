#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

#include "types.h"

void __dbg_print_tok(FILE* stream, token_t tok);
void __dbg_print_type(FILE* stream, const type_t* t);
void __dbg_print_ast(FILE* stream, ast_node_t* n, int level);
void __dbg_print_symbol_table(FILE* stream, scope_t* root);
void __dbg_print_symbol_table(FILE* stream, scope_t* scope);
void __dbg_print_data(FILE* stream, constant_t c);
void __dbg_print_disass(FILE* stream, program_t* p, scope_t* root);
void __dbg_print_disass_inst(FILE* stream, instruction_t* inst);
void __dbg_print_stack(FILE* stream, vm_t* vm);

#endif
