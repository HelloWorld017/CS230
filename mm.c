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
void* heap;
size_t heap_size;

size_t* root;

/*
 * Backend
 * ====
 *
 * A data structure implementation
 */

/*
 * _backend_node: Convert ptr to node
 */
inline size_t* _backend_node(size_t* ptr) {
	return (size_t*) ((*ptr) & ~0x7);
}

/*
 * _backend_level: Get level of given node
 */
inline int _backend_level(size_t* ptr) {
	return (((*(ptr + 1)) & 0x7) << 3) + ((*(ptr + 2)) & 0x7);
}

/*
 * _backend_left: Get left node of given node
 */
inline size_t* _backend_left(size_t* node) {
	return _backend_node(node + 1);
}

/*
 * _backend_right: Get right node of given node
 */
inline size_t* _backend_right(size_t* node) {
	return _backend_node(node + 2);
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
	*(node + 1) = ((size_t) _backend_right(left_node)) | ((level >> 3) & 0x7);

	// (Right child of left_node) is (node)
	*(left_node + 2) = ((size_t) node) | (left_level & 0x7);

	// (node) is (left_node)
	*node_ptr = ((size_t) left_node) | ((*node_ptr) & 0x7);
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
	*(node + 2) = ((size_t) _backend_left(right_node)) | (level & 0x7);

	// (Left child of right_node) is (node)
	*(right_node + 1) = ((size_t) node) | ((right_level >> 3) & 0x7);

	// Assign right_level
	*(right_node + 2) = ((size_t) right_right_node) | (right_level & 0x7);

	// (node) is (right_node)
	*node_ptr = ((size_t) right_node) | ((*node_ptr) & 0x7);
}

inline int backend_init(void* heap, size_t heap_align_size) {
	if (heap_align_size < 4)
		return 1;

	size_t* tree_space = (size_t*) ALIGN((size_t) heap);
	size_t heap_size = heap_align_size * ALIGNMENT;

	*tree_space = (heap_size & ~0x7);
	*(tree_space + 1) = 0;
	*(tree_space + 2) = 0;
	*(tree_space + heap_align_size) = (heap_size & ~0x7);
	return 0;
}

/*
 * backend_add: Add new node to the tree
 */
size_t _backend_add_node_size;
size_t* _backend_add_node;
void _backend_add(size_t* current_ptr) {
	// Initialize iteration variable
	size_t* current_node = _backend_node(current_ptr);

	// Get size of iterating node
	size_t current_size = (*current_node & ~0x7);
	size_t* next_ptr;

	// Decide where to descend
	// > Sort by size, then sort by address
	if (current_size < _backend_add_node_size) {
		next_ptr = current_node + 1;
	} else if (current_size > _backend_add_node_size) {
		next_ptr = current_node + 2;
	} else {
		if (current_node < _backend_add_node) {
			next_ptr = current_node + 1;
		} else {
			next_ptr = current_node + 2;
		}
	}

	// If we can't proceed, break loop
	size_t* next_node = _backend_node(next_ptr);
	if (next_node == NULL) {
		*next_ptr = ((size_t) _backend_add_node) | ((*next_ptr) & 0x7);
		return;
	}

	_backend_add(next_ptr);

	// Else, skew & split this node if available
	_backend_skew(current_ptr);
	_backend_split(current_ptr);
}

inline void backend_add(size_t* node) {
	_backend_add_node_size = (*node) & ~0x7;
	_backend_add_node = node;
	_backend_add((size_t*) &root);
}

/*
 * _backend_nearest: Find predecessor / successor ptr
 */
inline size_t* _backend_nearest(size_t* node_ptr, int is_predecessor) {
	size_t* current_ptr = node_ptr;

	if (current_ptr == NULL)
		return current_ptr;

	size_t* current_node = _backend_node(node_ptr);
	if (current_node == NULL)
		return current_ptr;

	while (1) {
		size_t* next_ptr =
				is_predecessor ? current_node + 1 : current_node + 2;

		size_t* next_node = _backend_node(next_ptr);
		if (next_node == NULL) {
			return current_ptr;
		} else {
			current_node = next_node;
			current_ptr = next_ptr;
		}
	}
}

/*
 * backend_pop: Find smallest node which size is larger than given size.
 *     Remove and return the node
 *     If the find fails, returns null
 */
inline void _backend_pop_rebalance(size_t* current_ptr) {
	if (current_ptr == NULL)
		return;

	size_t* current_node = _backend_node(current_ptr);
	if (current_node == NULL)
		return;

	size_t* left_node = _backend_left(current_node);
	size_t* right_node = _backend_right(current_node);

	// Shrink level to target_level
	int left_level, right_level, target_level = -1;

	if (left_node != NULL) {
		left_level = _backend_level(left_node);
		target_level = left_level + 1;
	}

	if (right_node != NULL) {
		right_level = _backend_level(right_node);
		target_level = right_level + 1;
	}

	if ((left_node != NULL) && (right_node != NULL)) {
		target_level = ((left_level < right_level) ? left_level : right_level) + 1;
	}

	if (_backend_level(current_node) > target_level && target_level > 0) {
		*(current_node + 1) = (*(current_node + 1) & ~0x7) | ((target_level >> 3) & 0x7);
		*(current_node + 2) = (*(current_node + 2) & ~0x7) | (target_level & 0x7);

		if (right_node != NULL && (_backend_level(right_node) > target_level)) {
			*(right_node + 1) = (*(right_node + 1) & ~0x7) | ((target_level >> 3) & 0x7);
			*(right_node + 2) = (*(right_node + 2) & ~0x7) | (target_level & 0x7);
		}
	}

	_backend_skew(current_node);
	if (right_node != NULL) {
		_backend_skew(right_node + 2);
	}
	_backend_split(current_node);
	_backend_split(current_node + 2);
}

size_t _backend_pop_size;
size_t* _backend_pop(size_t* current_ptr) {
	// Initialize iteration variable
	size_t* current_node = _backend_node(current_ptr);

	// Get size of iterating node
	size_t current_size = (*current_node) & ~0x7;
	size_t* next_ptr;

	if (_backend_pop_size <= current_size) {
		// When the result is in the left node or current_node

		// Calculate left node
		next_ptr = current_node + 1;
		size_t* next_node = _backend_node(next_ptr);

		if (next_node != NULL && _backend_pop_size < ((*next_node) & ~0x7)) {
			// If the result is in the left node
			next_ptr = _backend_pop(next_ptr);
		} else {
			// If the result is current_node
			size_t* replace_ptr;
			if (next_node != NULL) {
				replace_ptr = _backend_nearest(next_ptr, 0);
			} else {
				next_ptr = current_node + 2;
				next_node = _backend_node(next_ptr);

				if (next_node != NULL) {
					replace_ptr = _backend_nearest(next_ptr, 1);
				} else {
					replace_ptr = NULL;
				}
			}

			size_t* replace_node = NULL;
			if (replace_ptr != NULL) {
				replace_node = _backend_node(replace_ptr);
				*replace_ptr = ((size_t) NULL) | ((*replace_ptr) & 0x7);
			}

			*current_ptr = ((size_t) replace_node) | ((*current_ptr) & 0x7);
			next_ptr = current_ptr;
		}
	} else {
		// When the result is in the right node
		next_ptr = current_node + 2;
		size_t* next_node = _backend_node(next_ptr);

		if (next_node != NULL) {
			next_ptr = _backend_pop(next_ptr);
		} else {
			// As there's no right node, we couldn't find
			return NULL;
		}
	}

	// Rebalance the tree of current level
	_backend_pop_rebalance(current_ptr);

	return next_ptr;
}

inline size_t* backend_pop(size_t size) {
	_backend_pop_size = size;
	_backend_pop((size_t*) &root);
}

void _backend_debug(size_t* node) {
	if (node == NULL) return;
	_backend_debug(_backend_left(node));
	printf("NODE %d\n", *(node) & ~0x7);
	_backend_debug(_backend_right(node));
}

void backend_debug(void) {
	_backend_debug(root);
}


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
	heap = mem_heap_lo();
	heap_size = mem_heapsize();

	// *heap
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
