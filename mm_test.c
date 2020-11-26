#include <stdlib.h>
#include <time.h>
#include "mm.c"

size_t random_add(int count) {
	size_t size = ALIGN(rand() % 1020 + 24);
	printf("Alloc (%d)", size);
	fflush(stdout);

	size_t* node = malloc(size);

	*node = (size & ~0x7);
	*(node + 1) = 0;
	*(node + 2) = 0;
	*(node + (size >> 3) - 1) = (size & ~0x7);

	printf("[%d]: %d\n", count, size);
	backend_add(node);
	return size;
}

int main(void) {
	void* heap = malloc(1024);
	backend_init(heap, 1024);
	// backend_debug();

	size_t sizes[378];
	for (int i = 0; i < 378; i++) {
		sizes[i] = random_add(i);
	}

	// backend_debug();

	for (int i = 0; i < 378; i++) {
		printf("Find: %d\n", sizes[i]);
		size_t* node = backend_pop(sizes[i]);
		if (node == NULL) {
			printf("Failed to pop!: %d\n", sizes[i]);
			backend_debug();
			return 1;
		}

		size_t node_size = (*node) & ~0x7;
		printf("Pop [%d]: %d => %d\n", i, sizes[i], node_size);
	}

	printf("Done! :D");
}
