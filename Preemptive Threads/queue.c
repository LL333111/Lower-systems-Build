/*
 * queue.c
 *
 * Definition of the queue structure and implemenation of its API functions.
 *
 */

#include "queue.h"
#include <stdlib.h>
#include <assert.h>

struct _fifo_queue
{
    node_item_t *head;
    node_item_t *tail;
    int size;
    int capacity;
    void *owner;
};

void node_init(node_item_t *node, int id)
{
    (*node).id = id;
    (*node).next = NULL;
    (*node).prev = NULL;
    (*node).in_or_not = 0;
}

bool node_in_queue(node_item_t *node)
{
    if ((*node).in_or_not == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

fifo_queue_t *queue_create(unsigned capacity)
{
    if (capacity < 1)
    {
        return NULL;
    }

    fifo_queue_t *new_queue = malloc(sizeof(fifo_queue_t));
    if (new_queue == NULL)
    {
        return NULL;
    }
    new_queue->head = NULL;
    new_queue->tail = NULL;
    new_queue->capacity = capacity;
    new_queue->size = 0;

    return new_queue;
}

void queue_destroy(fifo_queue_t *queue)
{
    assert(queue != NULL);
    assert((*queue).size == 0);
    free(queue);
}

node_item_t *queue_pop(fifo_queue_t *queue)
{

    if ((*queue).size == 0)
    {
        return NULL;
    }
    node_item_t *item = (*queue).head;
    (*queue).head = (*queue).head->next;
    (*queue).size--;
    (*item).in_or_not = 0;
    if ((*queue).size > 0)
    {
        (*queue).head->prev = NULL;
    }
    else
    {
        (*queue).tail = NULL;
    }

    item->next = NULL;
    item->prev = NULL;
    return item;
}

node_item_t *queue_top(fifo_queue_t *queue)
{
    if ((*queue).size == 0)
    {
        return NULL;
    }
    return (*queue).head;
}

int queue_push(fifo_queue_t *queue, node_item_t *node)
{
    // make sure we aren't enqueuing a node that already belongs to another queue.
    assert(!node_in_queue(node));

    if ((*queue).size == (*queue).capacity)
    {
        return -1;
    }

    if ((*queue).size == 0)
    {
        (*queue).head = node;
        (*queue).tail = node;
        node->prev = NULL;
        node->next = NULL;
    }
    else
    {
        (*queue).tail->next = node;
        (*node).prev = (*queue).tail;
        node->next = NULL;
        (*queue).tail = node;
    }
    (*queue).size++;
    (*node).in_or_not = 1;
    return 0;
}

node_item_t *queue_remove(fifo_queue_t *queue, int id)
{

    node_item_t *start = queue->head;
    while (start != NULL)
    {
        if (start->id == id)
        {
            if (start->prev != NULL)
            {
                start->prev->next = start->next;
            }
            else
            {
                queue->head = start->next;
            }
            if (start->next != NULL)
            {
                start->next->prev = start->prev;
            }
            else
            {
                queue->tail = start->prev;
            }
            start->in_or_not = 0;
            (*queue).size--;

            start->prev = NULL;
            start->next = NULL;
            return start;
        }
        start = start->next;
    }
    return NULL;
}

int queue_count(fifo_queue_t *queue)
{
    return queue->size;
}

void queue_set_owner(fifo_queue_t *queue, void *owner)
{
    queue->owner = owner;
}

void *queue_get_owner(fifo_queue_t *queue)
{
    return queue->owner;
}