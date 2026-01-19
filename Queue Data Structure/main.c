/*
 * main.c
 *
 * Testing code for your queue library. This file will not be graded.
 *
 */

#include "queue.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

int create_node_and_push(fifo_queue_t * queue)
{
    static int next_id = 1;
    node_item_t * item = malloc(sizeof(node_item_t));
    assert(item);
    node_init(item, next_id++);
    printf("pushing item %d into queue... ", item->id);
    int ret = queue_push(queue, item);
    if (ret < 0) {
        printf("failed.\n");
        free(item);
    }
    else {
        printf("success.\n");
    }
        
    return ret;
}

int pop_and_free_node(fifo_queue_t * queue)
{
    printf("popped item from queue... ");
    node_item_t * item = queue_pop(queue);
    if (!item) {
        printf("failed.\n");
        return -1;
    }
    else {
        printf("got item %d.\n", item->id);
        free(item);
    }
    return 0;
}

/* 
 * Write your own test cases here
 */
int main(int argc, const char * argv[])
{
    int ret;

    /* Example test case: ensure we cannot insert above queue capacity */
    fifo_queue_t * q1 = queue_create(1);

    // Should succeed the first time
    ret = create_node_and_push(q1);
    assert(ret == 0);

    // Should fail this time
    ret = create_node_and_push(q1);
    assert(ret < 0);

    // Should succeed the first time
    ret = pop_and_free_node(q1);
    assert(ret == 0);

    // Should fail this time
    ret = pop_and_free_node(q1);
    assert(ret < 0);

    queue_destroy(q1);

    return 0;
}