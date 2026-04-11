#ifndef MACROS_H
#define MACROS_H

#define UNREACHABLE(fmt, ...) \
  do { \
    fprintf(stderr, "[FATAL] UNREACHABLE\n  %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);\
    abort();\
  } while(0);

#define TODO(fmt, ...) \
  do { \
    fprintf(stderr, "[FATAL] UNIMPLEMENTED\n  %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);\
    abort();\
  } while(0);

#define DLLIST_ADD(obj, head)    \
  do {                           \
    (obj)->next = (head);        \
    (head) = (obj);              \
    if((obj)->next)              \
      (obj)->next->prev = (obj); \
    (obj)->prev = NULL;          \
  } while (0);

#define MIN(a, b) \
  ((a) > (b) ? (b) : (a))

#define MAX(a, b) \
  ((a) < (b) ? (b) : (a))

#if defined(__GNUC__) || defined(__clang__)
#ifdef __MINGW_PRINTF_FORMAT
#define FMT_PRINTF(fmt, first_vararg) __attribute__ ((format (__MINGW_PRINTF_FORMAT, fmt, first_vararg)))
#else
#define FMT_PRINTF(fmt, first_vararg) __attribute__ ((format (printf, fmt, first_vararg)))
#endif
#else
#define FMT_PRINTF(...)
#endif

#endif
