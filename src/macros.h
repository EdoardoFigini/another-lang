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

#define DIAGF(p, lvl, fmt, ...) \
  do { \
    char* line_start = (char*)(p)->tokens.items[(p)->current].start; \
    char* line_end   = (char*)(p)->tokens.items[(p)->current].start; \
    while(line_start != (p)->source.items && *(line_start-1) != '\n') line_start--; \
    while(line_end   != &da_last(&(p)->source) && *line_end != '\n')  line_end++;   \
    int line_len = line_end - line_start;\
    fprintf( \
        stderr, \
        "[" #lvl "]\n  %zu:%zu: " fmt "\n    %.*s\n    %*s^\n\n", \
        (p)->line, (p)->col, \
        ## __VA_ARGS__, \
        line_len, line_start, \
        (int)(p)->col-1, "" \
    );\
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

#endif
