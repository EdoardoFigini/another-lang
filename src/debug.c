#include <stdio.h>
#include <inttypes.h>

#include "debug.h"

#include "types.h"
#include "vec.h"
#include "macros.h"

void __dbg_print_tok(FILE* stream, token_t tok) {
#ifdef NDEBUG
  (void)stream;
  (void)tok;
#else
  fprintf(stream, "%3zu:%3zu: ", tok.loc.line, tok.loc.col);
  if(tok.kind < 256) fprintf(stream, "%-20c", tok.kind);
  else {
    switch(tok.kind) {
      case TOK_EOF: fprintf(stream, "%-20s", "TOK_EOF"); break;
      case TOK_IDENT: fprintf(stream, "%-20s", "TOK_IDENT"); break;
      case TOK_ARROW: fprintf(stream, "%-20s", "TOK_ARROW"); break;
      case TOK_COLCOL: fprintf(stream, "%-20s", "TOK_COLCOL"); break;
      case TOK_EQEQ: fprintf(stream, "%-20s", "TOK_EQEQ"); break;
      case TOK_GEQ: fprintf(stream, "%-20s", "TOK_GEQ"); break;
      case TOK_LEQ: fprintf(stream, "%-20s", "TOK_LEQ"); break;
      case TOK_INTLIT: fprintf(stream, "%-20s", "TOK_INTLIT"); break;
      case TOK_REALLIT: fprintf(stream, "%-20s", "TOK_REALLIT"); break;
      case TOK_STRLIT: fprintf(stream, "%-20s", "TOK_STRLIT"); break;
      case TOK_RETURN: fprintf(stream, "%-20s", "TOK_RETURN"); break;
      case TOK_EXTERN: fprintf(stream, "%-20s", "TOK_EXTERN"); break;
      case TOK_EXPORT: fprintf(stream, "%-20s", "TOK_EXPORT"); break;
      case TOK_CONST: fprintf(stream, "%-20s", "TOK_CONST"); break;
      case TOK_IF: fprintf(stream, "%-20s", "TOK_IF"); break;
      case TOK_ELSE: fprintf(stream, "%-20s", "TOK_ELSE"); break;
      case TOK_WHILE: fprintf(stream, "%-20s", "TOK_WHILE"); break;
      case TOK_IMPL: fprintf(stream, "%-20s", "TOK_IMPL"); break;
      case TOK_INTERFACE: fprintf(stream, "%-20s", "TOK_INTERFACE"); break;
      case TOK_MOD: fprintf(stream, "%-20s", "TOK_MOD"); break;
      case TOK_STRUCT: fprintf(stream, "%-20s", "TOK_STRUCT"); break;
      case TOK_TYPE: fprintf(stream, "%-20s", "TOK_TYPE"); break;
      case TOK_NEW: fprintf(stream, "%-20s", "TOK_NEW"); break;
      case TOK_IMPORT: fprintf(stream, "%-20s", "TOK_IMPORT"); break;
      default: fprintf(stream, "%-20s", "<INVALID TOKEN>"); break;
    }
  }
  fprintf(stream, "%.*s\n", SLICE_FMT(tok.view));
#endif
}

void __dbg_print_ast(FILE* stream, ast_node_t* n, int level) {
#ifdef NDEBUG
  (void)stream;
  (void)n;
  (void)level;
#else
  if (!n) return;
  switch(n->ast_kind) {
    case AST_ROOT:
      ast_root_t* root = (ast_root_t*)n;
      fprintf(stream, "%*s%s\n", level, "","AST_ROOT");
      fprintf(stream, "%*s%s\n", level + 2, "","DEFINITIONS:");
      vec_foreach(d, &root->func_defs) {
        __dbg_print_ast(stream, (ast_node_t*)*d, level + 4);
      }
      vec_foreach(d, &root->var_defs) {
        __dbg_print_ast(stream, (ast_node_t*)*d, level + 4);
      }
      break;
    case AST_QN:
      ast_qn_t* qn = (ast_qn_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_QN");
      fprintf(stream, "%*sName: %s\n", level, "", qn->name);
      __dbg_print_ast(stream, (ast_node_t*)qn->next, level + 2);
      break;
    case AST_EXPR:
      ast_expr_t* expr = (ast_expr_t*)n;
      fprintf(stream, "%*s%s", level, "", "AST_EXPR");
      switch(expr->kind) {
        case EXPR_SYMBOL:
          fprintf(stream, " (%s)\n", "EXPR_SYMBOL");
          __dbg_print_ast(stream, (ast_node_t*)expr->as.symbol.name, level + 2);
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
          __dbg_print_ast(stream, (ast_node_t*)expr->as.binop.lhs, level + 2);
          fprintf(stream, "%*srhs:\n", level + 2, "");
          __dbg_print_ast(stream, (ast_node_t*)expr->as.binop.rhs, level + 2);
          break;
        case EXPR_UNOP:
          fprintf(stream, " (%s)\n", "EXPR_UNOP");
          fprintf(stream, "%*sop: %c\n", level + 2, "", expr->as.unop.op);
          fprintf(stream, "%*soperand:\n", level + 2, "");
          __dbg_print_ast(stream, (ast_node_t*)expr->as.unop.operand, level + 2);
          break;
        case EXPR_ACCESS:
          fprintf(stream, " (%s)\n", "EXPR_ACCESS");
          fprintf(stream, "%*sobject:\n", level + 2, "");
          __dbg_print_ast(stream, (ast_node_t*)expr->as.access.owner, level + 2);
          fprintf(stream, "%*sfield:\n", level + 2, "");
          fprintf(stream, "%*s%s\n", level + 2, "", expr->as.access.field);
          break;
        case EXPR_FUNCALL:
          fprintf(stream, " (%s)\n", "EXPR_FUNCALL");
          fprintf(stream, "%*scallee:\n", level + 2, "");
          __dbg_print_ast(stream, (ast_node_t*)expr->as.funcall.callee, level + 2);
          fprintf(stream, "%*sArgs:\n", level + 2, "");
          vec_foreach(a, &expr->as.funcall.args) {
            __dbg_print_ast(stream, (ast_node_t*)*a, level + 2);
          }
          break;
        case EXPR_SUBEXPR:
          fprintf(stream, " (%s)\n", "EXPR_SUBEXPR");
          __dbg_print_ast(stream, (ast_node_t*)expr->as.subexpr, level + 2);
          break;
        case EXPR_ASSIGNMENT:
          fprintf(stream, " (%s)\n", "EXPR_ASSIGNMENT");
          fprintf(stream, "%*slhs:\n", level + 2, "");
          __dbg_print_ast(stream, (ast_node_t*)expr->as.assign.lhs, level + 2);
          fprintf(stream, "%*srhs:\n", level + 2, "");
          __dbg_print_ast(stream, (ast_node_t*)expr->as.assign.rhs, level + 2);
          break;
        case EXPR_MKOBJ:
          fprintf(stream, " (%s)\n", "EXPR_MKOBJ");
          fprintf(stream, "%*sfields:\n", level + 2, "");
          vec_foreach(f, &expr->as.mkobj.fields) {
            fprintf(stream, "%*s%s:\n", level + 2, "", f->field);
            __dbg_print_ast(stream, (ast_node_t*)f->value, level + 2);
          }
          break;
        case EXPR_MKARR:
          fprintf(stream, " (%s)\n", "EXPR_MKARR");
          TODO("EXPR_MKARR");
          break;
        case EXPR_INDEX:
          fprintf(stream, " (%s)\n", "EXPR_INDEX");
          TODO("EXPR_INDEX");
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
          __dbg_print_ast(stream, (ast_node_t*)stmt->as.retval, level + 2);
          break;
        case STMT_EXPR:
          __dbg_print_ast(stream, (ast_node_t*)stmt->as.expression, level + 2);
          break;
        case STMT_VAR_DEF:
          fprintf(stream, "%*s%s\n", level, "", "STMT_VAR_DEF");
          __dbg_print_ast(stream, (ast_node_t*)stmt->as.var_def, level + 2);
          break;
        case STMT_IF:
          TODO("__dbg_print_ast: STMT_IF");
        case STMT_WHILE:
          TODO("__dbg_print_ast: STMT_WHILE");
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
        __dbg_print_ast(stream, (ast_node_t*)def->init, level + 2);
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
      __dbg_print_ast(stream, (ast_node_t*)def->sig, level + 2);
      if(def->body)
        __dbg_print_ast(stream, (ast_node_t*)def->body, level + 2);
      break;
    }
    case AST_TYPE:
      fprintf(stream, "%*s%s\n", level, "", "AST_TYPE");
      fprintf(stream, "%*sName:", level + 2, "");
      __dbg_print_ast(stream, (ast_node_t*)((ast_type_t*)n)->name, level + 2);
      break;
    case AST_BODY:
      fprintf(stream, "%*s%s\n", level, "", "AST_BODY");
      vec_foreach(stmt, &((ast_body_t*)n)->stmts) {
        __dbg_print_ast(stream, (ast_node_t*)*stmt, level + 2);
      }
      break;
    case AST_PARAM:
      ast_param_t* param = (ast_param_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_PARAM");
      fprintf(stream, "%*sName: %s\n", level + 2, "", param->name);
      fprintf(stream, "%*sType:\n", level + 2, "");
      __dbg_print_ast(stream, (ast_node_t*)param->type, level + 2);
      break;
    case AST_SIG:
      ast_sig_t* sig = (ast_sig_t*)n;
      fprintf(stream, "%*s%s\n", level, "", "AST_SIG");
      fprintf(stream, "%*sParams:\n", level + 2, "");
      vec_foreach(p, &sig->params) {
        __dbg_print_ast(stream, (ast_node_t*)*p, level + 2);
      }
      fprintf(stream, "%*sRet type:\n", level + 2, "");
      __dbg_print_ast(stream, (ast_node_t*)sig->ret, level + 2);
      break;
    case AST_IMPL:
      TODO("__dbg_print_ast: AST_IMPL");
    case AST_IFACE:
      TODO("__dbg_print_ast: AST_IFACE");
    case AST_ATTR_LIST:
      TODO("__dbg_print_ast: AST_ATTR_LIST");
    case AST_STRUCT:
      TODO("__dbg_print_ast: AST_STRUCT");
    case AST_MOD:
      TODO("__dbg_print_ast: AST_MOD");
    case AST_TYPEDEF:
      TODO("__dbg_print_ast: AST_TYPEDEF");
    default:
      UNREACHABLE("__dbg_print_ast %d", n->ast_kind);
  }
#endif
}

static inline void __dbg_print_symbol_table_entries(FILE* stream, scope_t* scope) {
#ifdef NDEBUG
  (void)stream;
  (void)scope;
#else
  if(!scope) return;
  vec_foreach(s, &scope->symbols) {
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
    fprintf(stream, "0x%08X %s ", (*s)->addr, (*s)->addr_resolved ? "    " : "(NR)");
    __dbg_print_type(stream, (*s)->type);
    fprintf(stream, "\n");
  }
  __dbg_print_symbol_table_entries(stream, scope->parent);
#endif
}

void __dbg_print_symbol_table(FILE* stream, scope_t* scope) {
#ifdef NDEBUG
  (void)stream;
  (void)scope;
#else
  if(!scope) return;
  fprintf(stream, "%s symbol Table:\n", scope->name);
  fprintf(stream, "%-30s %-30s %-10s %-15s %s\n", "name", "scope", "storage", "addr", "type");
  __dbg_print_symbol_table_entries(stream, scope);
#endif
}

void __dbg_print_data(FILE* stream, constant_t c) {
#ifdef NDEBUG
  (void)stream;
  (void)c;
#else
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
      UNREACHABLE("__dbg_print_data");
  }
#endif
}

void __dbg_print_disass_inst(FILE* stream, instruction_t* inst) {
#ifdef NDEBUG
  (void)stream;
  (void)inst;
#else
    switch(*inst) {
      case INST_NOP:
        fprintf(stream, "  %-10s\n", "NOP"); break;
      case INST_PUSH:
        fprintf(stream, "  %-10s 0x%08X\n", "PUSH", *(++inst)); break;
      // case INST_PUSHL:
      //   fprintf(stream, "  %-10s 0x%016lX\n", "PUSHL", ((uint64_t)*(++inst) << 32) | *(++inst)); break;
      case INST_POP:
        fprintf(stream, "  %-10s\n", "POP"); break;
      case INST_DUP:
        fprintf(stream, "  %-10s\n", "DUP"); break;
      case INST_SWAP:
        fprintf(stream, "  %-10s\n", "SWAP"); break;
      case INST_LOAD:
        fprintf(stream, "  %-10s 0x%08X\n", "LOAD", *(++inst)); break;
      case INST_LOADG:
        fprintf(stream, "  %-10s 0x%08X\n", "LOADG", *(++inst)); break;
      case INST_LOADC: {
        fprintf(stream, "  %-10s 0x%08X\n", "LOADC", *(++inst)); break;
      }
      case INST_LOADF:
        fprintf(stream, "  %-10s 0x%08X\n", "LOADF", *(++inst)); break;
      case INST_LOADI:
        fprintf(stream, "  %-10s\n", "LOADI"); break;
      case INST_STORE:
        fprintf(stream, "  %-10s 0x%08X\n", "STORE", *(++inst)); break;
      case INST_STOREG:
        fprintf(stream, "  %-10s 0x%08X\n", "STOREG", *(++inst)); break;
      case INST_STOREF:
        fprintf(stream, "  %-10s 0x%08X\n", "STOREF", *(++inst)); break;
      case INST_STOREI:
        fprintf(stream, "  %-10s\n", "STOREI"); break;
      case INST_JMP: {
        uint32_t op = *(++inst);
        fprintf(stream, "  %-10s 0x%08X        \n", "JMP", op);
        break;
      }
      case INST_JNZ: {
        uint32_t op = *(++inst);
        fprintf(stream, "  %-10s 0x%08X        \n", "JNZ", op);
        break;
      }
      case INST_JZ: {
        uint32_t op = *(++inst);
        fprintf(stream, "  %-10s 0x%08X        \n", "JZ", op);
        break;
      }
      case INST_CALL: {
        uint32_t op = *(++inst);
        fprintf(stream, "  %-10s 0x%08X\n", "CALL", op); break;
      }
      case INST_HOSTCALL: {
        uint32_t op = *(++inst);
        fprintf(stream, "  %-10s 0x%08X\n", "HOSTCALL", op); break;
      }
      case INST_RET:
        fprintf(stream, "  %-10s\n", "RET"); break;

      case INST_FTOI:
        fprintf(stream, "  %-10s\n", "FTOI"); break;
      case INST_ITOF:
        fprintf(stream, "  %-10s\n", "ITOF"); break;

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
       case INST_LNOT:
        fprintf(stream, "  %-10s\n", "LNOT"); break;
       case INST_HALT:
        fprintf(stream, "  %-10s\n", "HALT"); break;

      case INST_MKOBJ:
        fprintf(stream, "  %-10s 0x%08X\n", "MKOBJ", *(++inst)); break;

      case INST_COUNT:
      default:
        fprintf(stream, "  %-10s\n", "<INVALID>"); break;
    }
#endif
}

void __dbg_print_disass(FILE* stream, program_t* p, scope_t* root) {
#ifdef NDEBUG
  (void)stream;
  (void)p;
  (void)root;
#else
  for (size_t i = 0; i < p->code.count; i++) {
    vec_foreach(s, &root->symbols) {
      if(i == (*s)->addr && (*s)->kind == SYMB_FUNC && (*s)->storage != STO_EXTERN)
        fprintf(stream, "function <%s>:\n", (*s)->name);
    }
    fprintf(stream, "  0x%08" PRIx64, i);
    switch(vec_get(&p->code, i)) {
      case INST_NOP:
        fprintf(stream, "  %-10s\n", "NOP"); break;
      case INST_PUSH:
        fprintf(stream, "  %-10s 0x%08X\n", "PUSH", vec_get(&p->code, ++i)); break;
      // case INST_PUSHL:
      //   fprintf(stream, "  %-10s 0x%016lX\n", "PUSHL", ((uint64_t)vec_get(&p->code, ++i) << 32) | vec_get(&p->code, ++i)); break;
      case INST_POP:
        fprintf(stream, "  %-10s\n", "POP"); break;
      case INST_DUP:
        fprintf(stream, "  %-10s\n", "DUP"); break;
      case INST_SWAP:
        fprintf(stream, "  %-10s\n", "SWAP"); break;
      case INST_LOAD:
        fprintf(stream, "  %-10s 0x%08X\n", "LOAD", vec_get(&p->code, ++i)); break;
      case INST_LOADG:
        fprintf(stream, "  %-10s 0x%08X\n", "LOADG", vec_get(&p->code, ++i)); break;
      case INST_LOADC: {
        uint32_t op = vec_get(&p->code, ++i);
        fprintf(stream, "  %-10s 0x%08X        ", "LOADC", op);
        fprintf(stream, "    ->    ");
        __dbg_print_data(stream, vec_get(&p->constants, op));
        fprintf(stream, "\n");
        break;
      }
      case INST_LOADF:
        fprintf(stream, "  %-10s 0x%08X\n", "LOADF", vec_get(&p->code, ++i)); break;
      case INST_LOADI:
        fprintf(stream, "  %-10s\n", "LOADI"); break;
      case INST_STORE:
        fprintf(stream, "  %-10s 0x%08X\n", "STORE", vec_get(&p->code, ++i)); break;
      case INST_STOREG:
        fprintf(stream, "  %-10s 0x%08X\n", "STOREG", vec_get(&p->code, ++i)); break;
      case INST_STOREF:
        fprintf(stream, "  %-10s 0x%08X\n", "STOREF", vec_get(&p->code, ++i)); break;
      case INST_STOREI:
        fprintf(stream, "  %-10s\n", "STOREI"); break;
      case INST_JMP: {
        uint32_t op = vec_get(&p->code, ++i);
        fprintf(stream, "  %-10s 0x%08X        \n", "JMP", op);
        break;
      }
      case INST_JNZ: {
        uint32_t op = vec_get(&p->code, ++i);
        fprintf(stream, "  %-10s 0x%08X        \n", "JNZ", op);
        break;
      }
      case INST_JZ: {
        uint32_t op = vec_get(&p->code, ++i);
        fprintf(stream, "  %-10s 0x%08X        \n", "JZ", op);
        break;
      }
      case INST_CALL: {
        uint32_t op = vec_get(&p->code, ++i);
        fprintf(stream, "  %-10s 0x%08X        ", "CALL", op);
        vec_foreach(s, &root->symbols) {
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
        uint32_t op = vec_get(&p->code, ++i);
        fprintf(stream, "  %-10s 0x%08X        ", "HOSTCALL", op);
        vec_foreach(s, &root->symbols) {
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

      case INST_FTOI:
        fprintf(stream, "  %-10s\n", "FTOI"); break;
      case INST_ITOF:
        fprintf(stream, "  %-10s\n", "ITOF"); break;

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
       case INST_LNOT:
        fprintf(stream, "  %-10s\n", "LNOT"); break;
       case INST_HALT:
        fprintf(stream, "  %-10s\n", "HALT"); break;

      case INST_MKOBJ:
        fprintf(stream, "  %-10s 0x%08X\n", "MKOBJ", vec_get(&p->code, ++i)); break;

      case INST_COUNT:
      default:
        fprintf(stream, "  %-10s\n", "<INVALID>"); break;
    }
  }
#endif
}

void render_type(sb_t* sb, const type_t* t);
void __dbg_print_type(FILE* stream, const type_t* t) {
#ifdef NDEBUG
  (void)stream;
  (void)t;
#else
  sb_t sb = { 0 };

  render_type(&sb, t);
  fprintf(stream, "%.*s", SB_FMT(sb));
#endif
}

void __dbg_print_stack(FILE* stream, vm_t* vm) {
#ifdef NDEBUG
  (void)stream;
  (void)vm;
#else
  fprintf(stream, "+------------+\n");
  for(int i = MIN(vm->max_stack, (int)(vm->sp - vm->active_task->stack)) - 1; i >= 0; i--) {
    fprintf(stream, "| 0x%08X |\n", vm->active_task->stack[i]);
  }
  fprintf(stream, "+------------+\n\n");
#endif
}
