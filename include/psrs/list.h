#ifndef LIST_H
#define LIST_H

#include "macro.h"

#include <stddef.h>

struct node {
        long val;
        struct node *next;
};

struct list {
        size_t size;
        struct node *head;
        struct node *current;
};


int list_init(struct list **self);
int list_add(struct list *self, const long value);
int list_copy(struct list *self, long array[const]);
int list_destroy(struct list **self);
#endif /* LIST_H */
