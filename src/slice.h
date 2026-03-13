#ifndef SLICE_H
#define SLICE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <stdlib.h>
#endif

typedef struct {
  const char* data;
  size_t size;
} slice_t;

#define SLICE_FMT(s) (int)(s).size, (s).data

typedef struct {
  slice_t* items;
  size_t   count;
  size_t   capacity;
} slicearr_t;


slice_t slice_from_cstr(const char* str);
// keep this as macro to avoid having to #include "sb.h"
#define slice_from_sb(sb) \
  (slice_t){ .data = (sb).items, .size = (sb).count }

int   slice_atoi(slice_t s, size_t max_len);
float slice_atof(slice_t s, size_t max_len);

slice_t slice_advance_by(slice_t s, size_t n);
slice_t slice_advance_until(slice_t s, const char* str);
slice_t slice_advance_after(slice_t s, const char* str);

slice_t slice_trim_start(slice_t s);
slice_t slice_trim_end(slice_t s);
slice_t slice_trim(slice_t s);

slice_t slice_sub(slice_t s, size_t start, size_t end);
size_t slice_index_of(slice_t s, char chr);
size_t slice_index_of_cstr(slice_t s, const char* str);

bool slice_eq(slice_t a, slice_t b);
bool slice_starts_with(slice_t s, slice_t pre);
bool slice_ends_with(slice_t s, slice_t suf);

bool slice_eq_cstr(slice_t a, const char* b);
bool slice_starts_with_cstr(slice_t s, const char* pre);
bool slice_ends_with_cstr(slice_t s, const char* suf);


void slice_advance_by_in_place(slice_t *s, size_t n);
bool slice_advance_until_in_place(slice_t* s, const char* str);
bool slice_advance_after_in_place(slice_t* s, const char* str);

void slice_trim_start_in_place(slice_t* s);
void slice_trim_end_in_place(slice_t* s);
void slice_trim_in_place(slice_t* s);


void slice_split(slice_t s, const char* sep, unsigned int limit, slicearr_t* sa);

#endif // SLICE_H

#ifdef SLICE_IMPLEMENTATION

#include <string.h>
#include <ctype.h>

slice_t slice_from_cstr(const char* str) {
  return (slice_t){
    .data = str,
    .size = strlen(str),
  };
}

static inline void slicearr_push(slicearr_t* sa, const char* data, size_t size) {
  if (sa->count >= sa->capacity) {
    size_t cap = sa->capacity * 2;
    size_t size = cap * sizeof(*sa->items);

#ifdef _WIN32
    sa->items = sa->items
                ? HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sa->items, size)
                : HeapAlloc(GetProcessHeap(),   HEAP_ZERO_MEMORY, size);
#else
    sa->items = realloc(sa->items, size);
#endif

    assert(sa->items && "Failed to realloc.");
    sa->capacity = cap;
  }
  
  sa->items[sa->count++] = (slice_t){
    .data = data,
    .size = size,
  };
}

slice_t slice_advance_by(slice_t s, size_t n) {
  return (slice_t) {
    .data = s.data + n,
    .size = s.size - n,
  };
}

void slice_advance_by_in_place(slice_t *s, size_t n) {
  s->data += n;
  s->size -= n;
}

void slice_split(slice_t s, const char* sep, unsigned int limit, slicearr_t* sa) {
  if (!s.data) return;
  if (!sa) return;
  size_t start = 0;
  size_t sep_len = strlen(sep);
  for (size_t i=0; i < s.size && (limit == 0 || sa->count < limit); i++) {
    if (s.size - i >= sep_len && memcmp(s.data + i, sep, sep_len) == 0) {
      slicearr_push(sa, s.data + start, i - start);
      i += sep_len - 1;
      start = i + 1;
    }
  }
  slicearr_push(sa, s.data + start, s.size - start);
}

static inline int sized_atoi(const char* data, size_t size) {
  int n = 0;
  int sign = 1;
  if (size == 0) return 0;
  for (size_t i=0; i < size; i++, data++) {
    if (i==0 && *data == '-') { sign = -1; continue; }
    if (!isdigit(*data)) return 0;
    n = (n * 10) + (*data - '0');
  }
  return n * sign;
}

int slice_atoi(slice_t s, size_t max_len) {
  return sized_atoi(s.data, max_len > 0 ? max_len : s.size);
}

float slice_atof(slice_t s, size_t max_len) {
  size_t len = max_len > 0 ? max_len : s.size;
  char* tmp = calloc(len + 1, sizeof(*s.data));
  memcpy(tmp, s.data, len * sizeof(*s.data));
  return strtof(tmp, NULL);
}

void slice_trim_start_in_place(slice_t* s) {
  while(s->size > 0 && isspace(*s->data)) {
    s->size--;
    s->data++;
  }
}

void slice_trim_end_in_place(slice_t* s) {
  while(s->size > 0 && isspace(s->data[s->size - 1])) {
    s->size--;
  }
}

void slice_trim_in_place(slice_t* s) {
  slice_trim_start_in_place(s);
  slice_trim_end_in_place(s);
}

slice_t slice_trim_start(slice_t s) {
  slice_t out = s;
  slice_trim_start_in_place(&out);
  return out;
}

slice_t slice_trim_end(slice_t s) {
  slice_t out = s;
  slice_trim_end_in_place(&out);
  return out;
}

slice_t slice_trim(slice_t s) {
  slice_t out = s;
  slice_trim_start_in_place(&out);
  slice_trim_end_in_place(&out);
  return out;
}

slice_t slice_advance_until(slice_t s, const char* str) {
  while(!slice_starts_with_cstr(s, str) && s.size) { s.data++; s.size--; }
  return s; 
}

slice_t slice_advance_after(slice_t s, const char* str) {
  s = slice_advance_until(s, str);
  if (s.size <= 0) return (slice_t){ 0 };
  s.data += strlen(str);
  s.size -= strlen(str);
  return s;
}

bool slice_advance_until_in_place(slice_t* s, const char* str) {
  while(!slice_starts_with_cstr(*s, str) && s->size) { s->data++; s->size--; }
  return !s->data;
}

bool slice_advance_after_in_place(slice_t* s, const char* str) {
  if(slice_advance_until_in_place(s, str)) return 1;
  s->data += strlen(str);
  s->size -= strlen(str);
  return 0;
}

bool slice_eq(slice_t a, slice_t b) {
  return a.size == b.size && a.size != 0 && memcmp(a.data, b.data, a.size) == 0;
}

bool slice_eq_cstr(slice_t a, const char* b) {
  return slice_eq(a, slice_from_cstr(b)); 
}

bool slice_starts_with(slice_t s, slice_t pre) {
  return pre.size != 0 && pre.size <= s.size && memcmp(s.data, pre.data, pre.size) == 0;
}

bool slice_starts_with_cstr(slice_t s, const char* pre) {
  return slice_starts_with(s, slice_from_cstr(pre));
}

bool slice_ends_with(slice_t s, slice_t pre) {
  return pre.size <= s.size && memcmp(s.data + (s.size - 1 - pre.size), pre.data, pre.size) == 0;
}

bool slice_ends_with_cstr(slice_t s, const char* pre) {
  return slice_ends_with(s, slice_from_cstr(pre));
}

slice_t slice_sub(slice_t s, size_t start, size_t end) {
  end   = end < s.size ? end : s.size - 1;
  start = start > 0 ? start : 0;

  return (slice_t) {
    .data = s.data + start,
    .size = end,
  };
}

size_t slice_index_of(slice_t s, char chr) {
  for (size_t i =0; i < s.size; i++) {
    if(s.data[i] == chr) return i;
  }
  return s.size;
}

size_t slice_index_of_cstr(slice_t s, const char* str) {
  for (size_t i =0; i < s.size; i++) {
    if(slice_starts_with_cstr(s, str)) return i;
  }
  return s.size;
}

#endif // SLICE_IMPLEMENTATION
