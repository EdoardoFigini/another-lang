#ifndef _BYTECODE_H
#define _BYTECODE_H

#include <stdint.h>

typedef uint32_t instruction_t; enum {
  INST_NOP = 0,

  INST_PUSH,
  INST_POP,

  INST_LOAD,
  INST_LOADG,
  INST_STORE,
  INST_STOREG,

  INST_CALL,
  INST_RET,

  INST_COUNT,
};

typedef struct {
  instruction_t* items;
  size_t count;
  size_t capacity;
} instrarr_t;

#endif
