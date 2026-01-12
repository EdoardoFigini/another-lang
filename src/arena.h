#ifndef ARENA_H
#define ARENA_H

#ifndef ARENA_ASSERT
#include <assert.h>
#define ARENA_ASSERT(x) assert(x)
#endif

#ifndef ARENA_PAGES_PER_BLOCK
#define ARENA_PAGES_PER_BLOCK 1
#endif

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

#define __FILENAME__ __FILE_NAME__

#else 
#error Unsupported platform

#endif

#if defined(__GNUC__) || defined(__clang__)
#define ARENA_FMT_PRINTF(fmt, first_vararg) __attribute__ ((format (printf, fmt, first_vararg)))
#else
#define ARENA_FMT_PRINTF(...)
#endif

#include <stddef.h>
#include <stdarg.h>

typedef struct _block {
  arena_u32_t size;
#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) 
  arena_u32_t __align;
#endif
  struct _block *next;
  arena_byte_t data[];
} block_t;

typedef struct {
  block_t *head;
  size_t block_count;
  // TODO: last_error
} arena_t;

void* arena_alloc(arena_t*, size_t);
void arena_free(arena_t*);
char* arena_strdup(arena_t*, const char*);
char* arena_sprintf(arena_t* a, const char* fmt, ...) ARENA_FMT_PRINTF(2, 3); 

#ifdef ARENA_IMPLEMENTATION

#define PAGE_SIZE 0x1000
#define BLOCK_CAPACITY ((PAGE_SIZE * ARENA_PAGES_PER_BLOCK) - offsetof(block_t, data))
#define BLOCK_CAN_ALLOC(block_ptr, bytes) (block_ptr != NULL && block_ptr->size + bytes < BLOCK_CAPACITY)

static block_t* __arena_alloc_block() {
  // NOTE: is alignment still valid on 32 bit?
  ARENA_ASSERT(offsetof(block_t, data) == sizeof(void*) * 2);
#ifdef _WIN32
  LPVOID pages = VirtualAlloc(NULL, PAGE_SIZE * ARENA_PAGES_PER_BLOCK, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
  void* pages = mmap(NULL, PAGE_SIZE * ARENA_PAGES_PER_BLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
  block_t* block = (block_t*)pages;
  return block;
}

static void __arena_dealloc_block(block_t* block) {
#ifdef _WIN32
  VirtualFree(block, 0, MEM_RELEASE);
#else
  munmap(block, PAGE_SIZE * ARENA_PAGES_PER_BLOCK);
#endif
}

void* arena_alloc(arena_t* a, size_t bytes) {
  if (!a) return NULL;
  if (bytes > BLOCK_CAPACITY) {
    fprintf(
      stderr,
      "%s:%d: Trying to allocate too many bytes (%zu, max is %zu)"
      "You can `#define ARENA_PAGES_PER_BLOCK x` to adjust capacity.",
      __FILENAME__, __LINE__, bytes, BLOCK_CAPACITY
    );
    abort();
  }

  if (a->head == NULL || !BLOCK_CAN_ALLOC(a->head, bytes)) {
    block_t *b = __arena_alloc_block(); 

    b->next = a->head;
    a->head = b;

    a->block_count++;
  }

  size_t free_idx = a->head->size;

  a->head->size += bytes;
  
  return (void*)(a->head->data + free_idx);
}

void arena_free(arena_t *a) {
  if (!a) return;
  if (a->head == NULL) return;

  block_t* tmp;
  block_t* next;

  for (tmp = a->head; tmp != NULL; tmp = next) {
    next = tmp->next;
    __arena_dealloc_block(tmp);
  }

  a->head = NULL;
  a->block_count = 0;
}

char* arena_strdup(arena_t* a, const char* str) {
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

char* arena_sprintf(arena_t* a, const char* fmt, ...) {
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

#undef BLOCK_CAN_ALLOC
#undef BLOCK_CAPACITY
#undef PAGE_SIZE

#endif // !ARENA_IMPLEMENTATION
#endif // !ARENA_H
