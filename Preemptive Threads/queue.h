/*
 * queue.h
 *
 * Declaration of API functions that operate on the queue structure.
 *
 * You may not change this file.
 *
 */

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "thread.h"
#include "ut369.h"

typedef struct thread node_item_t;
// fifo_queue_t declared in ut369.h

/*
 * Initialize an already-allocated node structure.
 */
void node_init(node_item_t * node, int id);

/*
 * Check whether the node is already part of a queue.
 * Returns true if yes, false if no.
 */
bool node_in_queue(node_item_t * node);

/*
 * Create a new queue structure.
 * Return a dynamically allocated queue structure upon success.
 * When out of memory, or if capacity is equal to 0, return NULL.
 */
fifo_queue_t * queue_create(unsigned capacity);

/*
 * Delete an empty queue.
 * If the queue is not empty when this function is called, the program
 * should crash on an assertion error.
 */
void queue_destroy(fifo_queue_t * queue);

/*
 * Return the node at the top of the queue and removes it from the queue,
 * or NULL if the queue is empty.
 */
node_item_t * queue_pop(fifo_queue_t * queue);

/*
 * Return the node at the top of the queue without popping it, or NULL if the
 * queue is empty.
 */
node_item_t * queue_top(fifo_queue_t * queue);

/*
 * Insert the node to the bottom of the queue.
 * Return 0 on success,
 *       -1 if the queue is at capacity (do not insert in this case).
 */
int queue_push(fifo_queue_t * queue, node_item_t * node);

/*
 * Return the node in the queue with the specified id and removes it from the queue.
 * You may assume all ids in a queue are unique.
 * Return NULL if no node in the queue has the specified id.
 */
node_item_t * queue_remove(fifo_queue_t * queue, int id);

/*
 * Return the number of elements in the queue.
 */
int queue_count(fifo_queue_t * queue);

/*
 * Get the owner of the queue, or NULL if no one owns this queue. The
 * caller needs to cast the return value to the actual pointer type.
 */
void * queue_get_owner(fifo_queue_t *queue);

/*
 * Set the owner of the queue to an arbitrary pointer `owner`.
 */
void queue_set_owner(fifo_queue_t *queue, void * owner);

#endif /* _QUEUE_H_ */
