#include <stddef.h>
#include <stdbool.h>

#include "pipeline.h"

#include "parser.h"

static queue_node_t* make_node(pipeline_t* p, ast_node_t* node) {
  return arena_push(&p->arena, (queue_node_t){ .node = node  });
}

bool q_is_empty(queue_t* q) {
  return q->head == NULL;
}

void q_enqueue(queue_t* q, pipeline_t* p, ast_node_t* node) {
  queue_node_t* n = make_node(p, node); 

  if (q_is_empty(q)) {
    q->head = n;
    q->tail = n;
  } else {
    q->tail->next = n;
    q->tail       = n;
  }

  q->len++;
}

queue_node_t* q_dequeue(queue_t* q) {
  if (q_is_empty(q)) return NULL;

  queue_node_t* n = q->head;

  q->head = q->head->next;
  n->next = NULL;

  q->len--;

  return n;
}
