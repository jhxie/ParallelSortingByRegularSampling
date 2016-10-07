#include "psrs/list.h"

#include <errno.h>
#include <stdlib.h>

int list_init(struct list **self)
{
        struct list *temp = NULL;

        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        temp = (struct list *)malloc(sizeof(struct list));

        if (NULL == temp) {
                return -1;
        }

        temp->size = 0U;
        temp->head = NULL;
        temp->current = NULL;

        *self = temp;
        return 0;
}

int list_add(struct list *self, const long value)
{
        struct node *temp_node = NULL;
        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        temp_node = (struct node *)malloc(sizeof(struct node));

        if (NULL == temp_node) {
                return -1;
        }

        temp_node->val = value;
        temp_node->next = NULL;

        if (NULL == self->head) {
                self->head = temp_node;
                self->current = temp_node;
        } else {
                self->current->next = temp_node;
                self->current = temp_node;
        }

        self->size += 1U;
        return 0;
}

/*
 * NOTE:
 * It is callers' responsibility to ensure the array has enough memory for
 * storing sizeof(long) * self->size number of bytes.
 */
int list_copy(struct list *self, long array[const])
{
        struct node *temp_node = NULL;

        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        temp_node = self->head;
        for (size_t i = 0; i < self->size; ++i, temp_node = temp_node->next) {
                array[i] = temp_node->val;
        }
        return 0;
}

int list_destroy(struct list **self)
{
        struct list *temp = NULL;
        struct node *temp_node = NULL;

        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        temp = *self;

        if (NULL == temp->head) {
                free(temp);
                *self = NULL;
                return 0;
        }

        while (NULL != temp->head) {
                temp_node = temp->head->next;
                free(temp->head);
                temp->head = temp_node;
        }
        free(*self);
        *self = NULL;
        return 0;
}
