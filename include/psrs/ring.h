#ifndef RING_H
#define RING_H

#include "macro.h"

#include <stddef.h>

typedef void *(*ring_alloc_t)(size_t size);
typedef void (*ring_free_t)(void *ptr);

struct ring_node {
        void *data;
        struct ring_node *next;
};

struct ring_iter {
        struct ring_node *pos;
};

struct ring {
        size_t length;
        ring_alloc_t alloc;
        ring_free_t clean;
        struct ring_node *head;
        struct ring_node *pos;
};

int ring_init(struct ring **self,
              const size_t length,
              ring_alloc_t alloc,
              ring_free_t clean);
int ring_add(struct ring *self, const void *data, const size_t size);
int ring_length(const struct ring *self, size_t *length);
int ring_destroy(struct ring **self);

int ring_iter_init(struct ring_iter **self, const struct ring *circle);
int ring_iter_walk(struct ring_iter *self, void **data);
int ring_iter_destroy(struct ring_iter **self);

#ifdef RING_ONLY
static int ring_node_init_(struct ring_node **self, ring_alloc_t alloc);
static int ring_node_fill_(struct ring_node *self,
                           const void *data,
                           const size_t size,
                           ring_alloc_t alloc,
                           ring_free_t clean);
static int ring_node_destroy_(struct ring_node **self, ring_free_t clean);
#endif

#endif /* RING_H */
