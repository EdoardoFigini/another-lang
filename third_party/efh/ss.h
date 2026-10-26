/**
 * ss.h - v1.1.0 - Copyright (c) 2026 Edoardo Figini
 * 
 * # String Slices in C
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
 *      #define SS_IMPLEMENTATION
 * ```
 * in *ONE* source file, before #including this file.
 *
 * example usage:
 * ```
 * #define SS_IMPLEMENTATION
 * #include "ss.h"
 *
 * #include <stdio.h>
 *
 * int main() {
 *   ss_t list = SS("apple, banana, cherry");
 *
 *   for (size_t i=0; i < 3; i++) {
 *     ss_t s = ss_tok(&list, ',');
 *     s = ss_trim(s);
 *     
 *     printf(PRI_SS "\n", SS_FMT(s));
 *   }
 * }
 * ```
 *
 *
 * The library can also be compiled as its own translation unit.
 * For example, with gcc:
 * ```console
 * $ gcc -DSS_IMPLEMENTATION -x c -c ss.h 
 * ```
 *
 *
 * ## Config Parameters
 *
 * This library offers presonalization through macros.
 * Before #include "ss.h" you can define macros to customize the library behavior:
 *   - `SS_DEF`: can be used to alter function declarations (e.g. `static inline`, 
 *               `extern`, etc.)
 *               (defaults to nothing)
 *
 * 
 * ## Changelog
 *
 *    1.1.0 (2026-10-16) New constructors 
 *                       - Add ss constructor from ptr and length
 *                       - Add SS static constructor from string literal
 *                       - Add ss_from_extents constructor from pointer difference
 *                       New manipulation functions accepting a function pointer
 *                       for matching
 *                       - Add ss_advance_while
 *                       - Add ss_advance_while_in_place
 *                       - Add ss_tok_while
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


#ifndef SS_H
#define SS_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h> // for atof
#else
#include <stdlib.h>
#endif

// lib config

#ifndef SS_DEF
#define SS_DEF
#endif

//////////////

/**
 * The string slice.
 * @param data const pointer to the data
 *             (does not own the data, hence the const)
 * @param lne length of the slice: how many characters
 *            starting from data are in the view 
 */
typedef struct {
  const char* data;
  size_t len;
} ss_t;

/**
 * inttypes-style format string placeholder
 */
#define PRI_SS "%.*s"

/**
 * Format string argument of PRI_SS
 * @param s string slice
 */
#define SS_FMT(s) (int)(s).len, (s).data

/*
 * Creates a slice from poiter and size.  
 * @param data the buffer (that owns the data) 
 * @param len length of the view
 * @return string slice 
 */
SS_DEF ss_t ss(const char* data, size_t len);
/*
 * Creates a slice from a C-string literal 
 * (null terminated).  
 * @param str string literal
 */
#define SS(str) (ss_t){ .data = (str), .len = sizeof((str)) - 1 }
/*
 * Creates a slice from a C-string (null terminated).  
 * @param str the buffer (that owns the data) 
 * @return string slice 
 */
SS_DEF ss_t ss_from_cstr(const char* str);
/*
 * Creates a slice from a string builder (see sb.h)
 * @param sb the string builder (that owns the data) 
 * @return string slice 
 */
#define ss_from_sb(sb) \
  (ss_t){ .data = (sb).data, .len = (sb).len }
// NOTE: keep this as macro to avoid having to #include "sb.h"

/*
 * Creates a slice from start and end pointers.
 * `end` MUST be >= `start`.
 * Use this function carefully: taking the difference between
 * two pointers that do not point to contiguous memory may
 * induce problems.
 * @param start pointer to beginning of the string
 * @param end pointer to end of the string
 * @return string slice 
 */
SS_DEF ss_t ss_from_extents(const char* start, const char* end);

/**
 * Converts the initial portion of the slice to int.
 * @param s string slice (should start with a number)
 * @return converted integer
 */
SS_DEF int   ss_atoi(ss_t s);
/**
 * Converts the initial portion of the slice to float.
 * Due to implementation, the maximum length of the number
 * is 127 chars. Characters from the 128th onwards will
 * be truncated and not considered in the conversion.
 * @param s string slice (should start with a number)
 * @return converted floating point number
 * \warning Allocates memory for conversion
 */
SS_DEF float ss_atof(ss_t s);

/**
 * Returns a new slice advanced by the desired number of characters
 * @param s string slice
 * @param n (positive) number of chars to advance by
 * @return advanced slice (empty if n > s.len)
 */
SS_DEF ss_t ss_advance_by(ss_t s, size_t n);
/**
 * Returns a new slice advanced until the matching sequence is found 
 * @param s string slice
 * @param str C-string containig the matching target
 * @return advanced slice (empty if str was not found)
 */
SS_DEF ss_t ss_advance_until(ss_t s, const char* str);
/**
 * Returns a new slice advanced after the matching sequence is found 
 * @param s string slice
 * @param str C-string containig the matching target
 * @return advanced slice (empty if str was not found)
 */
SS_DEF ss_t ss_advance_after(ss_t s, const char* str);
/**
 * Returns a new slice advanced until the matching function
 * returns false 
 * @param s string slice
 * @param f matching function 
 * @return advanced slice (empty if str was not found)
 */
SS_DEF ss_t ss_advance_while(ss_t s, int (f)(int));

/**
 * Trims whitespace from the beginning of the slice.
 * @param s string slice
 * @return new trimmed string slice
 */
SS_DEF ss_t ss_trim_start(ss_t s);

/**
 * Trims whitespace from the end of the slice.
 * @param s string slice
 * @return new trimmed string slice
 */
SS_DEF ss_t ss_trim_end(ss_t s);

/**
 * Trims whitespace from both the beginning and the end of the slice.
 * @param s string slice
 * @return new trimmed string slice
 */
SS_DEF ss_t ss_trim(ss_t s);

/**
 * Tokenizes the slice by a delimiter character.
 * Advances the slice pointing to the next token, and returns the token found.
 * @param s pointer to the string slice to be tokenized
 * @param chr delimiter character
 * @return string slice containing the token
 */
SS_DEF ss_t ss_tok(ss_t* s, char chr);

/**
 * Tokenizes the slice while the matching function returns true. 
 * Advances the slice pointing to the next token, and returns the token found.
 * @param s pointer to the string slice to be tokenized
 * @param f matching function 
 * @return string slice containing the token
 */
SS_DEF ss_t ss_tok_while(ss_t* s, int (f)(int));

/**
 * Extracts a sub-slice from the given slice.
 * @param s string slice
 * @param start starting index (inclusive)
 * @param end ending index (exclusive)
 * @return new sub-slice
 */
SS_DEF ss_t ss_sub(ss_t s, size_t start, size_t end);

/**
 * Finds the index of the first occurrence of a character.
 * @param s string slice
 * @param chr character to find
 * @return index of the character, or s.len if not found
 */
SS_DEF size_t ss_index_of(ss_t s, char chr);

/**
 * Finds the index of the first occurrence of a C-string.
 * @param s string slice
 * @param str C-string to find
 * @return index of the first character of the substring, or s.len if not found
 */
SS_DEF size_t ss_index_of_cstr(ss_t s, const char* str);

// TODO: size_t ss_find(ss_t haystack, ss_t needle)

/**
 * Checks if two string slices are equal.
 * @param a first string slice
 * @param b second string slice
 * @return true if equal, false otherwise
 */
SS_DEF bool ss_eq(ss_t a, ss_t b);

// TODO: eq, starts_witch, ends_with, etc. ignore case

/**
 * Checks if a string slice starts with another slice.
 * @param s string slice
 * @param pre prefix slice to check
 * @return true if s starts with pre, false otherwise
 */
SS_DEF bool ss_starts_with(ss_t s, ss_t pre);

/**
 * Checks if a string slice ends with another slice.
 * @param s string slice
 * @param suf suffix slice to check
 * @return true if s ends with suf, false otherwise
 */
SS_DEF bool ss_ends_with(ss_t s, ss_t suf);

/**
 * Checks if a string slice is equal to a C-string.
 * @param a string slice
 * @param b C-string
 * @return true if equal, false otherwise
 */
SS_DEF bool ss_eq_cstr(ss_t a, const char* b);

/**
 * Checks if a string slice starts with a C-string.
 * @param s string slice
 * @param pre C-string prefix to check
 * @return true if s starts with pre, false otherwise
 */
SS_DEF bool ss_starts_with_cstr(ss_t s, const char* pre);

/**
 * Checks if a string slice ends with a C-string.
 * @param s string slice
 * @param suf C-string suffix to check
 * @return true if s ends with suf, false otherwise
 */
SS_DEF bool ss_ends_with_cstr(ss_t s, const char* suf);

/**
 * Advances a slice in place by a given number of characters.
 * @param s pointer to string slice
 * @param n number of characters to advance
 */
SS_DEF void ss_advance_by_in_place(ss_t *s, size_t n);

/**
 * Advances a slice in place until the matching sequence is found.
 * @param s pointer to string slice
 * @param str C-string containing the matching target
 * @return true if target was found, false otherwise
 */
SS_DEF bool ss_advance_until_in_place(ss_t* s, const char* str);

/**
 * Advances a slice in place after the matching sequence is found.
 * @param s pointer to string slice
 * @param str C-string containing the matching target
 * @return true if target was found, false otherwise
 */
SS_DEF bool ss_advance_after_in_place(ss_t* s, const char* str);
/**
 * Advances a slice in place until the matching function
 * returns false 
 * @param s pointer to string slice
 * @param f matching function 
 * @return advanced slice (empty if str was not found)
 */
SS_DEF bool ss_advance_while_in_place(ss_t* s, int (f)(int));

/**
 * Trims whitespace from the beginning of the slice in place.
 * @param s pointer to string slice
 */
SS_DEF void ss_trim_start_in_place(ss_t* s);

/**
 * Trims whitespace from the end of the slice in place.
 * @param s pointer to string slice
 */
SS_DEF void ss_trim_end_in_place(ss_t* s);

/**
 * Trims whitespace from both the beginning and the end of the slice in place.
 * @param s pointer to string slice
 */
SS_DEF void ss_trim_in_place(ss_t* s);

#endif // SS_H

#ifdef SS_IMPLEMENTATION

#include <string.h>
#include <ctype.h>

SS_DEF ss_t ss(const char* data, size_t len) {
  return (ss_t){
    .data = data,
    .len = len,
  };
}

SS_DEF ss_t ss_from_cstr(const char* str) {
  return (ss_t){
    .data = str,
    .len = strlen(str),
  };
}

SS_DEF ss_t ss_from_extents(const char* start, const char* end) {
  return (ss_t){
    .data = start,
    .len = (end - start),
  };
}

static inline int sized_atoi(const char* data, size_t size) {
  int n = 0;
  int sign = 1;
  if (size == 0) return 0;
  for (size_t i=0; i < size; i++, data++) {
    if (i==0 && *data == '-') { sign = -1; continue; }
    if (!isdigit(*data)) return n;
    n = (n * 10) + (*data - '0');
  }
  return n * sign;
}

SS_DEF int ss_atoi(ss_t s) {
  ss_trim_start_in_place(&s);
  return sized_atoi(s.data, s.len);
}

SS_DEF float ss_atof(ss_t s) {
  char buf[128] = { 0 };
  ss_trim_start_in_place(&s);
  size_t len = s.len < sizeof(buf) - 1 ? s.len : sizeof(buf) - 1;

  memcpy(buf, s.data, len);
  float res = atof(buf);

  return res;
}

SS_DEF void ss_trim_start_in_place(ss_t* s) {
  while(s->len > 0 && isspace((unsigned char)*s->data)) {
    s->len--;
    s->data++;
  }
}

SS_DEF void ss_trim_end_in_place(ss_t* s) {
  while(s->len > 0 && isspace((unsigned char)s->data[s->len - 1])) {
    s->len--;
  }
}

SS_DEF void ss_trim_in_place(ss_t* s) {
  ss_trim_start_in_place(s);
  ss_trim_end_in_place(s);
}

SS_DEF ss_t ss_trim_start(ss_t s) {
  ss_t out = s;
  ss_trim_start_in_place(&out);
  return out;
}

SS_DEF ss_t ss_trim_end(ss_t s) {
  ss_t out = s;
  ss_trim_end_in_place(&out);
  return out;
}

SS_DEF ss_t ss_trim(ss_t s) {
  ss_t out = s;
  ss_trim_start_in_place(&out);
  ss_trim_end_in_place(&out);
  return out;
}

SS_DEF ss_t ss_advance_by(ss_t s, size_t n) {
  if (n > s.len) n = s.len;
  return (ss_t) {
    .data = s.data + n,
    .len = s.len - n,
  };
}

SS_DEF ss_t ss_advance_until(ss_t s, const char* str) {
  while(!ss_starts_with_cstr(s, str) && s.len) { s.data++; s.len--; }
  return s; 
}

SS_DEF ss_t ss_advance_after(ss_t s, const char* str) {
  s = ss_advance_until(s, str);
  if (s.len <= 0) return (ss_t){ 0 };
  s.data += strlen(str);
  s.len -= strlen(str);
  return s;
}

SS_DEF ss_t ss_advance_while(ss_t s, int (f)(int)) {
  while(s.len && f(*s.data)) { s.data++; s.len--; }
  return s; 
}

SS_DEF void ss_advance_by_in_place(ss_t *s, size_t n) {
  if (n > s->len) n = s->len;
  s->data += n;
  s->len -= n;
}

SS_DEF bool ss_advance_until_in_place(ss_t* s, const char* str) {
  while(!ss_starts_with_cstr(*s, str) && s->len) { s->data++; s->len--; }
  return s->len;
}

SS_DEF bool ss_advance_after_in_place(ss_t* s, const char* str) {
  if(!ss_advance_until_in_place(s, str)) return false;
  s->data += strlen(str);
  s->len -= strlen(str);
  return true;
}

SS_DEF bool ss_advance_while_in_place(ss_t* s, int (f)(int)) {
  while(s->len && f(*s->data) && s->len) { s->data++; s->len--; }
  return s->len; 
}

SS_DEF bool ss_eq(ss_t a, ss_t b) {
  return a.len == b.len && ((a.len != 0 && memcmp(a.data, b.data, a.len) == 0) || !a.len);
}

SS_DEF bool ss_eq_cstr(ss_t a, const char* b) {
  return ss_eq(a, ss_from_cstr(b)); 
}

SS_DEF bool ss_starts_with(ss_t s, ss_t pre) {
  return pre.len == 0 || (pre.len <= s.len && memcmp(s.data, pre.data, pre.len) == 0);
}

SS_DEF bool ss_starts_with_cstr(ss_t s, const char* pre) {
  return ss_starts_with(s, ss_from_cstr(pre));
}

SS_DEF bool ss_ends_with(ss_t s, ss_t suf) {
  return suf.len == 0 || (suf.len <= s.len && memcmp(s.data + (s.len - suf.len), suf.data, suf.len) == 0);
}

SS_DEF bool ss_ends_with_cstr(ss_t s, const char* suf) {
  return ss_ends_with(s, ss_from_cstr(suf));
}

SS_DEF ss_t ss_tok(ss_t* s, char chr) {
  ss_t res = *s;

  size_t idx = ss_index_of(*s, chr);

  ss_advance_by_in_place(s, idx + 1);

  res.len = idx;

  return res;
}

SS_DEF ss_t ss_tok_while(ss_t* s, int (f)(int)) {
  ss_t og = *s;
  ss_advance_while_in_place(s, f);
  return ss_from_extents(og.data, s->data);
}

SS_DEF ss_t ss_sub(ss_t s, size_t start, size_t end) {
  end   = end < s.len ? end : s.len;
  start = start < s.len ? (start > 0 ? start : 0) : s.len;

  return (ss_t) {
    .data = s.data + start,
    .len = (end > start) ? (end - start) : 0,
  };
}

SS_DEF size_t ss_index_of(ss_t s, char chr) {
  for (size_t i =0; i < s.len; i++) {
    if(s.data[i] == chr) return i;
  }
  return s.len;
}

// TODO: rolling hash algo https://youtu.be/qS-4gVdKy7I?t=2021
SS_DEF size_t ss_index_of_cstr(ss_t s, const char* str) {
  for (size_t i =0; i < s.len; i++) {
    if(ss_starts_with_cstr(ss_advance_by(s, i), str)) return i;
  }
  return s.len;
}

#endif // SS_IMPLEMENTATION
