#include "list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LINKEDLIST_ITER(head) \
	struct list_node* item = head->next; \
	struct list_node* tail = head->prev; \
	for (; item != tail; item = item->next)

#define LINKEDLIST_LINK(prev_node, next_node) ({ \
    prev_node->next = next_node; \
    next_node->prev = prev_node; \
})

/*	
Allocate a linked list node with a given key
Allocate a node using malloc(),
initialize the pointers to NULL,
set the key value to the key provided by the argument
 */
struct list_node *allocate_node_with_key(int key) {
	struct list_node* newNode = malloc(sizeof(struct list_node));
	newNode->next = NULL;
	newNode->prev = NULL;
	newNode->value = NULL;
	newNode->key = key;

	return newNode;
}

/*	
Initialize the key values of the head/tail list nodes (I used -1 key values)
Also, link the head and tail to point to each other
 */
void initialize_list_head_tail(struct list_node *head, struct list_node *tail) {
	head->key = -1;
	tail->key = -1;
	LINKEDLIST_LINK(head, tail);
	LINKEDLIST_LINK(tail, head);
}

/*	
Insert the *new_node* after the *node*
 */
void insert_node_after (struct list_node *node, struct list_node *new_node) {
	struct list_node* nextNode = node->next;
	LINKEDLIST_LINK(node, new_node);
	LINKEDLIST_LINK(new_node, nextNode);
	return;
}

/*	
Remove the *node* from the list
 */
void del_node (struct list_node *node) {
	LINKEDLIST_LINK(node->prev, node->next);
	free(node);
	return;
}

/*	
Search from the head to the tail (excluding both head and tail,
as they do not hold valid keys), for the node with a given *search_key*
and return the node.
You may assume that the list will only hold nodes with unique key values
(No duplicate keys in the list)
 */
struct list_node *search_list (struct list_node *head, int search_key) {
	LINKEDLIST_ITER(head) {
		if (item->key == search_key) {
			return item;
		}
	}

	return NULL;
}

/*	
Count the number of nodes in the list (excluding head and tail), 
and return the counted value
 */
int count_list_length (struct list_node *head) {
	int len = 0;
	LINKEDLIST_ITER(head) {
		len++;
	}

	return len;
}

/*	
Check if the list is empty (only head and tail exist in the list)
Return 1 if empty. Return 0 if list is not empty.
 */
int is_list_empty (struct list_node *head) {
	return head->prev == head->next ? 1 : 0;
}

/*	
Loop through the list and print the key values
This function will not be tested, but it will aid you in debugging your list.
You may add calls to the *iterate_print_keys* function in the test.c
at points where you need to debug your list.
But make sure to test your final version with the original test.c code.
 */
void iterate_print_keys (struct list_node *head) {
	LINKEDLIST_ITER(head) {
		printf("%d", item->key);
	}
}

/*	
Insert a *new_node* at the sorted position so that the keys of the nodes of the
list (including the key of the *new_node*) is always sorted (increasing order)
 */
int insert_sorted_by_key (struct list_node *head, struct list_node *new_node) {
	LINKEDLIST_ITER(head) {
		if(item->key < new_node->key) {
			continue;
		}

		break;
	}

	insert_node_after(item->prev, new_node);
	return 0;
}
