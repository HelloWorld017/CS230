#include "mm.c"

int main(void) {
	void* heap = malloc(1024);
	backend_init(heap, 1024);
	backend_debug();
}