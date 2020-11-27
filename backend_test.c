#include <stdlib.h>
#include <time.h>
#include "mm.c"

size_t add(int count, size_t size, size_t** ptr) {
	size = ALIGN(size);
	printf("Alloc (%u)", size);
	fflush(stdout);

	size_t* node = malloc(size);

	*node = (size & ~0x7);
	*(((size_t*) (((char*) node) + size)) - 1) = (size & ~0x7);

	printf("[%d]: %u\n", count, size);
	backend_add(node);

	*ptr = node;
	return size;
}

size_t random_add(int count, size_t** ptr) {
	return add(count, rand() % 1020 + 24, ptr);
}

int main(void) {
	backend_init();
	// backend_debug();

	size_t sizes[378];
	size_t* ptrs[378];
	for (int i = 0; i < 189; i++) {
		sizes[2 * i] = random_add(i, &ptrs[2 * i]);
		sizes[2 * i + 1] = add(i, i + BACKEND_MIN_SIZE, &ptrs[2 * i + 1]);
	}

	int breakpoint = -1;

	for (int i = 0; i < 378; i++) {
		if (i == breakpoint) {
			backend_debug();
		}

		if (rand() % 10 < 9) {
			printf("Pop: %u\n", sizes[i]);
			Node* node = (Node*) backend_pop(sizes[i]);
			if (node == NULL) {
				printf("Failed to pop!: %u\n", sizes[i]);
				backend_debug();
				return 1;
			}

			size_t node_size = _backend_size(node);
			printf("Pop [%d]: %u => %u\n", i, sizes[i], node_size);
		} else {
			printf("Remove: %u\n", sizes[i]);
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
