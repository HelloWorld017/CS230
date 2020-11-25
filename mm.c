/*
 * mm.c - nenw*'s Malloc Implementation
 * > It's "malloc", not "murloc"
 *
 * Implemented using Doubly-Linked List and AA Tree (equiv. 2-3 Tree) for indexing
 * > Find free memory using AA Tree, sorted by Size -> Address
 * > Coalesce using Doubly-linked List
 *
 * Block Format:
 * > Legend
 * >> ^: For AA Tree
 * >> a (1bit): Allocated or not
 * >> l (Total 6bit): The level of node
 *
 * > Allocated
 * >> |  Size        | a |
 * >> |  Payload         |
 * >> |  Padding         |
 * >> |  Size        | a |
 *
 * > Free
 * >> |  Size              | a  |
 * >> |  ChildL^ | l1 | l2 | l3 |
 * >> |  ChildR^ | l4 | l5 | l6 |
 * >> |  Size              | a  |
 *
 * malloc(size) / O(log n):
 * > Find first node larger than size
 * > Delete it from tree
 * > Add the node of remainder to the tree
 *
 * free(size) / O(log n):
 * > Find the previous node from Header/Footer
 * > Coalesce it
 * > Add the node to the tree
 *
 * realloc(size) / O(log n):
 * > If the next node is empty, malloc it and merge
 * > If not, malloc and free
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*
 * Global Variables
 * ====
 */
static void* heap;
static size_t heap_size;

static size_t* root;

/*
 * Backend
 * ====
 *
 * A data structure implementation
 */

/*
 * _backend_left: Get left node of given node
 */
inline size_t* _backend_left(size_t* node) {
	return _backend_ptr(node + 1);
}

/*
 * _backend_right: Get right node of given node
 */
inline size_t* _backend_right(size_t* node) {
	return _backend_ptr(node + 2);
}

/*
 * _backend_level: Get level of given node
 */
inline int _backend_level(size_t* ptr) {
	return ((*(ptr + 1)) & 0x7) << 3 + ((*(ptr + 2)) & 0x7);
}

/*
 * _backend_node: Convert ptr to node
 */
inline size_t* _backend_node(size_t* ptr) {
	return (*ptr) & ~0x7;
}

/*
 * _backend_skew: Apply skew operation to given ptr
 */
inline void _backend_skew(size_t* node_ptr) {
	if (node_ptr == NULL) return;

	// Get node from ptr
	size_t* node = _backend_node(node_ptr);
	if (node == NULL) return;

	// Get left node from node
	size_t* left_node = _backend_left(node);
	if (left_node == NULL) return;

	// When left is same level with node
	// (Horizontal connection with left)
	int level = _backend_level(node);
	int left_level = _backend_level(left_node);
	if(level != left_level) return;

	// (Left child of node) is (Right child of left_node)
	*(node + 1) = _backend_right(left_node) | ((level >> 3) & 0x7);

	// (Right child of left_node) is (node)
	*(left_node + 2) = node | (left_level & 0x7);

	// (node) is (left_node)
	*node_ptr = left_node | ((*node_ptr) & 0x7);
}

/*
 * _backend_skew: Apply split operation to given ptr
 */
inline void _backend_split(size_t* node_ptr) {
	if (node_ptr == NULL) return;

	// Get node from ptr
	size_t* node = _backend_node(node_ptr);
	if (node == NULL) return;

	// Get left, right, right_right node from node
	size_t* left_node = _backend_left(node);
	size_t* right_node = _backend_right(node);
	size_t* right_right_node = _backend_right(right_node);
	if (right_node == NULL || right_right_node == NULL) return;

	// When right_right is same level with node
	// (Two simultaneous horizontal connection)
	int level = _backend_level(node);
	int right_level = _backend_level(right_node);
	int right_right_level = _backend_level(right_right_node);
	if (level != right_right_level) return;

	// Increase right level
	right_level++;

	// (Right child of node) is (Left child of right_node)
	*(node + 2) = _backend_left(right_node) | (level & 0x7);

	// (Left child of right_node) is (node)
	*(right_node + 1) = node | ((right_level >> 3) & 0x7);

	// Assign right_level
	*(right_node + 2) = right_right_node | (right_level & 0x7);

	// (node) is (right_node)
	*node_ptr = right_node | ((*node_ptr) & 0x7);
}

inline void backend_init() {

}

/*
 * backend_add: Add new node to the tree
 */
inline void backend_add(size_t* node) {
	// Get size of adding node
	size_t node_size = (*node) & ~0x7;

	// Initialize iteration variable
	size_t* current_node = root;
	size_t* current_ptr = (size_t*) &root;

	while (1) {
		// Get size of iterating node
		size_t current_size = (*current_node & ~0x7);
		size_t* next_ptr;

		// Decide where to descend
		// > Sort by size, then sort by address
		if (current_size < node_size) {
			next_ptr = current_node + 1;
		} else if (current_size > node_size) {
			next_ptr = current_node + 2;
		} else {
			if (current_node < node) {
				next_ptr = current_node + 1;
			} else {
				next_ptr = current_node + 2;
			}
		}

		// If we can't proceed, break loop
		size_t* next_node = _backend_node(next_ptr);
		if (next_node == NULL) {
			current_ptr = next_ptr;
			break;
		}

		// Else, skew & split this node if available
		_backend_skew(current_ptr);
		_backend_split(current_ptr);

		current_ptr = next_ptr;
		current_node = next_node;
	}

	// Insert new node
	*current_ptr = node | ((*current_ptr) & 0x7);
}

/*
 * _backend_nearest: Find predecessor / successor node
 */
inline size_t* _backend_nearest(size_t* node, int is_predecessor) {
	size_t* current_node = node;

	if (current_node == NULL)
		return current_node;

	while (1) {
		size_t* next_node =
				is_predecessor ? _backend_left(current_node) : _backend_right(current_node);

		if (next_node == NULL) {
			return current_node;
		} else {
			current_node = next_node;
		}
	}
}

/*
 * backend_pop: Find smallest node which size is larger than given size.
 *     Remove and return the node
 *     If the find fails, returns null
 */
inline size_t* backend_pop(size_t size) {
	// Initialize iteration variable
	size_t* current_node = root;
	size_t* current_ptr = (size_t*) &root;

	while (1) {
		// Get size of iterating node
		size_t current_size = (*current_node) & ~0x7;
		size_t* next_ptr;

		if (size <= current_size) {
			// When the result is in the left node or current_node

			// Calculate left node
			next_ptr = current_node + 1;
			size_t* next_node = _backend_node(next_ptr);

			if (next_node != NULL && size < ((*next_node) & ~0x7)) {
				// If the result is in the left node
				current_ptr = next_ptr;
				current_node = next_node;
			} else {
				// If the result is current_node
				break;
			}
		} else {
			// When the result is in the right node
			next_ptr = current_node + 2;
			size_t* next_node = _backend_node(next_ptr);

			if (next_node != NULL) {
				current_ptr = next_ptr;
				current_node = next_node;
			} else {
				// As there's no right node, we couldn't find
				return NULL;
			}
		}

		// Rebalance the tree of current level
		_backend_shrink(current_ptr);

	}
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
	heap = mem_heap_lo();
	heap_size = mem_heapsize();

	*heap
	return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
	int newsize = ALIGN(size + SIZE_T_SIZE);
	void *p = mem_sbrk(newsize);
	if(p == (void *) -1)
		return NULL;
	else {
		*(size_t *) p = size;
		return (void *) ((char *) p + SIZE_T_SIZE);
	}
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
	void *oldptr = ptr;
	void *newptr;
	size_t copySize;

	newptr = mm_malloc(size);
	if(newptr == NULL)
		return NULL;
	copySize = *(size_t * )((char *) oldptr - SIZE_T_SIZE);
	if(size < copySize)
		copySize = size;
	memcpy(newptr, oldptr, copySize);
	mm_free(oldptr);
	return newptr;
}
