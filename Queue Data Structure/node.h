/*
 * node.h
 *
 * Definition of your node structure. 
 *
 * You should update this file to complete the assignment.
 *
 */
 
#ifndef _NODE_H_
#define _NODE_H_


// forward declaration of queue structure
struct _fifo_queue; 
 
typedef struct _node_item {
    
    /*
     * must have for each item. do not remove.
     */
    int id;
    
    struct _node_item *next; 
    struct _node_item *prev;
	
    int in_or_not;    
} node_item_t;


#endif /* _NODE_H_ */
