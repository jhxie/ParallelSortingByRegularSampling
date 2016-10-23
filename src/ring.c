#include "psrs/macro.h"
#define RING_ONLY
#include "psrs/ring.h"
#undef RING_ONLY

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int ring_init(struct ring **self,
              const size_t length,
              ring_alloc_t alloc,
              ring_free_t clean)
{
        struct ring *ring = NULL;
        struct ring_node *ring_node = NULL;

        if (NULL == self || 0U == length) {
                errno = EINVAL;
                return -1;
        }

        if (NULL == alloc && NULL == clean) {
                alloc = malloc;
                clean = free;
        } else if (NULL != alloc && NULL != clean) {
                ;
        } else {
                errno = EINVAL;
                return -1;
        }

        ring = (struct ring *)alloc(sizeof(struct ring));

        if (NULL == ring) {
                return -1;
        }

        /*
         * From this point on, all the 'ring_node's in a 'ring' is
         * empty-initialized - the data field for each 'ring_node' is to be
         * filled (or overwritten) later through 'ring_add' call.
         */
        if (0 > ring_node_init_(&ring_node, alloc)) {
                clean(ring);
                return -1;
        }

        /* Initializes the 'ring' structure and the 1st 'ring_node'. */
        ring->length = length;
        ring->alloc = alloc;
        ring->clean = clean;
        ring->head = ring_node;
        ring->pos = ring_node;
        /* Create a self-loop for the 1st 'ring_node'. */
        ring->pos->next = ring->head;

        /*
         * Then initializes all the rest 'ring_node's while maintaining
         * the circular property - 'next' field of the last 'ring_node'
         * structure must point to the head 'ring_node'.
         */
        for (size_t i = 1U; i < length; ++i) {
                if (0 > ring_node_init_(&ring_node, alloc)) {
                        ring_destroy(&ring);
                        return -1;
                }
                ring->pos->next = ring_node;
                ring->pos = ring->pos->next;
                ring->pos->next = ring->head;
        }

        ring->pos = ring->head;
        *self = ring;

        return 0;
}

int ring_add(struct ring *self, const void *data, const size_t size)
{
        struct ring_node *ring_node = NULL;

        if (NULL == self || NULL == data || 0U == size) {
                errno = EINVAL;
                return -1;
        }

        ring_node = self->pos;

        if (0 >
            ring_node_fill_(ring_node, data, size, self->alloc, self->clean)) {
                return -1;
        }

        self->pos = self->pos->next;

        return 0;
}

int ring_length(const struct ring *self, size_t *length)
{
        if (NULL == self || NULL == length) {
                errno = EINVAL;
                return -1;
        }

        *length = self->length;
        return 0;
}

int ring_destroy(struct ring **self)
{
        struct ring *ring = NULL;
        struct ring_node *ring_node = NULL;
        struct ring_node *head_addr = NULL;
        ring_free_t clean = NULL;

        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        ring = *self;
        clean = ring->clean;
        head_addr = ring->head;

        for (ring_node = ring->head->next; ring->length != 0; --ring->length) {
                /*
                 * NOTE:
                 * The 'length' of ring is in range [1, ∞], so 'ring->head'
                 * must be a non-NULL value regardless of the 'length'.
                 * Then there is no way for the following function call to
                 * fail - return value is unchecked in this case.
                 */
                ring_node_destroy_(&(ring->head), clean);

                if (head_addr != ring_node) {
                        ring->head = ring_node;
                        ring_node = ring->head->next;
                }
        }

        clean(ring);
        *self = NULL;
        return 0;
}

int ring_iter_init(struct ring_iter **self, const struct ring *circle)
{
        struct ring_iter *ring_iter = NULL;

        if (NULL == self || NULL == circle) {
                errno = EINVAL;
                return -1;
        }

        ring_iter = (struct ring_iter *)malloc(sizeof(struct ring_iter));

        if (NULL == ring_iter) {
                return -1;
        }

        ring_iter->pos = circle->head;

        *self = ring_iter;
        return 0;
}

int ring_iter_walk(struct ring_iter *self, void **data)
{
        struct ring_iter *ring_iter = NULL;

        if (NULL == self || NULL == data) {
                errno = EINVAL;
                return -1;
        }

        ring_iter = self;

        *data = ring_iter->pos->data;
        ring_iter->pos = ring_iter->pos->next;

        return 0;
}

int ring_iter_destroy(struct ring_iter **self)
{
        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        free(*self);
        *self = NULL;
        return 0;
}

static int ring_node_init_(struct ring_node **self, ring_alloc_t alloc)
{
        struct ring_node *ring_node = NULL;

        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        if (NULL == alloc) {
                alloc = malloc;
        }

        ring_node = (struct ring_node *)alloc(sizeof(struct ring_node));

        if (NULL == ring_node) {
                return -1;
        }

        ring_node->data = NULL;
        ring_node->next = NULL;
        *self = ring_node;

        return 0;
}

static int ring_node_fill_(struct ring_node *self,
                           const void *data,
                           const size_t size,
                           ring_alloc_t alloc,
                           ring_free_t clean)
{
        void *temp = NULL;

        if (NULL == self || NULL == data || 0U == size) {
                errno = EINVAL;
                return -1;
        }

        if (NULL == alloc && NULL == clean) {
                alloc = malloc;
                clean = free;
        } else if (NULL != alloc && NULL != clean) {
                ;
        } else {
                errno = EINVAL;
                return -1;
        }

        if (NULL != self->data) {
                clean(self->data);
        }

        temp = alloc(size);

        if (NULL == temp) {
                return -1;
        }

        self->data = temp;
        memcpy(self->data, data, size);
        return 0;
}

static int ring_node_destroy_(struct ring_node **self, ring_free_t clean)
{
        struct ring_node *ring_node = NULL;

        if (NULL == self) {
                errno = EINVAL;
                return -1;
        }

        if (NULL == clean) {
                clean = free;
        }

        ring_node = *self;

        clean(ring_node->data);
        clean(ring_node);

        *self = NULL;
        return 0;
}
