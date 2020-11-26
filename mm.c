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
 * >> |  Size        | a | (size_t)
 * >> |  Payload         | (void)
 * >> |  Padding         | (void)
 * >> |  Size        | a | (size_t)
 *
 * > Free
 * >> |  Size              | a  | (size_t)
 * >> |  ChildL^ | l1 | l2 | l3 | (size_t)
 * >> |  ChildR^ | l4 | l5 | l6 | (size_t)
 * >> |  Padding                | (void)
 * >> |  Size              | a  | (size_t)
 *
 * malloc(size):
 * > Find first node larger than size
 * > Delete it from tree
 * > Add the node of remainder to the tree
 *
 * free(size):
 * > Find the previous / next node from Header/Footer
 * > Coalesce it
 * > Add the node to the tree
 *
 * realloc(size):
 * > If the next node is empty, malloc it and merge
 * > If not, malloc and free
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"

#ifdef BACKEND_DEBUG
	#define MM_DEBUG(fmt, args...) printf(fmt, ##args)
#else
	#define MM_DEBUG(fmt, args...) do {} while (0)
	#include "memlib.h"
#endif

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (sizeof(size_t))

/*
 * Backend
 * ====
 *
 * A data structure implementation
 */
#define BACKEND_MIN_SIZE ALIGN(SIZE_T_SIZE * 4)
size_t* root;

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
	if (right_node == NULL) return;

	size_t* right_right_node = _backend_right(right_node);
	if (right_right_node == NULL) return;

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

inline void backend_init(void) {
	root = NULL;
}

/*
 * backend_add: Add new node to the tree
 */
size_t _backend_add_node_size;
size_t* _backend_add_node;
void _backend_add(size_t* current_ptr) {
	// Get iterating node
	size_t* current_node = _backend_node(current_ptr);

	// If current node is null, assign and escape
	if (current_node == NULL) {
		*current_ptr = ((size_t) _backend_add_node) | ((*current_ptr) & 0x7);
		return;
	}

	// Get size of iterating node
	size_t current_size = (*current_node & ~0x7);
	size_t* next_ptr;

	// Decide where to descend
	// > Sort by size, then sort by address
	if (_backend_add_node_size < current_size) {
		next_ptr = current_node + 1;
	} else if (_backend_add_node_size > current_size) {
		next_ptr = current_node + 2;
	} else {
		if (_backend_add_node < current_node) {
			next_ptr = current_node + 1;
		} else {
			next_ptr = current_node + 2;
		}
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

	// Get start node
	size_t* current_node = _backend_node(node_ptr);
	if (current_node == NULL)
		return current_ptr;

	while (1) {
		// If it is predecessor, keep descend to left
		// Else, keep descend to right
		size_t* next_ptr =
				is_predecessor ? current_node + 1 : current_node + 2;

		size_t* next_node = _backend_node(next_ptr);
		if (next_node == NULL) {
			// When we can't proceed, return final node
			return current_ptr;
		} else {
			// As we can proceed, keep proceed to next child
			current_node = next_node;
			current_ptr = next_ptr;
		}
	}
}


/*
 * _backend_remove_rebalance:
 *     Make one level of the tree, which balance is broken by remove, as a balanced
 */
inline void _backend_remove_rebalance(size_t* current_ptr) {
	if (current_ptr == NULL)
		return;

	// Get current node
	size_t* current_node = _backend_node(current_ptr);
	if (current_node == NULL)
		return;

	// Try to get left & right child of node
	size_t* left_node = _backend_left(current_node);
	size_t* right_node = _backend_right(current_node);

	/*
	 * Rebalance Step 1: Shrink level to target_level
	 */
	int left_level, right_level, target_level = -1;

	if (left_node != NULL) {
		// When has left child
		left_level = _backend_level(left_node);
		target_level = left_level + 1;
	}

	if (right_node != NULL) {
		// When has right child
		right_level = _backend_level(right_node);
		target_level = right_level + 1;
	}

	if ((left_node != NULL) && (right_node != NULL)) {
		// When has both child, make the target level as the minimum of two target levels
		target_level = ((left_level < right_level) ? left_level : right_level) + 1;
	}

	if (_backend_level(current_node) > target_level && target_level > 0) {
		// When the level is larger than target_level, shrink it to target_level
		*(current_node + 1) = (*(current_node + 1) & ~0x7) | ((target_level >> 3) & 0x7);
		*(current_node + 2) = (*(current_node + 2) & ~0x7) | (target_level & 0x7);

		if (right_node != NULL && (_backend_level(right_node) > target_level)) {
			// When the level of right child is larger than target_level, shrink it to target_level
			*(right_node + 1) = (*(right_node + 1) & ~0x7) | ((target_level >> 3) & 0x7);
			*(right_node + 2) = (*(right_node + 2) & ~0x7) | (target_level & 0x7);
		}
	}

	/*
	 * Rebalance Step 2: Skew current_node, right_node, right_right_node
	 */
	_backend_skew(current_ptr);
	_backend_skew(current_node + 2);
	if (right_node != NULL) {
		_backend_skew(right_node + 2);
	}

	/*
	 * Rebalance Step 3: Skew current_node, right_node
	 */
	_backend_split(current_ptr);
	_backend_split(current_node + 2);
}

/*
 * _backend_remove_unbalanced: Remove given node from tree and return
 *     This method does not rebalance tree
 */
inline size_t* _backend_remove_unbalanced(size_t* current_ptr) {
	size_t* current_node = _backend_node(current_ptr);

	// First find node to replace the node
	size_t* replace_ptr;
	size_t* next_ptr = current_node + 1;
	size_t* next_node = _backend_node(next_ptr);

	if (next_node != NULL) {
		// When we have left child,
		// the successor node is the node to be replaced with current_node
		replace_ptr = _backend_nearest(next_ptr, 0);
	} else {
		// When we don't have left child
		next_ptr = current_node + 2;
		next_node = _backend_node(next_ptr);

		if (next_node != NULL) {
			// If we only have right child,
			// the predecessor node is the node to be replaced with current_node
			replace_ptr = _backend_nearest(next_ptr, 1);
		} else {
			// current_node is a leaf node and can be safely deleted
			replace_ptr = NULL;
		}
	}

	size_t* replace_node = NULL;
	if (replace_ptr != NULL) {
		// If we have to replace current_node with another node,

		// save it and
		replace_node = _backend_node(replace_ptr);

		// remove the node (with only one child or not) from the tree
		size_t* new_replace_node = NULL;

		if (replace_node != NULL) {
			size_t* replace_left_node = _backend_left(replace_node);
			if (replace_left_node != NULL) {
				// If it has a left child, remove by replacing it with the child
				new_replace_node = replace_left_node;
			} else {
				// If it has a right child, remove by replacing it with the child
				// If it has no child, remove by replacing it with null
				new_replace_node = _backend_right(replace_node);
			}
		}

		*replace_ptr = ((size_t) new_replace_node) | ((*replace_ptr) & 0x7);
		*(replace_node + 1) = *(current_node + 1);
		*(replace_node + 2) = *(current_node + 2);
	}

	// Replace the pointer to current_node, with the pointer to replace_node
	*current_ptr = ((size_t) replace_node) | ((*current_ptr) & 0x7);

	return current_node;
}

/*
 * backend_remove: Remove and return given node
 *     If there's no such node, return null
 */
size_t _backend_remove_size;
size_t _backend_remove_target;
size_t* _backend_remove(size_t* current_ptr) {
	// Get current iterating node
	size_t* current_node = _backend_node(current_ptr);

	if (current_node == NULL) {
		return NULL;
	}

	// Get size of iterating node
	size_t current_size = (*current_node) & ~0x7;

	// Descend to left or right
	size_t* next_ptr;
	size_t* next_node;
	if (_backend_remove_size < current_size) {
		// Descend to left
		next_node = _backend_remove(current_node + 1);
	} else if (_backend_remove_size > current_size) {
		// Descend to right
		next_node = _backend_remove(current_node + 2);
	} else {
		// Same size. From now, we sort by address
		if (_backend_remove_target < current_node) {
			// Descend to left
			next_node = _backend_remove(current_node + 1);
		} else if (_backend_remove_target > current_node) {
			// Descend to right
			next_node = _backend_remove(current_node + 2);
		} else {
			// Found the value

			// Remove it
			_backend_remove_unbalanced(current_ptr);
			next_node = current_node;
		}
	}

	// Rebalance current level
	_backend_remove_rebalance(current_node);
	return next_node;
}

inline size_t* backend_remove(size_t* target_node) {
	if (target_node == NULL)
		return target_node;

	_backend_remove_size = (*target_node) & ~0x7;
	_backend_remove_target = target_node;
	return _backend_remove((size_t*) &root);
}

/*
 * backend_pop: Find smallest node which size is larger than given size.
 *     Remove and return the node
 *     If the find fails, returns null
 */
size_t _backend_pop_size;
size_t* _backend_pop(size_t* current_ptr) {
	// Get current iterating node
	size_t* current_node = _backend_node(current_ptr);

	if (current_node == NULL) {
		return NULL;
	}

	// Get size of iterating node
	size_t current_size = (*current_node) & ~0x7;
	MM_DEBUG("Iterate! %d\n", current_size);

	// The node to be returned
	size_t* next_node;

	if (_backend_pop_size <= current_size) {
		// When the result is in the left node or current_node

		// Calculate left node
		size_t* next_ptr = current_node + 1;
		next_node = _backend_node(next_ptr);
		int is_current = 0;

		if (_backend_pop_target != current_node) {
			// If current mode is find mode
			is_current = 1;
		} else if (next_node != NULL) {
			// If we have left child, first descend to left child
			MM_DEBUG("Left > ");
			size_t* retval = _backend_pop(next_ptr);

			if (retval == NULL) {
				// If we cannot find closer candidate, the result is current_node
				is_current = 1;
			} else {
				// If we have found closer candidate, the result is returned value
				next_node = retval;
			}
		} else {
			// If we don't have any left child, the result is current_node
			is_current = 1;
		}

		if (is_current) {
			// If the result is current_node
			MM_DEBUG("Found! %d\n", current_size);

			// Remove and return current_node
			next_node = _backend_remove_unbalanced(current_ptr);
		}
	} else {
		// When the result is in the right node
		size_t* next_ptr = current_node + 2;
		next_node = _backend_node(next_ptr);

		if (next_node != NULL) {
			MM_DEBUG("Right > ");
			next_node = _backend_pop(next_ptr);

			if (next_node == NULL)
				return NULL;
		} else {
			MM_DEBUG("Not-Found //\n");
			// As there's no right node, we couldn't find
			return NULL;
		}
	}

	// Rebalance the tree of current level
	_backend_remove_rebalance(current_ptr);
	MM_DEBUG("Rebalance > Done...\n");

	return next_node;
}

inline size_t* backend_pop(size_t size) {
	_backend_pop_size = size;
	return _backend_pop((size_t*) &root);
}

/*
 * backend_debug: Prints preorder traversal result
 */
void _backend_debug(size_t* node, int lvl) {
	for (int i = 0; i < lvl; i++)
		MM_DEBUG(" ");

	if (node == NULL) {
		MM_DEBUG("()\n");
		return;
	}

	MM_DEBUG("NODE %d (\n", *(node) & ~0x7);
	_backend_debug(_backend_left(node), lvl + 1);
	_backend_debug(_backend_right(node), lvl + 1);

	for (int i = 0; i < lvl; i++)
		MM_DEBUG(" ");

	MM_DEBUG(")\n");
}

void backend_debug(void) {
	_backend_debug(root, 0);
}

#ifdef BACKEND_DEBUG
int mm_init(void) { return 0; }
void *mm_malloc(size_t size) { return NULL; }
void mm_free(void *ptr) {}
void *mm_realloc(void *ptr, size_t size) { return NULL; }
#else

void* heap;

/* 
 * mm_init - initialize the malloc package
 */
int mm_init(void) {
	// Initialize memlib
	mem_init();

	// Get the aligned start position of heap
	size_t start_heap = mem_heap_lo();
	heap = ALIGN(start_heap);

	// Claim some memory to align heap start
	mem_sbrk(heap - start_heap);

	// Initialize tree
	backend_init();

	return 0;
}

/* 
 * mm_malloc - Allocate memory
 * > Find first node larger than size
 * > Delete it from tree
 * > Add the node of remainder to the tree
 */
void *mm_malloc(size_t size) {
	size_t* allocated = backend_pop(size);
	size_t node_size = ALIGN(size + 2 * SIZE_T_SIZE);
	size_t body_size;

	if (allocated == NULL) {
		// When we cannot coalesce
		size_t* claim_start = ((char*) mem_heap_hi() + 1);
		size_t claim_size = node_size;

		// Calculate body size
		body_size = claim_size - 2 * SIZE_T_SIZE;

		if (mem_heapsize() > 0) {
			// Try to coalesce with previous memory
			size_t* footer = (size_t*) (((char*) claim_start) - SIZE_T_SIZE);

			// Make sure it is free, not allocated
			if(!((*footer) & 0x7)) {
				// We can earn size of (Header + Body + Footer)
				size_t body_size = (*footer) & ~0x7;
				claim_size -= body_size + 2 * SIZE_T_SIZE;

				// Calculate pointer to node and remove
				size_t* header = (size_t*) (((char*) footer) - body_size - SIZE_T_SIZE);
				backend_remove(header);

				claim_start = header;
			}
		}

		// Claim memory
		if (mem_sbrk(claim_size) < 0) {
			// When failed to claim more memory
			return NULL;
		}

		allocated = claim_start;
	} else {
		// Calculate body size from header
		body_size = (*allocated) & ~0x7;

		// Check if we can split the allocated node into two parts
		size_t allocated_node_size = ALIGN(body_size + 2 * SIZE_T_SIZE);

		if (allocated_node_size > node_size + BACKEND_MIN_SIZE) {
			// Shrink body size
			body_size = node_size - 2 * SIZE_T_SIZE;

			// Calculate body size of next node
			size_t next_body_size = allocated_node_size - node_size - 2 * SIZE_T_SIZE;

			// Assign size to header of next node
			size_t* next_start = (size_t*) ((char*) allocated) + node_size;
			*next_start = ((next_body_size) & ~0x7);

			// Assign size to footer of next node
			size_t* next_footer = (size_t*) (((char*) next_start) + SIZE_T_SIZE + next_body_size);
			*next_footer = ((next_body_size) & ~0x7);

			// Index next node to the tree
			backend_add(next_start);
		}
	}

	// Calculate start of body
	char* body_start = ((char*) allocated) + SIZE_T_SIZE;

	// Assign size & allocated flag to header
	*allocated = ((body_size) & ~0x7) | 0x1;

	// Assign size & allocated flag to footer
	size_t* allocated_footer = (size_t*) (body_start + body_size);
	*allocated_footer = ((body_size) & ~0x7) | 0x1;

	// Return start of body
	return (void*) body_start;
}

/*
 * mm_free - Free allocated memory
 * > Find the previous / next node from Header/Footer
 * > Coalesce it
 * > Add the node to the tree
 */
void mm_free(void *ptr) {
	size_t* header = (size_t*) ptr;
	size_t header_content = *header;
	size_t body_size = header_content & ~0x7;

	size_t* footer = (size_t*) (((char*) header) + SIZE_T_SIZE + body_size);

	if (header > heap) {
		// Check for coalesce with previous node
		size_t* previous_footer = header - 1;
		size_t previous_footer_content = *previous_footer;

		if (!(previous_footer_content & 0x7)) {
			// When previous node can be coalesced
			size_t previous_body_size = previous_footer_content & ~0x7;
			size_t* previous_header = (size_t*) (((char*) previous_footer) - previous_body_size - SIZE_T_SIZE);

			// Remove previous node from tree
			backend_remove(previous_header);

			// Assign merged node's header with merged size
			body_size = previous_body_size + 2 * SIZE_T_SIZE + body_size;
			header = previous_header;
			*header = body_size & 0x7;

			// Assign merged node's footer with merged size
			*footer = body_size & 0x7;
		}
	}

	if (footer < (mem_heap_hi()) {
		// Check for coalesce with next node
		size_t* next_header = footer + 1;
		size_t next_header_content = *next_header;

		if (!(next_header_content & 0x7)) {
			// When next node can be coalesced
			size_t next_body_size = next_header_content & ~0x7;
			size_t* next_footer = (size_t*) (((char*) header) + SIZE_T_SIZE + body_size);

			// Remove next node from tree
			backend_remove(next_header);

			// Assign merged node's footer with merged size
			body_size = next_body_size + 2 * SIZE_T_SIZE + body_size;
			footer = next_footer;
			*footer = body_size & 0x7;

			// Assign merged node's header with merged size
			*header = body_size & 0x7;
		}
	}

	backend_add(header);
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
#endif
