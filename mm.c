/*
 * mm.c - nenw*'s Malloc Implementation
 * > It's "malloc", not "murloc"
 *
 * Implemented using Doubly-Linked List and AVL Tree for indexing
 * > Find free memory using AVL Tree, sorted by Size -> Address
 * > Coalesce using Doubly-linked List
 * > Originally, it was implemented with AA Tree but,
 * >     as it was too slow, I have rewrote whole code into AVL Tree
 *
 * Block Format:
 * > Legend
 * >> ^: For AVL Tree
 * >> a (1bit): Allocated or not
 *
 * > Allocated
 * >> |  Size        | a | (size_t)
 * >> |  Padding         | (void) (As it forces me to align payload)
 * >> |  Payload         | (void)
 * >> |  Padding         | (void)
 * >> |  Size        | a | (size_t)
 *
 * > Free
 * >> |  Size              | a  | (size_t)
 * >> |  ChildL^                | (size_t)
 * >> |  ChildR^                | (size_t)
 * >> |  Height^                | (unsigned)
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
#include "memlib.h"

#ifdef BACKEND_DEBUG
	#define BACKEND_DEBUG_PRINT(fmt, args...) printf(fmt, ##args)
#else
	#define BACKEND_DEBUG_PRINT(fmt, args...) do {} while (0)
#endif

#ifdef MM_DEBUG
	#define MM_DEBUG_PRINT(fmt, args...) printf(fmt, ##args)
#else
	#define MM_DEBUG_PRINT(fmt, args...) do {} while (0)
#endif

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define ALIGNMENT_LOG 3

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (sizeof(size_t))
#define SIZE_T_SIZE_PAD (ALIGN(SIZE_T_SIZE) - SIZE_T_SIZE)
#define SIZE_T_SIZE_PADDED ALIGN(SIZE_T_SIZE)

/*
 * Backend
 * ====
 *
 * A data structure implementation
 */

typedef struct _Node {
	size_t size;
	struct _Node* child_l;
	struct _Node* child_r;
	unsigned height: 16;
} Node;

#define BACKEND_MIN_SIZE (ALIGN(sizeof(Node)) + SIZE_T_SIZE_PADDED)
Node* root;

/*
 * _backend_factor: Get balance factor
 */
inline int _backend_factor(Node* node) {
	int left_height = node->child_l ? node->child_l->height : 0;
	int right_height = node->child_r ? node->child_r->height : 0;

	return left_height - right_height;
}

inline void _backend_update_height(Node* node) {
	BACKEND_DEBUG_PRINT("Update height!\n");
	int left_height = node->child_l ? node->child_l->height : 0;
	int right_height = node->child_r ? node->child_r->height : 0;

	node->height = (left_height < right_height ? right_height : left_height) + 1;
}

inline size_t _backend_size(Node* node) {
	return (node->size) & ~0x7;
}

/*
 * _backend_rotate_left: Apply left rotation operation to given node
 */
inline Node* _backend_rotate_left(Node* node) {
	// Get right node from node
	Node* right_node = node->child_r;

	// (right of node) is (left of right_node)
	node->child_r = right_node->child_l;

	// (left of right_node) is (node)
	right_node->child_l = node;
	_backend_update_height(node);

	// (node) is (right_node)
	return right_node;
}

/*
 * _backend_rotate_right: Apply right rotation operation to given node
 */
inline Node* _backend_rotate_right(Node* node) {
	// Get left node from node
	Node* left_node = node->child_l;

	// (left of node) is (right of left_node)
	node->child_l = left_node->child_r;

	// (right of left_node) is (node)
	left_node->child_r = node;
	_backend_update_height(node);

	// (node) is (left_node)
	return left_node;
}

/*
 * _backend_remove_rebalance:
 *     Make one level of the tree, which balance is broken, as a balanced
 */
inline Node* _backend_rebalance(Node* node, int where) {
	if (where == -1) return node;

	BACKEND_DEBUG_PRINT("Rebalance!\n");
	int factor = _backend_factor(node);
	if (factor > 1) {
		if (where == 1) {
			// LR case
			node->child_l = _backend_rotate_left(node->child_l);
			return _backend_rotate_right(node);
		} else {
			// LL case
			return _backend_rotate_right(node);
		}
	} else if (factor < -1) {
		if (where == 1) {
			// RR case
			return _backend_rotate_left(node);
		} else {
			// RL case
			node->child_r = _backend_rotate_right(node->child_r);
			return _backend_rotate_left(node);
		}
	}

	return node;
}

inline void backend_init(void) {
	root = NULL;
}

/*
 * backend_add: Add new node to the tree
 */
Node* _backend_add_node;
Node* _backend_add(Node* current_node, size_t node_size, int* where) {
	// If current node is null, assign and escape
	if (current_node == NULL) {
		return _backend_add_node;
	}

	// Get size of iterating node
	size_t current_size = _backend_size(current_node);
	BACKEND_DEBUG_PRINT("Add %u\n", current_size);

	int my_where = -1;

	// Decide where to descend
	// > Sort by size, then sort by address
	if (node_size > current_size) {
		// Descend to right
		current_node->child_r = _backend_add(current_node->child_r, node_size, &my_where);
		*where = 1;
	} else if (node_size < current_size) {
		// Descend to left
		current_node->child_l = _backend_add(current_node->child_l, node_size, &my_where);
		*where = 0;
	} else {
		if (_backend_add_node < current_node) {
			// Descend to left
			current_node->child_l = _backend_add(current_node->child_l, node_size, &my_where);
			*where = 0;
		} else {
			// Descend to right
			current_node->child_r = _backend_add(current_node->child_r, node_size, &my_where);
			*where = 1;
		}
	}

	return _backend_rebalance(current_node, my_where);
}

inline void backend_add(size_t* node_raw) {
	Node* node = (Node*) node_raw;
	size_t node_size = _backend_size(node);
	node->height = 0;
	node->child_l = NULL;
	node->child_r = NULL;

	_backend_add_node = node;

	int where;
	root = _backend_add(root, node_size, &where);
	BACKEND_DEBUG_PRINT("Done add!\n");
}

/*
 * _backend_successor: Find successor node
 */
inline Node* _backend_successor(Node* current_node) {
	if (current_node == NULL)
		return NULL;

	while (1) {
		// Keep descend to left

		Node* next_node = current_node->child_l;
		if (next_node == NULL) {
			// When we can't proceed, return final node
			return current_node;
		} else {
			// As we can proceed, keep proceed to next child
			current_node = next_node;
		}
	}
}

/*
 * backend_remove: Remove and return remain node
 *     If there's no such node, return null
 */
Node* _backend_remove(Node* current_node, Node* target_node) {
	// Base case
	if (current_node == NULL)
		return NULL;

	// Get size of iterating node
	size_t current_size = _backend_size(current_node);
	BACKEND_DEBUG_PRINT("Remove! %d\n", current_size);

	// The node to be replaced with current_node
	Node* next_node = current_node;

	// Descend to left or right
	if (_backend_size(target_node) < current_size) {
		// Descend to left
		current_node->child_l = _backend_remove(current_node->child_l, target_node);
	} else if (_backend_size(target_node) > current_size) {
		// Descend to right
		current_node->child_r = _backend_remove(current_node->child_r, target_node);
	} else {
		// Same size. From now, we sort by address
		if (target_node > current_node) {
			// Descend to right
			current_node->child_r = _backend_remove(current_node->child_r, target_node);
		} else if (target_node < current_node) {
			// Descend to left
			current_node->child_l = _backend_remove(current_node->child_l, target_node);
		} else {
			// Found the value

			// Remove it
			if (current_node->child_l && current_node->child_r) {
				BACKEND_DEBUG_PRINT("Remove > Full case\n");
				Node* successor = _backend_successor(current_node->child_r);
				Node* remain = _backend_remove(current_node->child_r, successor);

				// Replace current with successor
				successor->child_l = current_node->child_l;
				successor->child_r = remain;

				next_node = successor;
			} else {
				if (current_node->child_l) {
					BACKEND_DEBUG_PRINT("Remove > Left case\n");
					// Only left node
					next_node = current_node->child_l;
				} else if (current_node->child_r) {
					BACKEND_DEBUG_PRINT("Remove > Right case\n");
					// Only right node
					next_node = current_node->child_r;
				} else {
					BACKEND_DEBUG_PRINT("Remove > Leaf case\n");
					// Leaf node
					return NULL;
				}
			}
		}
	}

	// Rebalance current level
	_backend_update_height(next_node);

	int where = _backend_factor(next_node) > 1 ?
		(next_node->child_l ? (_backend_factor(next_node->child_l) < 0 ? 1 : 0) : 0) :
		(next_node->child_r ? (_backend_factor(next_node->child_r) <= 0 ? 1 : 0) : 0);

	return _backend_rebalance(next_node, where);
}

inline void backend_remove(size_t* target_node) {
	if (target_node == NULL)
		return;

	root = _backend_remove(root, (Node*) target_node);
	BACKEND_DEBUG_PRINT("Done remove!\n");
}

/*
 * backend_pop: Find smallest node which size is larger than given size.
 *     Remove and return the node
 *     If the find fails, returns null
 */
Node* _backend_pop(Node* current_node, Node** return_node, size_t target_size) {
	// Base case
	if (current_node == NULL) {
		return NULL;
	}

	// Get size of iterating node
	size_t current_size = _backend_size(current_node);
	BACKEND_DEBUG_PRINT("Iterate! %d\n", current_size);

	// The node to be replaced with current_node
	Node* next_node = current_node;

	if (target_size <= current_size) {
		// When the result is in the left node or current_node

		// Calculate left node
		int is_current = 0;

		if (current_node->child_l != NULL) {
			// If we have left child, first descend to left child
			BACKEND_DEBUG_PRINT("Left > ");
			Node* retval = _backend_pop(current_node->child_l, return_node, target_size);

			if ((*return_node) == NULL) {
				// If we cannot find closer candidate, the result is current_node
				is_current = 1;
			} else {
				// If we have found closer candidate, the result is returned value
				current_node->child_l = retval;
			}
		} else {
			// If we don't have any left child, the result is current_node
			is_current = 1;
		}

		if (is_current) {
			// If the result is current_node
			BACKEND_DEBUG_PRINT("Found! %d\n", current_size);

			// Change to remove mode for further iteration
			next_node = _backend_remove(current_node, current_node);
			*return_node = current_node;
		}
	} else {
		// When the result is in the right node
		if (current_node->child_r != NULL) {
			BACKEND_DEBUG_PRINT("Right > ");
			Node* retval = _backend_pop(current_node->child_r, return_node, target_size);

			if ((*return_node) == NULL) {
				// Couldn't find in right
				return current_node;
			}

			// Found in right
			current_node->child_r = retval;
		} else {
			// As there's no right node, we couldn't find
			BACKEND_DEBUG_PRINT("Not-Found //\n");
			return current_node;
		}
	}


	if (next_node == NULL)
		return next_node;

	// Rebalance the tree of current level
	_backend_update_height(next_node);

	int where = _backend_factor(next_node) > 1 ?
		(next_node->child_l ? (_backend_factor(next_node->child_l) < 0 ? 1 : 0) : 0) :
		(next_node->child_r ? (_backend_factor(next_node->child_r) <= 0 ? 1 : 0) : 0);

	return _backend_rebalance(next_node, where);
}

inline size_t* backend_pop(size_t size) {
	Node* return_node = NULL;
	root = _backend_pop(root, &return_node, size);

	BACKEND_DEBUG_PRINT("Done pop!\n");
	return (size_t*) return_node;
}

/*
 * backend_debug: Prints preorder traversal result
 */
int _backend_debug(Node* node, int lvl, int print) {
	// Looking for the blitz loop this planet to search way...
	if (print)
		for (int i = 0; i < lvl; i++)
			BACKEND_DEBUG_PRINT(" ");

	if (node == NULL) {
		if (print)
			BACKEND_DEBUG_PRINT("()\n");
		return 0;
	}

	if (node == node->child_l) {
		return -1;
	}

	if (node == node->child_r) {
		return -1;
	}

	int size = 1;
	if (print)
		BACKEND_DEBUG_PRINT("NODE %d: %d (\n", _backend_size(node), lvl);

	int output = _backend_debug(node->child_l, lvl + 1, print);
	if (output < 0) return -1;
	size += output;

	output = _backend_debug(node->child_r, lvl + 1, print);
	if (output < 0) return -1;
	size += output;

	if (print) {
		for (int i = 0; i < lvl; i++)
			BACKEND_DEBUG_PRINT(" ");

		BACKEND_DEBUG_PRINT(")\n");
	}
	return size;
}

#ifdef BACKEND_DEBUG
int backend_debug(void) {
	return _backend_debug(root, 0, 1);
}

int backend_debug_silent(void) {
	return _backend_debug(root, 0, 0);
}
#else
inline int backend_debug(void) { return 0; }
inline int backend_debug_silent(void) { return 0; }
#endif


/*
 * Malloc
 * ====
 *
 * A malloc, free, realloc implementation
 */

void* heap;

/*
 * _mm_header: Get the header of given node by its footer and size
 */
inline size_t* _mm_header(size_t* footer, size_t body_size) {
	return (size_t*) (((char*) footer) - body_size - SIZE_T_SIZE_PAD - SIZE_T_SIZE_PADDED);
}

/*
 * _mm_footer: Get the footer of given node by its header and size
 */
inline size_t* _mm_footer(size_t* header, size_t body_size) {
	return (size_t*) (((char*) header) + SIZE_T_SIZE_PADDED + body_size + SIZE_T_SIZE_PAD);
}

/*
 * mm_init: Initialize the malloc package
 */
int mm_init(void) {
	// Initialize memlib
	mem_init();

	// Get the aligned start position of heap
	void* start_heap = mem_heap_lo();
	heap = (void*) ALIGN((size_t) start_heap);

	// Claim some memory to align heap start
	mem_sbrk(heap - start_heap);

	// Initialize tree
	backend_init();

	MM_DEBUG_PRINT("\n==== NENW'S MALLOC ====\n");
	MM_DEBUG_PRINT("Alignment: %u\n", ALIGNMENT);
	MM_DEBUG_PRINT("Minimal Backend Block: %u\n", BACKEND_MIN_SIZE);

	return 0;
}

/* 
 * mm_malloc - Allocate memory
 * > Find first node larger than size
 * > Delete it from tree
 * > Add the node of remainder to the tree
 *
 * Warning: The node is aligned when it is allocated,
 *     but not aligned when it is free
 *     (Actually it is aligned to 4 in x86)
 */
void *mm_malloc(size_t size) {
	MM_DEBUG_PRINT("==== Alloc %d ====\n", size);
	MM_DEBUG_PRINT("Padding Mode: %d\n", SIZE_T_SIZE_PAD);
	size_t* allocated = backend_pop(size);
	size_t node_size = ALIGN(size) + 2 * SIZE_T_SIZE_PADDED;
	size_t body_size;

	if (allocated == NULL) {
		MM_DEBUG_PRINT("Claim Mode: sbrk\n");

		// Default when we cannot coalesce
		size_t* claim_start = (size_t*) ((char*) mem_heap_hi() + 1);
		size_t claim_size = node_size;

		// Calculate body size
		body_size = claim_size - 2 * SIZE_T_SIZE_PADDED;

		if (mem_heapsize() > 0) {
			MM_DEBUG_PRINT("Coalesce with left!\n");
			// Try to coalesce with previous memory
			size_t* footer = (size_t*) (((char*) claim_start) - SIZE_T_SIZE);

			// Make sure it is free, not allocated
			if(!((*footer) & 0x7)) {
				// We can earn size of (Header + Body + Footer)
				size_t prev_body_size = (*footer) & ~0x7;
				MM_DEBUG_PRINT("Earned memory: %u\n", prev_body_size + 2 * SIZE_T_SIZE_PADDED);
				claim_size -= prev_body_size + 2 * SIZE_T_SIZE_PADDED;

				// Calculate pointer to node and remove
				size_t* header = _mm_header(footer, prev_body_size);
				backend_remove(header);

				claim_start = header;
			}
		}

		MM_DEBUG_PRINT("Claim size: %d\n", claim_size);

		// Claim memory
		if (mem_sbrk(claim_size) < 0) {
			// When failed to claim more memory
			return NULL;
		}

		allocated = claim_start;
	} else {
		MM_DEBUG_PRINT("Claim Mode: from tree\n");
		// Calculate body size from header
		body_size = (*allocated) & ~0x7;

		// Check if we can split the allocated node into two parts
		size_t allocated_node_size = ALIGN(body_size) + 2 * SIZE_T_SIZE_PADDED;

		if (allocated_node_size > node_size + BACKEND_MIN_SIZE) {
			// Shrink body size
			body_size = node_size - 2 * SIZE_T_SIZE_PADDED;

			// Calculate body size of next node
			size_t next_body_size = allocated_node_size - node_size - 2 * SIZE_T_SIZE_PADDED;

			// Assign size to header of next node
			size_t* next_header = (size_t*) (((char*) allocated) + node_size);
			*next_header = ((next_body_size) & ~0x7);

			// Assign size to footer of next node
			size_t* next_footer = _mm_footer(next_header, next_body_size);
			*next_footer = ((next_body_size) & ~0x7);

			// Index next node to the tree
			backend_add(next_header);
		}
	}

	// Calculate start of body
	char* body_start = ((char*) allocated) + SIZE_T_SIZE_PADDED;
	MM_DEBUG_PRINT("Done claim for %d: %u\n", body_size, (size_t) body_start);

	// Assign size & allocated flag to header
	MM_DEBUG_PRINT("Write header\n");
	*allocated = ((body_size) & ~0x7) | 0x1;

	// Assign size & allocated flag to footer
	size_t* allocated_footer = (size_t*) (body_start + body_size + SIZE_T_SIZE_PAD);
	MM_DEBUG_PRINT("Write footer\n");
	*allocated_footer = ((body_size) & ~0x7) | 0x1;

	// Return start of body
	MM_DEBUG_PRINT("Bye bye my kawai memory\n");
	return (void*) body_start;
}

/*
 * mm_free - Free allocated memory
 * > Find the previous / next node from Header/Footer
 * > Coalesce it
 * > Add the node to the tree
 */
void mm_free(void *ptr) {
	MM_DEBUG_PRINT("==== Free (%u) ====\n", (size_t) ptr);
	size_t* header = (size_t*) (((char*) ptr) - SIZE_T_SIZE_PADDED);
	size_t header_content = *header;
	size_t body_size = header_content & ~0x7;

	MM_DEBUG_PRINT("Found Size: %d\n", body_size);

	size_t* footer = _mm_footer(header, body_size);

	if (header > (size_t*) heap) {
		// Check for coalesce with previous node
		size_t* previous_footer = header - 1;
		size_t previous_footer_content = *previous_footer;

		if (!(previous_footer_content & 0x7)) {
			MM_DEBUG_PRINT("Coalesce with Left!\n");

			// When previous node can be coalesced
			size_t previous_body_size = previous_footer_content & ~0x7;
			size_t* previous_header = _mm_header(previous_footer, previous_body_size);

			// Remove previous node from tree
			backend_remove(previous_header);

			// Assign merged node's header with merged size
			body_size += previous_body_size + 2 * SIZE_T_SIZE_PADDED;
			header = previous_header;
			*header = body_size & ~0x7;

			// Assign merged node's footer with merged size
			*footer = body_size & ~0x7;
		}
	}

	if (footer < (size_t*) (((char*) mem_heap_hi() + 1) - SIZE_T_SIZE_PAD)) {
		// Check for coalesce with next node
		size_t* next_header = footer + 1;
		size_t next_header_content = *next_header;

		if (!(next_header_content & 0x7)) {
			MM_DEBUG_PRINT("Coalesce with Right!\n");

			// When next node can be coalesced
			size_t next_body_size = next_header_content & ~0x7;
			size_t* next_footer = _mm_footer(next_header, next_body_size);

			// Remove next node from tree
			backend_remove(next_header);

			// Assign merged node's footer with merged size
			body_size += next_body_size + 2 * SIZE_T_SIZE_PADDED;
			footer = next_footer;
			*footer = body_size & ~0x7;

			// Assign merged node's header with merged size
			*header = body_size & ~0x7;
		}
	}

	MM_DEBUG_PRINT("Write Header / Footer\n");
	*header = body_size & ~0x7;
	*footer = body_size & ~0x7;

	backend_add(header);
	backend_debug();
	MM_DEBUG_PRINT("Hello, my memory\n");
}

/*
 * mm_realloc: Reallocate the memory
 * > If the next node is empty, malloc it and merge
 * > If not, malloc and free
 */
void *mm_realloc(void *old_allocation, size_t new_size) {
	MM_DEBUG_PRINT("==== Realloc (%u) | [%u] ====\n", (size_t) old_allocation, new_size);

	size_t* header = (size_t*) (((char*) old_allocation) - SIZE_T_SIZE_PADDED);
	size_t old_size = (*header) & ~0x7;
	new_size = ALIGN(new_size);

	MM_DEBUG_PRINT("Size: %u => %u\n", old_size, new_size);

	if (new_size > old_size) {
		// Check for coalesce with next node when growing allocated memory
		size_t* footer = _mm_footer(header, old_size);
		if (footer < (size_t*) (((char*) mem_heap_hi() + 1) - SIZE_T_SIZE_PAD)) {
			size_t* next_header = footer + 1;
			size_t next_header_content = *next_header;

			if (!(next_header_content & 0x7)) {
				MM_DEBUG_PRINT("Coalesce with Right!\n");

				// When next node can be coalesced
				size_t next_body_size = next_header_content & ~0x7;
				size_t* next_footer = _mm_footer(next_header, next_body_size);

				// Remove next node from tree
				backend_remove(next_header);

				// Assign merged node's footer with merged size
				old_size += next_body_size + 2 * SIZE_T_SIZE_PADDED;
				footer = next_footer;
				*footer = (old_size & ~0x7) | 0x1;

				// Assign merged node's header with merged size
				*header = (old_size & ~0x7) | 0x1;
			}
		}
	}

	if (new_size + BACKEND_MIN_SIZE < old_size) {
		// When some part is left and can be split
		MM_DEBUG_PRINT("Split left space!\n");

		// Calculate body size of next node
		size_t next_body_size = old_size - new_size - 2 * SIZE_T_SIZE_PADDED;
		MM_DEBUG_PRINT("Split result: %u => %u + %u\n", old_size, new_size, next_body_size);
		old_size = new_size;

		// Assign shrinked size
		size_t* footer = _mm_footer(header, old_size);
		*header = ((new_size) & ~0x7) | 0x1;
		*footer = ((new_size) & ~0x7) | 0x1;

		// Assign size to header of next node
		size_t* next_header = footer + 1;
		*next_header = ((next_body_size) & ~0x7);

		// Assign size to footer of next node
		size_t* next_footer = _mm_footer(next_header, next_body_size);
		*next_footer = ((next_body_size) & ~0x7);

		// Index next node to the tree
		backend_add(next_header);
	}

	if (old_size < new_size) {
		// Allocate new memory if not available
		MM_DEBUG_PRINT("Mode: malloc-and-free\n");

		void* new_allocation = mm_malloc(new_size);
		memcpy(new_allocation, old_allocation, old_size);
		mm_free(old_allocation);

		return new_allocation;
	} else {
		MM_DEBUG_PRINT("Mode: reuse\n");
		return old_allocation;
	}
}
