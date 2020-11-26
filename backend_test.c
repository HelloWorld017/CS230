#include <stdlib.h>
#include <time.h>
#include "mm.c"

size_t random_add(int count, size_t** ptr) {
	size_t size = ALIGN(rand() % 1020 + 24);
	printf("Alloc (%zd)", size);
	fflush(stdout);

	size_t* node = malloc(size);

	*node = (size & ~0x7);
	*(node + 1) = 0;
	*(node + 2) = 0;
	*(((size_t*) (((char*) node) + size)) - 1) = (size & ~0x7);

	printf("[%d]: %zd\n", count, size);
	backend_add(node);

	*ptr = node;
	return size;
}

int main(void) {
	backend_init();
	// backend_debug();

	size_t sizes[378];
	size_t* ptrs[378];
	for (int i = 0; i < 378; i++) {
		sizes[i] = random_add(i, &ptrs[i]);
	}

	// backend_debug();

	for (int i = 0; i < 378; i++) {
		printf("Find: %zd\n", sizes[i]);
		if (rand() % 10 < 5) {
			size_t* node = backend_pop(sizes[i]);
			if (node == NULL) {
				printf("Failed to pop!: %zd\n", sizes[i]);
				backend_debug();
				return 1;
			}

			size_t node_size = (*node) & ~0x7;
			printf("Pop [%d]: %zd => %zd\n", i, sizes[i], node_size);
		} else {
			size_t* node = backend_remove(ptrs[i]);
			if (node == NULL) {
				printf("Failed to remove!: %zd\n", sizes[i]);
				backend_debug();
				return 1;
			}

			size_t node_size = (*node) & ~0x7;
			printf("Remove [%d]: %zd => %zd\n", i, sizes[i], node_size);
		}
	}

	printf("Done! :D\n");
}
