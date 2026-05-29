#ifndef VEC_H
#define VEC_H

#include <stddef.h>
#include <assert.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <string.h>
#endif

#define __VEC_HEADER__(T) \
  size_t count; \
  size_t cap; \
  T* data

#define VEC(T) struct { __VEC_HEADER__(T);  }

#ifndef VEC_GROWTH_FACTOR
#define VEC_GROWTH_FACTOR 2
#endif

typedef struct {
  __VEC_HEADER__(void);
} __vec_t;

#define vec_typeof(vec) __typeof__(*(vec)->data)

#define vec_push(vec, item) \
  __generic_vec_push((__vec_t*)(vec), (vec_typeof((vec))*)((vec_typeof((vec))[]){item}), sizeof(*(vec)->data))

// TODO: not implemented
// #define vec_extend(veca, vecb)

#define vec_is_empty(vec) ((vec)->count <= 0)

#define __STR(x) #x

#define ASSERT_NON_EMPTY(vec) \
  (vec_is_empty((vec)) ? __vec_abort(__STR(vec) " is empty.") : 0)

#define vec_pop(vec) \
  (vec)->data[(ASSERT_NON_EMPTY(vec), --(vec)->count)]

#define vec_first(vec) \
  (vec)->data[(ASSERT_NON_EMPTY(vec), 0)]

#define vec_last(vec) \
  (vec)->data[(ASSERT_NON_EMPTY(vec), (vec)->count - 1)]

#define vec_get(vec, idx) \
  (vec)->data[(ASSERT_NON_EMPTY(vec), __vec_check_idx(idx, (vec)->count))]

#ifdef _WIN32
#define vec_set(vec, idx, item) \
  CopyMemory(&vec_get((vec), (idx)), (vec_typeof((vec))*)((vec_typeof((vec))[]){item}), sizeof(*(vec)->data))
#else
#define vec_set(vec, idx, item) \
  memcpy(&vec_get((vec), (idx)), (vec_typeof((vec))*)((vec_typeof((vec))[]){item}), sizeof(*(vec)->data))
#endif

#define vec_swap(vec, idxa, idxb) \
  do { \
    __typeof__((idxa)) _idxa = (idxa); \
    __typeof__((idxb)) _idxb = (idxb); \
    vec_set((vec), _idxa, vec_get((vec), _idxa) ^ vec_get((vec), _idxb)); \
    vec_set((vec), _idxb, vec_get((vec), _idxa) ^ vec_get((vec), _idxb)); \
    vec_set((vec), _idxa, vec_get((vec), _idxa) ^ vec_get((vec), _idxb)); \
  } while (0)

#define vec_remove_unordered(vec, idx) \
  do {\
    vec_swap((vec), idx, (vec)->count - 1); \
    --(vec)->count;\
  } while (0)

#define vec_remove(vec, idx) \
  __generic_vec_remove((__vec_t*)(vec), (idx), sizeof(*(vec)->data))

#define vec_reserve(vec, cap) \
  __generic_vec_reserve((__vec_t*)(vec), (cap), sizeof(*(vec)->data))

#define vec_shrink(vec) \
  __generic_vec_shrink((__vec_t*)(vec), sizeof(*(vec)->data))

#ifdef _WIN32
#define vec_copy(dst, src) \
  do {\
    if (sizeof(*(src)->data) != sizeof(*(dst)->data)) \
      __vec_abort(__STR(src) " and " __STR(dst) " have different data size."); \
    vec_reserve((dst), (src)->cap); \
    CopyMemory((dst)->data, (src)->data, sizeof(*(src)->data) * (src)->count); \
    (dst)->count = (src)->count; \
  } while (0)
#else
#define vec_copy(src, dst) \
  do {\
    if (sizeof(*(src)->data) != sizeof(*(dst)->data)) \
      __vec_abort(__STR(src) " and " __STR(dst) " have different data size."); \
    vec_reserve((dst), (src)->cap); \
    memcpy((dst)->data, (src)->data, sizeof(*(src)->data) * (src)->count); \
    (dst)->count = (src)->count; \
  } while (0)
#endif

#define vec_indexof(vec, elem) \
  ( \
    ASSERT_NON_EMPTY((vec)), \
    (elem) <  (vec)->data                ? __vec_abort("Item not in vector " __STR(vec) ".") : 0, \
    (elem) >= (vec)->data + (vec)->count ? __vec_abort("Item not in vector " __STR(vec) ".") : 0, \
    (elem) -  (vec)->data \
  )

#define vec_data(vec)   (vec)->data
#define vec_count(vec)  (vec)->count
#define vec_length(vec) (vec)->count
#define vec_len(vec)    (vec)->count
#define vec_cap(vec)    (vec)->cap

#define vec_reset(vec) ((vec)->count = 0)
#define vec_clear(vec) __generic_vec_clear((__vec_t*)(vec))

#define vec_reserved_size(vec) (vec)->cap   * sizeof(*(vec)->data)
#define vec_data_size(vec)     (vec)->count * sizeof(*(vec)->data)
#define vec_elem_size(vec)                    sizeof(*(vec)->data)

#define vec_sort(vec, fn) \
  qsort((vec)->data, (vec)->count, sizeof(*(vec)->data), fn)

#define vec_foreach(iter, vec) \
  for ( \
    vec_typeof((vec))* iter = (vec)->data; \
    iter < (vec)->data + (vec)->count; \
    iter++ \
  )

void __generic_vec_reserve(__vec_t* vec, size_t cap, size_t size);
void __generic_vec_push(__vec_t* vec, void* item, size_t size);
void __generic_vec_clear(__vec_t* vec);
size_t __vec_check_idx(size_t idx, size_t count);


#ifdef _WIN32
__declspec(noreturn)
#else
__attribute__((noreturn))
#endif
void __vec_abort(const char* msg);

#endif // VEC_H

#ifdef VEC_IMPLEMENTATION

#include <stdio.h>

void __generic_vec_reserve(__vec_t* vec, size_t cap, size_t size) {
  size_t cap_bytes = cap * size;
#ifdef _WIN32
  if (vec->data) HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, vec->data);
  vec->data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cap_bytes);
#else
  if (vec->data) free(vec->data);
  vec->data = calloc(size, cap_bytes);
#endif

  if(!vec->data) __vec_abort("Failed to allocate memory.");

  vec->count = 0;
  vec->cap = cap;
}

void __generic_vec_shrink(__vec_t* vec, size_t size) {
  size_t cap_bytes = vec->count * size;

#ifdef _WIN32
  vec->data = vec->data
              ? HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, vec->data, cap_bytes)
              : HeapAlloc(GetProcessHeap(),   HEAP_ZERO_MEMORY, cap_bytes);
#else
  vec->data = realloc(vec->data, cap_bytes);
#endif

  if(!vec->data) __vec_abort("Failed to allocate memory.");
  vec->cap = vec->count;
}

void __generic_vec_push(__vec_t* vec, void* item, size_t size) {
  if (vec->count >= vec->cap) {
    size_t cap = vec->cap ? vec->cap * VEC_GROWTH_FACTOR : 2;
    size_t cap_bytes = cap * size;

#ifdef _WIN32
    vec->data = vec->data
                ? HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, vec->data, cap_bytes)
                : HeapAlloc(GetProcessHeap(),   HEAP_ZERO_MEMORY, cap_bytes);
#else
    vec->data = realloc(vec->data, cap_bytes);
#endif

    if(!vec->data) __vec_abort("Failed to allocate memory.");
    vec->cap = cap;
}

#ifdef _WIN32
  CopyMemory((BYTE*)vec->data + (vec->count * size), item, size);
#else
  memcpy(vec->data + (vec->count * size), item, size);
#endif

  vec->count += 1;
}

void __generic_vec_clear(__vec_t* vec) {
  if (vec->data) {
#ifdef _WIN32
    HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, vec->data);
#else
    free(vec->data);
#endif
  }

  vec->data  = NULL;
  vec->count = 0;
  vec->cap   = 0;
}

void __generic_vec_remove(__vec_t* vec, size_t idx, size_t size) {
  if (idx >= vec->count) __vec_abort("index out of range.");
  if (vec->count == 1 || idx == vec->count - 1) {
    --vec->count;
    return;
  }

#ifdef _WIN32
  BYTE* new_data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, vec->cap * size);
  CopyMemory(new_data, vec->data, idx * size);
  CopyMemory(new_data + idx * size, (BYTE*)vec->data + (idx + 1) * size, (vec->count - idx - 1) * size);
  HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, vec->data);
#else
  void* new_data = malloc(vec->cap * size);
  memcpy(new_data, vec->data, idx * size);
  memcpy(new_data + idx * size, vec->data + (idx + 1) * size, (vec->count - idx - 1) * size);
  free(vec->data);
#endif

  vec->data = new_data;
  vec->count--;
}

void __vec_abort(const char* msg) {
  fprintf(stderr, "vec.h: %s Aborting.\n", msg);
  abort();
}


size_t __vec_check_idx(size_t idx, size_t count) {
  if (idx >= count) __vec_abort("index out of range.");
  return idx;
}

#endif
