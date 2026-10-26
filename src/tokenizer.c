#include "efh/ss.h"
#include "efh/sb.h"
#include "efh/arena.h"

#include "tokenizer.h"
#include "report.h"
#include "parser.h"

#define COMPTIME_DIRECTIVE_PREFIX "@"
#define COMMENT_PREFIX "//"

const char* tok_keywords[] = {
  [KW_RETURN]    = "return",
  [KW_IF]        = "if",
  [KW_ELSE]      = "else",
  [KW_WHILE]     = "while",
  [KW_MOD]       = "mod",
  [KW_STRUCT]    = "struct",
  [KW_NEW]       = "new",
};

const char* tok_ctds[] = {
  [CT_IMPORT]   = COMPTIME_DIRECTIVE_PREFIX "import",
};

const char* tok_kind_str(arena_t* arena, tok_kind_t k) {
  if(k < 256) return arena_sprintf(arena, "`%c`", k);
  else {
    switch(k) {
      case TOK_EOF: return "EOF";
      case TOK_IDENT: return "identifier";
      case TOK_ARROW: return "->";
      case TOK_EQEQ: return "==";
      case TOK_GEQ: return ">=";
      case TOK_LEQ: return "<=";
      case TOK_INTLIT: return "integer literal";
      case TOK_REALLIT: return "real literal";
      case TOK_STRLIT: return "string literal";
      case TOK_RETURN:
      case TOK_IF:
      case TOK_ELSE:
      case TOK_WHILE:
      case TOK_MOD:
      case TOK_STRUCT:
      case TOK_NEW:
        return tok_keywords[k];
      case TOK_IMPORT:
        return tok_ctds[k];
      default: return "<INVALID TOKEN>";
    }
  }
}

static token_t make_token(tokenizer_t* t, tok_kind_t k, size_t len) {
  token_t tok = (token_t) {
    .kind = k,
    .loc = t->loc,
    .view = ss_sub(t->src, 0, len),
  };

  ss_advance_by_in_place(&t->src, len);

  return tok;
}

#define INVALID_TOKEN(l) (token_t) { .kind = TOK_INVALID, .loc = (l) }

static inline token_t tok_string(tokenizer_t* t) {
  int escape = 0;

  sb_t sb = { 0 };
  ss_t og = t->src;

  ss_advance_by_in_place(&t->src, 1); // skip opening "

  while(t->src.len > 0) {
    if (escape) {
      switch(*t->src.data) {
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
          fdiagf(stderr, DIAG_ERROR, t->loc, "Unknown escape sequence");
          return INVALID_TOKEN(t->loc);
      }

      escape = 0;
    } else {
      if      (*t->src.data == '\"') break;
      else if (*t->src.data == '\\') escape = 1;
      else if (*t->src.data == '\n') {
        fdiagf(stderr, DIAG_ERROR, t->loc, "Missing closing `\"`");
        return INVALID_TOKEN(t->loc);
      }
      else
        sb_appendf(&sb, "%c", *t->src.data);
    }
    ss_advance_by_in_place(&t->src, 1);
  }

  ss_advance_by_in_place(&t->src, 1); // skip closing "

  token_t tok = (token_t) {
    .as.s = arena_sprintf(t->arena, PRI_SB, SB_FMT(sb)),
    .loc = t->loc,
    .view = ss_from_extents(og.data, t->src.data),
    .kind = TOK_STRLIT,
  };

  sb_free(&sb);

  return tok;
}

// TODO: support exp notation for reals 
static inline token_t tok_num_literal(tokenizer_t* t) {
  int base = 10;
  int64_t integer = 0;
  double real = 0.0;

  ss_t og = t->src;

  lit_type_info_flags_t type_info = 0;

  if (*t->src.data == '0') {
    switch (*(t->src.data + 1)) {
      case 'x': base = 16; ss_advance_by_in_place(&t->src, 2); break;
      case 'b': base = 2;  ss_advance_by_in_place(&t->src, 2); break;
      case 'o': base = 8;  ss_advance_by_in_place(&t->src, 2); break;
      default: break;
    }
  }

  while(t->src.len > 0) {
    int digit = 0;
    
    if      (*t->src.data >= '0' && *t->src.data <= '9')
      digit = *t->src.data - '0';
    
    else if ( base == 16 && *t->src.data >= 'A' && *t->src.data <= 'F')
      digit = *t->src.data - 'A' + 10;

    else if ( base == 16 && *t->src.data >= 'a' && *t->src.data <= 'f')
      digit = *t->src.data - 'a' + 10;
    
    else if (*t->src.data == '_')
      continue;
    
    else if ( *t->src.data == 'U' || *t->src.data == 'u' || *t->src.data == 'L' || *t->src.data == 'l' )
      break;
    
    else if (base == 10 && (*t->src.data == 'F' || *t->src.data == 'f' || *t->src.data == '.')) {
      type_info |= TI_REAL;
      break;
    }
    
    else break;

    integer *= base;
    integer += digit;

    ss_advance_by_in_place(&t->src, 1);
  }

  if (type_info & TI_REAL) {
    real = (double)integer;

    double power = 10.0;

    ss_advance_by_in_place(&t->src, 1); // skip ./f/F

    ss_t decimal = ss_tok_while(&t->src, isdigit);

    for (size_t i=0; i < decimal.len; i++) {
      real += (double)(decimal.data[i] - '0') / power;
      power *= 10.0;
    }
  }

  switch (*t->src.data) {
    case 'U':
    case 'u': type_info |= TI_UNSIGNED; ss_advance_by_in_place(&t->src, 1); break;
    case 'F':
    case 'f': type_info |= TI_REAL; ss_advance_by_in_place(&t->src, 1); break;
    case 'L':
    case 'l': type_info |= TI_LONG; ss_advance_by_in_place(&t->src, 1); break;
  }

  token_t tok = (token_t) {
    .kind = type_info & TI_REAL ? TOK_REALLIT : TOK_INTLIT,
    .type_info = type_info,
    .loc = t->loc,
    .view = ss_from_extents(og.data, t->src.data),
  };

  if (type_info & TI_UNSIGNED) {
    if (type_info & TI_REAL) {
      fdiagf(stderr, DIAG_ERROR, t->loc, "Cannot declare an unsigned real literal.");
      return INVALID_TOKEN(t->loc);
    }
    tok.as.u = integer;
  } else {
    if (type_info & TI_REAL)
      tok.as.r = real;
    else 
      tok.as.i = integer;
  }

  return tok; 
}

static token_t tok_tokenize(tokenizer_t* t) {
  switch(*t->src.data) {
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
    case '#':
    case '!':
    case ':':
      return make_token(t, *t->src.data, 1);
    case '>':
      switch(*(t->src.data + 1)) {
        case '=': return make_token(t, TOK_GEQ, 2);
        default:  return make_token(t, *t->src.data, 1);
      }
      break;
    case '<':
      switch(*(t->src.data + 1)) {
        case '=': return make_token(t, TOK_LEQ, 2);
        default: return make_token(t, *t->src.data, 1);
      }
      break;
    case '=':
      switch(*(t->src.data + 1)) {
        case '=': return make_token(t, TOK_EQEQ, 2);
        default: return make_token(t, *t->src.data, 1);
      }
      break;
    case '.':
      if (isdigit(*(t->src.data + 1))) return tok_num_literal(t);
      return make_token(t, *t->src.data, 1);
      break;
    case '-':
      switch(*(t->src.data + 1)) {
        case '>': return make_token(t, TOK_ARROW, 2);
        case '=': return make_token(t, TOK_MINEQS, 2);
        default:  return make_token(t, *t->src.data, 1);
      }
      break;
    case '@': {
      const char* end = t->src.data + 1;
      for (; *end && (isalpha(*end) || isdigit(*end) || *end == '_'); end++);
      ss_t src_slice = ss(t->src.data, end - t->src.data);

      bool found = false; 
      for(size_t i=0; i < sizeof(tok_ctds)/sizeof(*tok_ctds); i++) {
        if (!tok_ctds[i]) continue;

        ss_t ctd_slice = ss_from_cstr(tok_ctds[i]);

        if(ss_eq(ctd_slice, src_slice)) {
          found = true;
          return make_token(t, i, end - t->src.data);
        }
      }
      if(!found) {
        fdiagf(stderr, DIAG_ERROR, t->loc, "Tokenizer: - unrecognized Compile-Time directive");
        return INVALID_TOKEN(t->loc);
      }
    }
      break;
    case '"':
      return tok_string(t);
      break;

    default:
      if(isalpha(*t->src.data) || *t->src.data == '_') {
        const char* end = t->src.data;
        for (; *end && (isalpha(*end) || isdigit(*end) || *end == '_'); end++);
        ss_t src_slice = ss(t->src.data, end - t->src.data);

        bool found = false;
        for(size_t i=0; i < sizeof(tok_keywords)/sizeof(*tok_keywords); i++) {
          if (!tok_keywords[i]) continue;

          ss_t kw_slice  = ss_from_cstr(tok_keywords[i]);

          if(ss_eq(kw_slice, src_slice)) {
            found = true;
            return make_token(t, i, end - t->src.data);
          }
        }
        if(found) break;

        token_t ident = make_token(t, TOK_IDENT, src_slice.len);
        ident.as.s = arena_sprintf(t->arena, PRI_SS, SS_FMT(src_slice));
      return ident;
      } else if (isdigit(*t->src.data)) {
        return tok_num_literal(t);
      }
  }

  fdiagf(stderr, DIAG_ERROR, t->loc, "Tokenizer: - Unrecognized token");
  return INVALID_TOKEN(t->loc);
}

token_t tok_next(tokenizer_t* t) {
  if (t->src.len == 0)
    return (token_t){ .kind = TOK_EOF, .loc = t->loc };

  // skip comments
  if (ss_starts_with_cstr(t->src, COMMENT_PREFIX)) ss_advance_until_in_place(&t->src, "\n");

  if(*t->src.data == '\n') {
    ss_advance_by_in_place(&t->src, 1);
    t->loc.line++;
    t->loc.col = 1;
    t->loc.line_view = ss_sub(t->src, 0, ss_index_of(t->src, '\n'));
  }

  // NOTE: this could ingest '\n'
  ss_t spaces = ss_tok_while(&t->src, isspace);
  t->loc.col += spaces.len;

  token_t tok = tok_tokenize(t);

  t->loc.col += tok.view.len;

  return tok;
}

tokenizer_t tok_init(ss_t src, const char* origin, arena_t* arena) {
  return (tokenizer_t) {
    .arena = arena,
    .src = src,
    .loc = (loc_t) {
      .line = 1,
      .col  = 1,
      .path = origin,
      .line_view = ss_sub(src, 0, ss_index_of(src, '\n')),
    },
  };
}

// static inline token_t get_tok(parser_t* p) {
//   if (p->current >= p->tokens.count) {
//     fdiagf(stderr, DIAG_FATAL, vec_get(&p->tokens, p->current).loc, "No more tokens.");
//     abort();
//   }
//   return vec_get(&p->tokens, p->current);
// }

// static inline int tok_is(parser_t* p, tok_kind_t kind) {
//   return get_tok(p).kind == kind; 
// }
