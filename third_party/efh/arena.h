/**
 * arena.h - v1.0.0 - Copyright (c) 2026 Edoardo Figini
 * 
 * # Arena allocator in C 
 *
 * ## STB-style Single Header Library
 *
 * This is a stb-style single header library. See
 * https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
 * for more info.
 *
 * This library deviates from stb guidelines in some aspect, namely naming
 * internal symbols.
 * See the dedicated chapter.
 *
 *
 * ## Usage
 *
 * This file provides both the interface and the implementation.
 * To instantiate the implementation,
 * ```
 *      #define ARENA_IMPLEMENTATION
 * ```
 * in *ONE* source file, before #including this file.
 *
 * The API reference is this file.
 * You can find documentation for functions and macros where they are defined.
 *
 * example usage:
 * ```
 * ```
 *
 * The library can also be compiled as its own translation unit.
 * For example, with gcc:
 * ```console
 * $ gcc -DARENA_IMPLEMENTATION -x c -c arena.h 
 * ```
 *
 *
 * ## Config Parameters
 *
 * This library offers presonalization through macros.
 * Before #include "arena.h" you can define macros to customize the library behavior:
 *   - `ARENA_DEF`:             can be used to alter function declarations (e.g.
 *                              `static inline`, `extern "C"`, etc.)
 *                              (defaults to nothing)
 *   - `ARENA_ASSERT`:          can be used to define a custom function to perform
 *                              assertions.
 *                              (defaults to libc assert)
 *   - `ARENA_PAGES_PER_BLOCK`: can be used to alter the number of VM pages the block
 *                              is composed of. A page is 4096 bytes. Change this value
 *                              if you need to allocate objects bigger than the capacity
 *                              of a block (page size - block header size).
 *                              (defaults to 1)
 *
 *
 * ## Naming
 *
 * All functions and macros available to the user are prefixed by `arena_` or `ARENA_`.
 * All functions, macros and type names dedicated exclusively to internal use are
 * prefixed by `__arena_` or `__ARENA_`.
 * This is chosen to avoid confusion when looking up public API (for example by
 * grep'ing for `arena_.*`, which would include stb-style `arena__` internal declarations).
 * The choice to "hide" internal symbols from the user is dictated by the fact that
 * they might change even across minor and patch releases.
 *
 *
 * ## Changelog 
 *
 *    1.0.0 (2026-06-10) First public release
 *
 *
 * ## Contributors
 *
 *   - Edoardo Figini (@EdoardoFigini)
 *
 *
 * ## License
 *
 * MIT License
 *
 * Copyright (c) 2026 Edoardo Figini
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


#ifndef ARENA_H
#define ARENA_H

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef UINT32 arena_u32_t;
typedef BYTE   arena_byte_t;

#elif defined(__linux__)
#include <stdlib.h>
#include <sys/mman.h>
#include <stdint.h>

typedef uint32_t arena_u32_t;
typedef uint8_t  arena_byte_t;

#else 
#error Unsupported platform

#endif

#include <stddef.h>
#include <stdarg.h>

// lib config

#ifndef ARENA_DEF
#define ARENA_DEF
#endif

#ifndef ARENA_ASSERT
#include <assert.h>
#define ARENA_ASSERT(x) assert(x)
#endif

#ifndef ARENA_PAGES_PER_BLOCK
#define ARENA_PAGES_PER_BLOCK 1
#endif

////////////

#if defined(__GNUC__) || defined(__clang__)
#define ARENA_FMT_PRINTF(fmt, first_vararg) __attribute__ ((format (printf, fmt, first_vararg)))
#else
#define ARENA_FMT_PRINTF(...)
#endif

typedef struct __arena_block {
  arena_u32_t size;
#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) 
  arena_u32_t __align;
#endif
  struct __arena_block *next;
  arena_byte_t data[];
} __arena_block_t;

/**
 * The arena
 * @param head ponter to the first block in the list
 * @param block_count number of blocks in the list
 */
typedef struct {
  __arena_block_t *head;
  size_t block_count;
} arena_t;

/**
 * Allocates a set size on the arena
 * @param a pointer to arena
 * @param size size in bytes of memory to reserve
 * @return pointer to newly allocated bytes
 */
ARENA_DEF void* arena_alloc(arena_t* a, size_t size);

/**
 * Deallocates the whole arena, unmaps all blocks
 * @param a pointer to arena to be freed
 */
ARENA_DEF void arena_free(arena_t* a);

/**
 * Creates a duplicate of a null-terminated string on the arena
 * @param a pointer to arena
 * @param str C-string to be duplicated
 * @return duplicated C-string on the arena
 */
ARENA_DEF char* arena_strdup(arena_t* a, const char* str);

/**
 * Renders a printf-style formatted string on the arena
 * @param a pointer to arena
 * @param fmt format string
 * @param ... variable length arguments
 * @return rendered C-string on the arena
 */
ARENA_FMT_PRINTF(2, 3) ARENA_DEF char* arena_sprintf(arena_t* a, const char* fmt, ...);

/**
 * Push an object on the arena.
 * @param a pointer to arena
 * @param item object to be allocated
 * @return pointer to allocated object
 */
#define arena_push(a, item) \
  __arena_push((a), (void*)((__typeof__((item))[]){(item)}), sizeof(item))

ARENA_DEF void* __arena_push(arena_t* a, void* data, size_t size);

#endif // !ARENA_H

#ifdef ARENA_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) && defined(__clang__)
extern __declspec(noreturn) void abort();
#endif


#define PAGE_SIZE 0x1000
#define BLOCK_CAPACITY ((PAGE_SIZE * ARENA_PAGES_PER_BLOCK) - offsetof(__arena_block_t, data))
#define BLOCK_CAN_ALLOC(block_ptr, bytes) (block_ptr != NULL && block_ptr->size + bytes < BLOCK_CAPACITY)

static __arena_block_t* __arena_alloc_block() {
  // NOTE: is alignment still valid on 32 bit?
  ARENA_ASSERT(offsetof(__arena_block_t, data) == sizeof(void*) * 2);
#ifdef _WIN32
  LPVOID pages = VirtualAlloc(NULL, PAGE_SIZE * ARENA_PAGES_PER_BLOCK, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  ARENA_ASSERT(pages);
#else
  void* pages = mmap(NULL, PAGE_SIZE * ARENA_PAGES_PER_BLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ARENA_ASSERT(pages != MAP_FAILED);
#endif
  __arena_block_t* block = (__arena_block_t*)pages;
  return block;
}

static void __arena_dealloc_block(__arena_block_t* block) {
#ifdef _WIN32
  VirtualFree(block, 0, MEM_RELEASE);
#else
  munmap(block, PAGE_SIZE * ARENA_PAGES_PER_BLOCK);
#endif
}

ARENA_DEF void* arena_alloc(arena_t* a, size_t bytes) {
  if (!a)     return NULL;
  if (!bytes) return NULL;

  // TODO: make number of pages a field of block and allow blocks to 
  // be multiple pages long here
  if (bytes > BLOCK_CAPACITY) {
    fprintf(
      stderr,
      "%s:%d: Trying to allocate too many bytes (%zu, max is %zu)\n"
      "You can `#define ARENA_PAGES_PER_BLOCK <n>` to adjust capacity.\n",
      __FILE__, __LINE__, bytes, BLOCK_CAPACITY
    );
    abort();
  }

  if (a->head == NULL || !BLOCK_CAN_ALLOC(a->head, bytes)) {
    __arena_block_t *b = __arena_alloc_block(); 

    b->next = a->head;
    a->head = b;

    a->block_count++;
  }

  size_t free_idx = a->head->size;

  a->head->size += bytes;
  
  return (void*)(a->head->data + free_idx);
}

ARENA_DEF void arena_free(arena_t *a) {
  if (!a) return;
  if (a->head == NULL) return;

  __arena_block_t* tmp;
  __arena_block_t* next;

  for (tmp = a->head; tmp != NULL; tmp = next) {
    next = tmp->next;
    __arena_dealloc_block(tmp);
  }

  a->head = NULL;
  a->block_count = 0;
}

ARENA_DEF char* arena_strdup(arena_t* a, const char* str) {
  size_t len = strlen(str) + 1;
  char* copy = (char*)arena_alloc(a, len);
  if (!copy) return NULL;

#ifdef _WIN32
  CopyMemory(copy, str, len);
#else
  memcpy(copy, str, len);
#endif
  
  return copy;
}

ARENA_DEF char* arena_sprintf(arena_t* a, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  va_list args_copy;
  va_copy(args_copy, args);
  int len = vsnprintf(NULL, 0, fmt, args_copy);
  va_end(args_copy);

  if (len < 0) {
    va_end(args);
    return NULL;
  }

  char* buffer = (char*)arena_alloc(a, (size_t)len + 1);
  if (!buffer) {
    va_end(args);
    return NULL;
  }

  vsnprintf(buffer, len + 1, fmt, args);
  va_end(args);

  return buffer;
}

ARENA_DEF void* __arena_push(arena_t* a, void* data, size_t size) {
  void* addr = arena_alloc(a, size);
  memcpy(addr, data, size);

  return addr;
}

#undef BLOCK_CAN_ALLOC
#undef BLOCK_CAPACITY
#undef PAGE_SIZE

#endif // !ARENA_IMPLEMENTATION
