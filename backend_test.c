#include <stdlib.h>
#include <time.h>
#include "mm.c"

size_t add(int count, size_t size, Node** ptr) {
	size = ALIGN(size);
	printf("Alloc (%u)", size);
	fflush(stdout);

	Node* node = malloc(size);

	node->size = (size & ~0x7);
	node->child_l = NULL;
	node->child_r = NULL;
	*(((size_t*) (((char*) node) + size)) - 1) = (size & ~0x7);

	printf("[%d]: %u\n", count, size);
	backend_add(node);

	*ptr = node;
	return size;
}

size_t random_add(int count, Node** ptr) {
	return add(count, rand() % 1020 + 24, ptr);
}

int main(void) {
	backend_init();
	// backend_debug();

	size_t sizes[378];
	Node* ptrs[378];
	for (int i = 0; i < 378; i++) {
		// sizes[i] = random_add(i, &ptrs[i]);
		sizes[i] = add(i, i + BACKEND_MIN_SIZE, &ptrs[i]);
	}

	for (int i = 0; i < 378; i++) {
		printf("Find: %u\n", sizes[i]);
		if (rand() % 10 < 5) {
			Node* node = backend_pop(sizes[i]);
			if (node == NULL) {
				printf("Failed to pop!: %u\n", sizes[i]);
				backend_debug();
				return 1;
			}

			size_t node_size = _backend_size(node);
			printf("Pop [%d]: %u => %u\n", i, sizes[i], node_size);
		} else {
			backend_remove(ptrs[i]);
			printf("Remove [%d]: %u\n", i, sizes[i]);
		}
		if (backend_debug_silent() < 0) {
			printf("Loop Detected!\n");
			backend_debug();
			exit(1);
		}
	}

	printf("Done! :D\n");
}
