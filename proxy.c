#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main(int argc, char** argv) {
	int port = atoi(argv[1]);
	struct sockaddr_in clientaddr;
	int clientlen = sizeof(clientaddr);
	pthread_t tid;
	int listenfd = open_listenfd(port);

	while (1) {
		int* connfdp = malloc(sizeof(int));
		*connfdp = accept(listenfd, (SA*) &clientaddr, &clientlen);
		pthread_create(&tid, NULL, thread, connfdp);
	}

	return 0;
}

/*
// What I wanted to make:
// * A thread pool with job queue implemented with lock-free queue
// * A robust parser parsing request on stream, not buffering full request
//
// But as I don't have so much time...
//
#define PARSER_STATE_FAILURE 0
#define PARSER_STATE_METHOD 1
#define PARSER_STATE_URI 2
#define PARSER_STATE_VERSION 3
#define PARSER_STATE_HEADER_KEY 4
#define PARSER_STATE_HEADER_VALUE 5
#define PARSER_RETURN_SUCCESS 0
#define PARSER_RETURN_FAILURE 1
#define PARSER_RETURN_NEED_MORE 2
#define PARSER_RETURN_TOO_LARGE 3
#define PARSER_BUFFER_SIZE 4096

typedef struct {
	int state;
	int buffer_size;
	char[PARSER_BUFFER_SIZE + 1] buffer;
} ParserState;

int expect(ParserState* state, char** ptr_ptr, const char* cmp) {
	char* ptr = *ptr_ptr;

	if (ptr == NULL)
		state->state = PARSER_STATE_FAILURE;

	if (state->state == PARSER_STATE_FAILURE) {
		*ptr_ptr = NULL;
		return PARSER_RETURN_FAILURE;
	}

	size_t my_len = strlen(ptr_ptr);
	size_t cmp_len = strlen(cmp);

	if (my_len + state->buffer_size < cmp_len) {
		if (state->buffer_size + my_len >= PARSER_BUFFER_SIZE) {
			state->state = PARSER_STATE_FAILURE;
			return PARSER_RETURN_TOO_LARGE;
		}

		strncpy(state->buffer + state->buffer_size, *ptr_ptr, my_len);
		state->buffer_size += my_len;
		state->buffer[state->buffer_size + 1] = NULL;
		return PARSER_RETURN_NEED_MORE;
	}

	if (!strncmp(ptr, cmp, len)) {
		return ptr + len;
	}

	return NULL;
}
*/

char* expect(char* start_ptr, const char* cmp) {
	if (start_ptr == NULL)
		return NULL;

	if (!strncmp(start_ptr, cmp, len)) {
		return start_ptr + len;
	}

	return NULL;
}

char* advance(char* start_ptr, const char* delimiter) {
	if (start_ptr == NULL)
		return NULL;

	size_t delim_len = strlen(delimiter);
	char* ptr = start_ptr;
	while (1) {
		if (*ptr == '\0')
			return NULL;

		if (!strncmp(ptr, cmp, delim_len)) {
			return ptr;
		}

		ptr++;
	}
}

char* match(char* start_ptr, const char* delimiter) {
	char* end_ptr = advance(start_ptr, delimiter);
	if (end_ptr == NULL)
		return NULL;

	size_t len = end_ptr - start_ptr;

	char* buffer = (char*) malloc(len + 1);
	strncpy(buffer, start_ptr, len);
	buffer[len] = '\0';

	return buffer;
}

void* thread(void* vargp) {
	int connfd = *((int*) vargp);
	pthread_detach(pthread_self());
	free(vargp);

	rio_t rio;
	rio_readinitb(&rio, connfd);

	// I wanted to parse the request on stream, not buffering whole request
	char buff[MAX_OBJECT_SIZE];
	while (fgets(buff, MAX_OBJECT_SIZE, connfd) != NULL) {
		expect("GET");
	}

	close(connfd);
	return NULL;
}