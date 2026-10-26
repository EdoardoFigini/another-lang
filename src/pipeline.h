#ifndef PIPELINE_H
#define PIPELINE_H

#include <stdbool.h>

#import "efh/arena.h"

#import "parser.h"

typedef struct {
  arena_t arena;
} pipeline_t;

typedef struct _queue_node {
  ast_node_t* node; 
  struct _queue_node* next;
} queue_node_t;

typedef struct {
  queue_node_t* head;
  queue_node_t* tail;
  size_t len;
} queue_t;

bool          q_is_empty(queue_t* q);
void          q_enqueue(queue_t* q, pipeline_t* p, ast_node_t* node);
queue_node_t* q_dequeue(queue_t* q);

#endif // PIPELINE_H
