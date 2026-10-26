/**
 * sb.h - v1.0.0 - Copyright (c) 2026 Edoardo Figini
 * 
 * # String Builder in C
 *
 * ## STB-style Single Header Library
 *
 * This is a stb-style single header library. See
 * https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
 * for more info.
 *
 *
 * ## Usage
 *
 * This file provides both the interface and the implementation.
 * To instantiate the implementation,
 * ```
 *      #define SB_IMPLEMENTATION
 * ```
 * in *ONE* source file, before #including this file.
 *
 * example usage:
 * ```C
 * #define SB_IMPLEMENTATION
 * #include "sb.h"
 *
 * #include <stdio.h>
 *
 * int main() {
 *   sb_t sb = {0};
 *
 *   sb_append(&sb, "Hello");
 *   sb_appendf(&sb, " %s", "World");
 *
 *   printf(PRI_SB "\n", SB_FMT(sb)); // Hello World
 *
 *   sb_free(&sb);
 * }
 * ```
 *
 *
 * The library can also be compiled as its own translation unit.
 * For example, with gcc:
 * ```console
 * $ gcc -DSB_IMPLEMENTATION -x c -c sb.h 
 * ```
 *
 *
 * ## Config Parameters
 *
 * This library offers presonalization through macros.
 * Before #include "ss.h" you can define macros to customize the library behavior:
 *   - `SB_GROWTH_FACTOR`: dictates how much memory is allocated each time the 
  *                        vector grows.
 *                         (defaults to 2)
 *   - `SB_DEFAULT_SIZE`:  can be used to alter default initial allocation size 
 *                         (defaults to 128)
 *   - `SB_DEF`:           can be used to alter function declarations (e.g. `static
 *                         inline`, `extern "C"`, etc.)
 *                         (defaults to nothing)
 *   - `SB_ALLOC`:         can be use to define a custom allocator
 *                         (defaults to calloc/HeapAlloc)
 *   - `SB_REALLOC`:       can be use to define a custom allocator
 *                         (defaults to realloc/HeapReAlloc)
 *   - `SB_FREE`:          can be use to define a custom allocator
 *                         (defaults to free/HeapFree)
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


#ifndef SB_H
#define SB_H

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else

#include <stdlib.h>

#endif

#if defined(__GNUC__) || defined(__clang__)
#define SB_FMT_PRINTF(fmt, first_vararg) __attribute__ ((format (printf, fmt, first_vararg)))
#else
#define SB_FMT_PRINTF(...)
#endif

/**
 * The string builder.
 * @param data pointer to the data
 * @param len length of the builder: how many characters
 *            have been added to the builder
 * @param cap number of bytes of reserved memory
 */
typedef struct {
  char*  data;
  size_t len;
  size_t cap;
} sb_t;

// lib config

#ifndef SB_DEFAULT_SIZE
#define SB_DEFAULT_SIZE  128
#endif

#ifndef SB_GROWTH_FACTOR
#define SB_GROWTH_FACTOR 2
#endif

#ifndef SB_DEF
#define SB_DEF 
#endif

#ifndef SB_ALLOC
#if defined(_WIN32)
#define SB_ALLOC(count, size) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (count) * (size))
#else
#define SB_ALLOC(count, size) calloc(count, size)
#endif
#endif

#ifndef SB_REALLOC
#if defined(_WIN32)
#define SB_REALLOC(ptr, count, size) HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (ptr), (count) * (size))
#else
#define SB_REALLOC(ptr, count, size) realloc((ptr), (count) * (size))
#endif
#endif

#ifndef SB_FREE
#if defined(_WIN32)
#define SB_FREE(ptr) HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, (ptr))
#else
#define SB_FREE(ptr) free((ptr))
#endif
#endif

////////////


/**
 * inttypes-style format string placeholder
 */
#define PRI_SB "%.*s"

/**
 * Format string argument of PRI_SB
 * @param sb string builder
 */
#define SB_FMT(sb) (int)(sb).len, (sb).data


/**
 * Reserves required memory for the sb.
 * The amount specified in the size parameter will be rounded to
 * the next power of 2.
 * Will fail if the size requested is less than the capacity of
 * the sb (meaning no shrinks are permitted) or the requested
 * size is 0.
 * @param sb pointer to sb_t structure
 * @param size requesetd size to allocate
 * @return amount of size allocated. If 0 error.
 */
SB_DEF size_t sb_reserve(sb_t *sb, size_t size);

/**
 * Appends n characters to a sb.
 * @param sb pointer to sb_t structure
 * @param str const char* pointing to the string to append
 * @param len amount of chars to append to sb
 * @return If >= 0 the number of chars appended, if < 0 error.
 */
SB_DEF int sb_n_append(sb_t *sb, const char *str, size_t len);

/**
 * Appends a null-terminated string to sb. (null terminator
 * will get discarded)
 * @param sb pointer to sb_t structure
 * @param str const char* pointing to the string to append
 * @return If >= 0 the number of chars appended, if < 0 error.
 */
SB_DEF int sb_append(sb_t *sb, const char *str);

/**
 * Appends a null-terminated string to sb.
 * @param sb pointer to sb_t structure
 * @param str const char* pointing to the string to append
 * @return If >= 0 the number of chars appended, if < 0 error.
 */
SB_DEF int sb_appendz(sb_t *sb, const char *str);

/**
 * Appends a formatted string to sb
 * @param sb pointer to sb_t structure
 * @param fmt const char* pointing to a printf-style format string
 * @param ... variadic arguments
 * @return If >= 0 the number of chars appended, if < 0 error.
 */
SB_FMT_PRINTF(2, 3) SB_DEF int sb_appendf(sb_t *sb, const char* fmt, ...);

/**
 * Reads contents of a file into sb. Resets sb.
 * @param filename path of the file to read
 * @param sb pointer to sb_t structure that will hold
 * the file contents
 * @return If >= 0 the number of chars read, if < 0 error.
 */
SB_DEF int sb_read_file(const char *filename, sb_t* sb);

/**
 * Writes sb to a file
 * @param filename path of the file to write
 * @param sb pointer to sb_t structure
 * @return If >= 0 the number of chars written, if < 0 error.
 */
SB_DEF int sb_write_to_file(const char *filename, sb_t* sb);

/**
 * Appends sb to a file
 * @param filename path of the file to write
 * @param sb pointer to sb_t structure
 * @return If >= 0 the number of chars read, if < 0 error.
 */
SB_DEF int sb_append_to_file(const char *filename, sb_t* sb);

/**
 * Concats the contents of a sb to another.
 * @param a pointer to sb_t structure that will be extended
 * with the contents of b.
 * @param b pointer to sb_t structure. Will not get reset or freed.
 * @return If >= 0 the number of chars appended, if < 0 error.
 */
SB_DEF int sb_concat(sb_t* a, sb_t* b);

/**
 * Resets the string builder without freeing memory.
 * @param sb pointer to string builder
 */
#define sb_reset(sb) (sb)->len = 0

/**
 * Clears the string guilder, freeing all allocated memory and
 * resetting fields to 0.
 * @param sb pointer to sb_t structure
 */
SB_DEF void sb_free(sb_t* sb);

#endif

#ifdef SB_IMPLEMENTATION

#if defined(_WIN32)
#define SB_MEMCOPY(dst, src, size) CopyMemory((BYTE*)(dst), (BYTE*)(src), (size))
#else
#define SB_MEMCOPY(dst, src, size) memcpy((dst), (src), (size))
#endif

// next power of 2 
static inline size_t ceil_p2(size_t size) {
  if (size <= 1) return 1;
  if (size & (size - 1)) {
#if defined(__clang__) || defined(__GNUC__)
    size_t leading_zeros = sizeof(size) == 8 ? __builtin_clzll(size - 1) : __builtin_clz(size - 1);
    if (leading_zeros == 0) return 0;
    return (size_t)1 << (sizeof(size)*8 - leading_zeros);
#else
    size--;
    for (short i = 1; i < sizeof(size)*8; i <<= 1)
      size |= size >> i;
    return size + 1;
#endif
  }
  return size;
}

SB_DEF size_t sb_reserve(sb_t *sb, size_t size) {
  if (size == 0) return 0;
  if (size <= sb->cap) return sb->cap;

  size_t size_new = ceil_p2(size);
  if (size_new < size) return 0; // overflow detected
  size = size_new;

  void* items = sb->data
                ? SB_REALLOC(sb->data, size, 1) 
                : SB_ALLOC(size, 1);

  if (!items) return 0;

  sb->data = items;
  sb->cap = size;
  return size;
}

SB_DEF int sb_n_append(sb_t *sb, const char *str, size_t len) {
  if (sb->cap == 0)
    if (sb_reserve(sb, SB_DEFAULT_SIZE) == 0) return -1;

  // check overflow
  if (len > ((size_t)-1) - sb->len) return -1;

  size_t size = sb->cap;
  while (len + sb->len >= size) { // = avoids realloc at next call
    size_t size_new = size * SB_GROWTH_FACTOR;
    if (size_new < size) return -1; // overflow detected
    size = size_new;
  }

  if (sb_reserve(sb, size) == 0) return -1;
  SB_MEMCOPY(sb->data + sb->len, str, len);

  sb->len += len;
  return (int)len;
}

SB_DEF int sb_append(sb_t *sb, const char *str) {
  return sb_n_append(sb, str, strlen(str));
}

SB_DEF int sb_appendln(sb_t *sb, const char *str) {
  int l = sb_n_append(sb, str, strlen(str) + 1);
  if (l < 0) return -1;
  sb->data[sb->len-1] = '\n';
  return l;
}

SB_DEF int sb_appendz(sb_t *sb, const char *str) {
  return sb_n_append(sb, str, strlen(str) + 1);
}

SB_DEF int sb_appendf(sb_t *sb, const char* fmt, ...) {
  va_list args, args_copy = { 0 };

  va_start(args, fmt);
  va_copy(args_copy, args);

#ifdef _WIN32
  int n = _vscprintf(fmt, args);
#else
  int n = vsnprintf(NULL, 0, fmt, args);
#endif
  va_end(args);

  if (n < 0) {
    va_end(args_copy);
    return n;
  }

  // check overflow
  if ((size_t)n + 1 > ((size_t)-1) - sb->len) return -1;

  if (sb_reserve(sb, sb->len + n + 1) == 0) return -1;
  char *dest = sb->data + sb->len;

  int len = vsnprintf(dest, n + 1, fmt, args_copy);
  va_end(args_copy);

  if (len > 0) sb->len += len;
  return len;
}

// NOTE: resets the sb and overwrites items
SB_DEF int sb_read_file(const char *filename, sb_t* sb) {
  int read = 0;

  sb_reset(sb);

#ifdef _WIN32
  HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if (hFile == INVALID_HANDLE_VALUE) return -1;

  LARGE_INTEGER liFileSize = { 0 };
  if (!GetFileSizeEx(hFile, &liFileSize)) return -1;

  if (sb_reserve(sb, liFileSize.QuadPart * sizeof(*sb->data)) == 0) return -1;
  if (!ReadFile(hFile, (LPVOID)sb->data, (DWORD)sb->cap, (LPDWORD)&read, NULL)) {
    sb_free(sb);
    return -1;
  }

  CloseHandle(hFile);
#else
  FILE* fp = fopen(filename, "r");
  if (!fp) return -1;

  fseek(fp , 0 , SEEK_END);
  size_t size = ftell(fp);
  rewind(fp);

  if (sb_reserve(sb, size) == 0) return -1;
  read = fread(sb->data, sizeof(*sb->data), size, fp);

  fclose(fp);
#endif

  sb->len = read;
  return read;
}

SB_DEF int sb_write_to_file(const char *filename, sb_t* sb) {
  int written = 0;

#ifdef _WIN32
  HANDLE hFile = CreateFileA(filename, FILE_WRITE_DATA, 0, NULL, CREATE_ALWAYS, 0, NULL);
  if (hFile == INVALID_HANDLE_VALUE) return -1;

  if (!WriteFile(hFile, (LPCVOID)sb->data, (DWORD)sb->len, (LPDWORD)&written, NULL)) return -1;

  CloseHandle(hFile);
#else
  FILE* fp = NULL;
  fp = fopen(filename, "w");
  if (!fp) return -1;

  written = fprintf(fp, "%.*s", SB_FMT(*sb));

  fclose(fp);
#endif
  return written;
}


SB_DEF int sb_append_to_file(const char *filename, sb_t* sb) {
  int written = 0;

#ifdef _WIN32
  HANDLE hFile = CreateFileA(filename, FILE_APPEND_DATA , 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) return -1;

  if (!WriteFile(hFile, (LPCVOID)sb->data, (DWORD)sb->len, (LPDWORD)&written, NULL)) return -1;

  CloseHandle(hFile);
#else
  FILE* fp = NULL;
  fp = fopen(filename, "a");
  if (!fp) return -1;

  written = fprintf(fp, PRI_SB, SB_FMT(*sb));

  fclose(fp);
#endif
  return written;
}

int sb_concat(sb_t* a, sb_t* b) {
  return sb_n_append(a, b->data, b->len);
}

SB_DEF void sb_free(sb_t* sb) {
  SB_FREE(sb->data);

  sb->data = NULL;
  sb->len = 0;
  sb->cap  = 0;
}

#endif
