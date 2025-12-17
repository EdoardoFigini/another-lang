#ifndef _BYTECODE_H
#define _BYTECODE_H

#include <stdint.h>

typedef uint32_t instruction_t; enum {
  INST_NOP = 0,

  INST_PUSH,
  // INST_PUSHL, // long
  INST_POP,
  // INST_POPL,

  INST_LOAD,
  INST_LOADG,
  INST_LOADC,
  INST_STORE,
  INST_STOREG,

  INST_CALL,
  INST_HOSTCALL,
  INST_ICALL, // interface call
  INST_RET,

  INST_ADD,
  INST_SUB,
  INST_MULT,
  INST_DIVI,
  INST_DIVU,
  INST_REM,
  INST_ADDF,
  INST_SUBF,
  INST_MULTF,
  INST_DIVF,

  INST_COUNT,
};

typedef struct {
  instruction_t* items;
  size_t count;
  size_t capacity;
} instrarr_t;

#endif
