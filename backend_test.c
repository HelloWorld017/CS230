#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "mm.c"

//#define TEST_PRINT 0
#ifdef TEST_PRINT
#define PRINT(fmt, a...) printf(fmt, ##a)
#else
#define PRINT(fmt, a...) do{}while(0)
#endif

size_t add(int count, size_t size, size_t** ptr) {
	size = ALIGN(size);
	PRINT("Alloc (%u)", size);
	fflush(stdout);

	size_t* node = malloc(size);

	*node = (size & ~0x7);
	*(((size_t*) (((char*) node) + size)) - 1) = (size & ~0x7);

	PRINT("[%d]: %u\n", count, size);
	backend_add(node);

	*ptr = node;
	return size;
}

size_t random_add(int count, size_t** ptr) {
	return add(count, rand() % 1020 + 24, ptr);
}

int main(void) {
	struct timeval tv, tve, elapsed;
	gettimeofday(&tv, NULL);

	backend_init();
	// backend_debug();

	size_t sizes[32768];
	size_t* ptrs[32768];
	for (int i = 0; i < 32768/2; i++) {
		sizes[2 * i] = random_add(i, &ptrs[2 * i]);
		sizes[2 * i + 1] = add(i, 128 + BACKEND_MIN_SIZE, &ptrs[2 * i + 1]);
	}

	int breakpoint = -1;

	backend_debug();

	for (int i = 0; i < 32768/2; i++) {
		if (i == breakpoint) {
			backend_debug();
		}

		if (rand() % 10 < 9) {
			PRINT("Pop: %u\n", sizes[i]);
			Node* node = (Node*) backend_pop(sizes[i]);
			if (node == NULL) {
				PRINT("Failed to pop!: %u\n", sizes[i]);
				backend_debug();
				return 1;
			}

			size_t node_size = _backend_size(node);
			PRINT("Pop [%d]: %u => %u\n", i, sizes[i], node_size);
		} else {
			PRINT("Remove: %u\n", sizes[i]);
			backend_remove(ptrs[i]);
			PRINT("Remove [%d]: %u\n", i, sizes[i]);
		}
		if (backend_debug_silent() < 0) {
			PRINT("Loop Detected!\n");
			backend_debug();
			exit(1);
		}
	}

	for (int i = 0; i < 32768/2; i++) {
		sizes[i] = random_add(i, &ptrs[i]);
	}

	PRINT("Done! :D\n");
	gettimeofday(&tve, NULL);

	timersub(&tve, &tv, &elapsed);
	double diff = ((double) elapsed.tv_sec) + (((double) elapsed.tv_usec) * 1e-6);
	printf("Elapsed: %f s\n",  diff);
}
