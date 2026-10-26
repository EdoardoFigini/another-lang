/**
 * vec.h - v1.0.0 - Copyright (c) 2026 Edoardo Figini
 * 
 * # Vectors/Dynamic arrays in C
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
 *      #define VEC_IMPLEMENTATION
 * ```
 * in *ONE* source file, before #including this file.
 *
 * The library offers two ways to declare a vector:
 *   - `VEC(T)`:        macro that expands to a struct for a vector of type T.
 *   - `VEC_HEADER(T)`: macro that expands to only the fields of the struct for a 
 *                      vector of type T.
 *                      This can be used to create structs that "inherit" the vector.
 *                      It is *MANDATORY* that the VEC_HEADER macro is the first
 *                      field of the struct, otherwise the API will break.
 *
 * examples:
 * ```C  
 * VEC(int) numbers = { 0 }; // creates an empty vector of integers
 * ```  
 *
 * ```C
 * typedef struct {
 *    VEC_HEADER(int);
 *    size_t count_positive;
 *    size_t count_negative;
 *    size_t count_zero;
 * } numbers_ex_t 
 *
 * numbers_ex_t numbers_ex = { 0 }; // creates an empty vector of integers 
 *                                  // with additional fields
 * ```
 *
 * The API reference is this file.
 * You can find documentation for functions and macros where they are defined.
 *
 * example usage:
 * ```
 * #define VEC_IMPLEMENTATION
 * #include "vec.h"
 *
 * #include <stdio.h>
 *
 * int main() {
 *    VEC(int) nums = { 0 };
 *
 *    vec_push(&nums, 0);
 *    vec_push(&nums, 1);
 *    vec_push(&nums, 2);
 *    vec_push(&nums, 3);
 *
 *    vec_foreach(n, &nums) {
 *      printf("%d\n", *n);
 *    }
 *
 *    vec_set(&nums, 0, 67);
 *    vec_data(&nums)[3] = 420; 
 *
 *    vec_remove_unordered(&nums, 1);
 *    vec_remove_unordered(&nums, 2);
 *
 *    for (size_t i=0; i < vec_len(&nums); i++) 
 *      printf("%d\n", vec_get(&nums, i));
 *
 * }
 * ```
 *
 * The library can also be compiled as its own translation unit.
 * For example, with gcc:
 * ```console
 * $ gcc -DVEC_IMPLEMENTATION -x c -c vec.h 
 * ```
 *
 *
 * ## Config Parameters
 *
 * This library offers presonalization through macros.
 * Before #include "vec.h" you can define macros to customize the library behavior:
 *   - `VEC_GROWTH_FACTOR`: dictates how much memory is allocated each time the 
  *                         vector grows.
 *                          (defaults to 2)
 *   - `VEC_DEF`:           can be used to alter function declarations (e.g. 
 *                          `static inline`, `extern "C"`, etc.)
 *                          (defaults to nothing)
 *   - `VEC_ALLOC`:         can be use to define a custom allocator
 *                          (defaults to calloc/HeapAlloc)
 *   - `VEC_REALLOC`:       can be use to define a custom allocator
 *                          (defaults to realloc/HeapReAlloc)
 *   - `VEC_FREE`:          can be use to define a custom allocator
 *                          (defaults to free/HeapFree)
 *
 *
 * ## Naming
 *
 * All functions and macros available to the user are prefixed by `vec_` or `VEC_`.
 * All functions, macros and type names dedicated exclusively to internal use are
 * prefixed by `__vec_` or `__VEC_`.
 * This is chosen to avoid confusion when looking up public API (for example by
 * grep'ing for `vec_.*`, which would include stb-style `vec__` internal declarations).
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
#include <stdint.h>
#endif

// Lib config 

#ifndef VEC_GROWTH_FACTOR
#define VEC_GROWTH_FACTOR 2
#endif

#ifndef VEC_DEF
#define VEC_DEF 
#endif

#ifndef VEC_ALLOC
#if defined(_WIN32)
#define VEC_ALLOC(count, size) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (count) * (size))
#else
#define VEC_ALLOC(count, size) calloc(count, size)
#endif
#endif

#ifndef VEC_REALLOC
#if defined(_WIN32)
#define VEC_REALLOC(ptr, count, size) HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (ptr), (count) * (size))
#else
#define VEC_REALLOC(ptr, count, size) realloc((ptr), (count) * (size))
#endif
#endif

#ifndef VEC_FREE
#if defined(_WIN32)
#define VEC_FREE(ptr) HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, (ptr))
#else
#define VEC_FREE(ptr) free((ptr))
#endif
#endif

//////////////

#if defined(_WIN32)
#define VEC_MEMCOPY(dst, src, size) CopyMemory((BYTE*)(dst), (BYTE*)(src), (size))
#define VEC_MEMMOVE(dst, src, size) MoveMemory((BYTE*)(dst), (BYTE*)(src), (size))
#else
#define VEC_MEMCOPY(dst, src, size) memcpy((dst), (src), (size))
#define VEC_MEMMOVE(dst, src, size) memmove((dst), (src), (size))
#endif


/**
 * Macro to define the fields of a vector.
 * Can be used inside a struct to embed a vector.
 * @param T type of the elements
 */
#define VEC_HEADER(T) \
  size_t len; \
  size_t cap; \
  T* data

/**
 * Macro to declare an anonymous struct representing a vector of type T.
 * @param T type of the elements
 */
#define VEC(T) struct { VEC_HEADER(T);  }

typedef VEC(void) __vec_t;

/**
 * Resolves to the type of the elements in the vector.
 * @param vec pointer to the vector
 */
#define vec_typeof(vec) __typeof__(*(vec)->data)

/**
 * Pushes an item to the end of the vector.
 * @param vec pointer to the vector
 * @param item item to push
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_push(vec, item) \
  __vec_generic_push((__vec_t*)(vec), (vec_typeof((vec))*)((vec_typeof((vec))[]){item}), sizeof(*(vec)->data))

// TODO: not implemented
// #define vec_extend(veca, vecb)

/**
 * Checks if the vector is empty.
 * @param vec pointer to the vector
 * @return true if empty, false otherwise
 */
#define vec_is_empty(vec) ((vec)->len <= 0)

#define __VEC_STR(x) #x

#define __VEC_ASSERT_NON_EMPTY(vec) \
  (vec_is_empty((vec)) ? __vec_abort(__VEC_STR(vec) " is empty.") : 0)

/**
 * Removes and returns the last element of the vector.
 * @param vec pointer to the vector
 * @return the popped element
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_pop(vec) \
  (vec)->data[(__VEC_ASSERT_NON_EMPTY(vec), --(vec)->len)]

/**
 * Returns the first element of the vector.
 * @param vec pointer to the vector
 * @return the first element
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_first(vec) \
  (vec)->data[(__VEC_ASSERT_NON_EMPTY(vec), 0)]

/**
 * Returns the last element of the vector.
 * @param vec pointer to the vector
 * @return the last element
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_last(vec) \
  (vec)->data[(__VEC_ASSERT_NON_EMPTY(vec), (vec)->len - 1)]

/**
 * Returns the element at the specified index.
 * @param vec pointer to the vector
 * @param idx index of the element
 * @return the element at the index
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_get(vec, idx) \
  (vec)->data[(__VEC_ASSERT_NON_EMPTY(vec), __vec_check_idx(idx, (vec)->len))]

/**
 * Sets the element at the specified index to a new value.
 * @param vec pointer to the vector
 * @param idx index to modify
 * @param item new value
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_set(vec, idx, item) \
  VEC_MEMCOPY(&vec_get((vec), (idx)), (vec_typeof((vec))*)((vec_typeof((vec))[]){item}), sizeof(*(vec)->data))

/**
 * Swaps two elements in the vector.
 * @param vec pointer to the vector
 * @param idxa index of the first element
 * @param idxb index of the second element
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_swap(vec, idxa, idxb) \
  do { \
    __typeof__((idxa)) _idxa = (idxa); \
    __typeof__((idxb)) _idxb = (idxb); \
    vec_typeof((vec)) tmp = vec_get((vec), _idxa); \
    vec_set((vec), _idxa, vec_get((vec), _idxb)); \
    vec_set((vec), _idxb, tmp); \
  } while (0)

/**
 * Removes an element at the specified index by swapping it with the last element.
 * This is an O(1) operation but does not preserve the order of elements.
 * @param vec pointer to the vector
 * @param idx index of the element to remove
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_remove_unordered(vec, idx) \
  do {\
    vec_swap((vec), idx, (vec)->len - 1); \
    --(vec)->len;\
  } while (0)

/**
 * Removes an element at the specified index, preserving the order of the
 * remaining elements. This is an O(n) operation.
 * @param vec pointer to the vector
 * @param idx index of the element to remove
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_remove(vec, idx) \
  __vec_generic_remove((__vec_t*)(vec), (idx), sizeof(*(vec)->data))

/**
 * Reserves memory for a specific capacity in the vector.
 * @param vec pointer to the vector
 * @param cap new capacity. Must be greater than the current capacity.
 *            If cap < vec->cap the function will return with no 
 *            effects on vec.
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_reserve(vec, cap) \
  __vec_generic_reserve((__vec_t*)(vec), (cap), sizeof(*(vec)->data))

/**
 * Shrinks the allocated memory to match the exact number of elements.
 * @param vec pointer to the vector
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_shrink(vec) \
  __vec_generic_shrink((__vec_t*)(vec), sizeof(*(vec)->data))

/**
 * Copies the contents of one vector to another.
 * @param dst pointer to the destination vector
 * @param src pointer to the source vector
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_copy(dst, src) \
  do {\
    if (sizeof(*(src)->data) != sizeof(*(dst)->data)) \
      __vec_abort(__VEC_STR(src) " and " __VEC_STR(dst) " have different data size."); \
    vec_reserve((dst), (src)->len); \
    VEC_MEMCOPY((dst)->data, (src)->data, sizeof(*(src)->data) * (src)->len); \
    (dst)->len = (src)->len; \
  } while (0)

/**
 * Finds the index of a pointer to an element within the vector.
 * @param vec pointer to the vector
 * @param elem pointer to the element
 * @return index of the element
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_indexof(vec, elem) \
  ( \
    __VEC_ASSERT_NON_EMPTY((vec)), \
    ((uintptr_t)(elem) < (uintptr_t)(vec)->data) ? __vec_abort("Item not in vector " __VEC_STR(vec) ".") : 0, \
    (elem) >= (vec)->data + (vec)->len ? __vec_abort("Item not in vector " __VEC_STR(vec) ".") : 0, \
    (elem) -  (vec)->data \
  )

/**
 * Returns a pointer to the underlying data array.
 * @param vec pointer to the vector
 * @return pointer to the elements
 */
#define vec_data(vec)   (vec)->data

/**
 * Returns the number of elements in the vector
 * @param vec pointer to the vector
 * @return number of elements
 */
#define vec_len(vec)    (vec)->len

/**
 * Returns the capacity of the vector.
 * @param vec pointer to the vector
 * @return maximum number of elements before reallocation is needed
 */
#define vec_cap(vec)    (vec)->cap

/**
 * Resets the vector without freeing memory.
 * @param vec pointer to the vector
 */
#define vec_reset(vec) ((vec)->len = 0)

/**
 * Clears the vector, freeing all allocated memory and resetting fields to 0.
 * @param vec pointer to the vector
 */
#define vec_clear(vec) __vec_generic_clear((__vec_t*)(vec))

/**
 * Returns the total allocated size in bytes.
 * @param vec pointer to the vector
 * @return allocated size in bytes
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_reserved_size(vec) ((vec)->cap * sizeof(*(vec)->data))

/**
 * Returns the size of the occupied portion of the vector in bytes.
 * @param vec pointer to the vector
 * @return size in bytes of the elements currently in the vector
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_data_size(vec)     ((vec)->len * sizeof(*(vec)->data))

/**
 * Returns the size of a single element in bytes.
 * @param vec pointer to the vector
 * @return size of one element in bytes
 */
#define vec_elem_size(vec)     sizeof(*(vec)->data)

/**
 * Sorts the elements in the vector using qsort.
 * @param vec pointer to the vector
 * @param fn comparator function pointer
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_sort(vec, fn) \
  qsort((vec)->data, (vec)->len, sizeof(*(vec)->data), fn)

/**
 * Iterates over the elements of the vector.
 * @param iter identifier for the pointer to the current element
 * @param vec pointer to the vector
 * \warning will evaluate vec multiple times, it is advisable to not 
 *          put state-changing logic as an argument to this macro
 */
#define vec_foreach(iter, vec) \
  for ( \
    vec_typeof((vec))* iter = (vec)->data; \
    iter < (vec)->data + (vec)->len; \
    iter++ \
  )

VEC_DEF void   __vec_generic_reserve(__vec_t* vec, size_t cap, size_t size);
VEC_DEF void   __vec_generic_shrink(__vec_t* vec, size_t size);
VEC_DEF void   __vec_generic_push(__vec_t* vec, void* item, size_t size);
VEC_DEF void   __vec_generic_remove(__vec_t* vec, size_t idx, size_t size);
VEC_DEF void   __vec_generic_clear(__vec_t* vec);
VEC_DEF size_t __vec_check_idx(size_t idx, size_t count);

#ifdef _WIN32
__declspec(noreturn)
#else
__attribute__((noreturn))
#endif
VEC_DEF void __vec_abort(const char* msg);

#endif // VEC_H

#ifdef VEC_IMPLEMENTATION

#include <stdio.h>

VEC_DEF void __vec_generic_reserve(__vec_t* vec, size_t cap, size_t size) {
  if (cap < vec->cap) return;

  void* data = vec->data
              ? VEC_REALLOC(vec->data, cap, size) 
              : VEC_ALLOC(cap, size); 

  if(!data) __vec_abort("Failed to allocate memory.");

  vec->data = data;
  vec->cap = cap;
}

VEC_DEF void __vec_generic_shrink(__vec_t* vec, size_t size) {
  void* data = vec->data
              ? VEC_REALLOC(vec->data, vec->len, size) 
              : VEC_ALLOC(vec->len, size); 

  if(!data) __vec_abort("Failed to allocate memory.");
  vec->data = data;
  vec->cap = vec->len;
}

VEC_DEF void __vec_generic_push(__vec_t* vec, void* item, size_t size) {
  if (vec->len >= vec->cap) {
    size_t cap = vec->cap ? vec->cap * VEC_GROWTH_FACTOR : 2;

    vec->data = vec->data
                ? VEC_REALLOC(vec->data, cap, size) 
                : VEC_ALLOC(cap, size); 

    if(!vec->data) __vec_abort("Failed to allocate memory.");
    vec->cap = cap;
  }

  VEC_MEMCOPY((char*)vec->data + (vec->len * size), item, size);

  vec->len += 1;
}

VEC_DEF void __vec_generic_clear(__vec_t* vec) {
  if (vec->data) {
    VEC_FREE(vec->data);
  }

  vec->data  = NULL;
  vec->len = 0;
  vec->cap   = 0;
}

VEC_DEF void __vec_generic_remove(__vec_t* vec, size_t idx, size_t size) {
  if (idx >= vec->len) __vec_abort("Index out of range.");

  if (idx < vec->len - 1)
    VEC_MEMMOVE(
      (char*)vec->data + (idx * size),
      (char*)vec->data + ((idx + 1) * size),
      (vec->len - idx - 1) * size
    );


  vec->len--;
}

VEC_DEF void __vec_abort(const char* msg) {
  fprintf(stderr, "vec.h: %s Aborting.\n", msg);
  abort();
}


VEC_DEF size_t __vec_check_idx(size_t idx, size_t count) {
  if (idx >= count) __vec_abort("Index out of range.");
  return idx;
}

#endif
