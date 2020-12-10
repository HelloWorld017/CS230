#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_LINE 8192

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main(int argc, char** argv) {
	struct sockaddr_in clientaddr;
	int clientlen = sizeof(clientaddr);
	pthread_t tid;
	int listenfd = open_listenfd(argv[1]);
	printf("Listening on %s...", argv[1]);

	while (1) {
		int* connfdp = malloc(sizeof(int));
		*connfdp = accept(listenfd, (SA*) &clientaddr, &clientlen);
		pthread_create(&tid, NULL, receive_thread, connfdp);
	}

	return 0;
}

int expect(char** ptr_ptr, const char* cmp) {
	char* start_ptr = *ptr_ptr;

	if (start_ptr == NULL)
		return 0;

	if (!strncmp(start_ptr, cmp, len)) {
		*ptr_ptr = start_ptr + len;
		return 1;
	}

	return 0;
}

int advance_while(char** ptr_ptr, const char* delimiter) {
	char* start_ptr = *ptr_ptr;

	if (start_ptr == NULL)
		return 0;

	size_t delim_len = strlen(delimiter);
	int consume = 0;

	while (1) {
		if (**ptr_ptr == '\0') {
			return consume;
		}

		if (strncmp(*ptr_ptr, cmp, delim_len)) {
			return consume;
		}

		consume += delim_len;
		*ptr_ptr += delim_len;
	}
}

int advance_till(char** ptr_ptr, const char* delimiter) {
	char* start_ptr = *ptr_ptr;

	if (start_ptr == NULL)
		return 0;

	size_t delim_len = strlen(delimiter);
	int consume;

	while (1) {
		if (**ptr_ptr == '\0')
			return 0;

		if (!strncmp(*ptr_ptr, cmp, delim_len)) {
			return consume;
		}

		consume++;
		*ptr_ptr++;
	}
}

char* match(char** ptr_ptr, const char* delimiter) {
	char* start_ptr = *ptr_ptr;
	if (start_ptr == NULL)
		return NULL;

	size_t len = advance_till(ptr_ptr, delimiter);

	char* buffer = (char*) malloc(len + 1);
	strncpy(buffer, start_ptr, len);
	buffer[len] = '\0';

	return buffer;
}

typedef struct {
	char* key;
	char* value;
} Header;

typedef struct {
	int method;
	char* host;
	char* path;
	char* version;
	Header* headers[];
	unsigned header_length;
	char* body;
} Request;

#define METHOD_UNKNOWN -1
#define METHOD_GET 0

int parse_method(char* method) {
	if (!strcmp(method, "GET"))
		return METHOD_GET;

	return METHOD_UNKNOWN;
}

#define VERSION_UNKNOWN -1
#define VERSION_HTTP_1_0 0
#define VERSION_HTTP_1_1 1

int parse_version(char* version) {
	if (!strcmp(version, "HTTP/1.0"))
		return VERSION_HTTP_1_0;

	if (!strcmp(version, "HTTP/1.1"))
		return VERSION_HTTP_1_1;

	return VERSION_UNKNOWN;
}

#define MAX_HEADERS 128

int parse_header(rio_t rio, Header** headers, int* status) {
	Header* header = malloc(sizeof(Header) * MAX_HEADERS);
	*headers = header;

	for (int i = 0; i < MAX_HEADERS; i++) {
		char[MAX_LINE] line;
		if (rio_readlineb(rio, line, MAX_LINE) <= 0) {
			return i;
		}

		if (expect(&line, "\r\n"))
			return i;

		header->key = match(&line, ":");
		expect(&line, " ");
		header->value = match(&line, "\r");

		int parse_success = expect(&line, "\n");
		if (!parse_success) {
			*status = 400;
			return i;
		}

		header++;
	}
}

Request* parse_request(rio_t rio, int* status) {
	char[MAX_LINE] line;
	if (rio_readlineb(rio, line, MAX_LINE) <= 0) {
		*status = 400;
		return NULL;
	}

	// Parse method, host, path and http version
	int parse_success = 1;
	char* method = match(&line, " ");
	parse_success &= expect(&line, " ");
	parse_success &= expect(&line, "http://");

	char* host = match(&line, "/");
	char* path = match(&line, " ");
	char* version = match(&line, "\r");
	parse_success &= expect(&line, "\n");

	if (!parse_success) {
		*status = 400;
		return NULL;
	}

	Request* request = malloc(sizeof(Request));
	request->method = parse_method(method);
	if (request->method == METHOD_UNKNOWN) {
		*status = 405;
		return NULL;
	}

	request->host = host;
	request->path = path;
	request->version = parse_version(version);
	if (request->version == VERSION_UNKNOWN) {
		*status = 505;
		return NULL;
	}

	*status = 200;
	request->header_length = parse_header(rio, &request->headers, status);

	if (*status != 200)
		return NULL;

	return request;
}

void dump_request(Request* req) {
	if (!req) {
		printf("Failed to parse request!\n");
		return;
	}

	printf("Version: %s\n", req->version);
	printf("Method: %s\n", req->method);
	printf("Host: %s\n", req->host);
	printf("Path: %s\n", req->path);
	printf("Headers ========\n");
	for (int i = 0; i < req->header_length; i++) {
		printf("Key: %s, Value: %s\n", req->headers[i]->key, req->headers[i]->value);
	}
}

void* receive_thread(void* vargp) {
	int connfd = *((int*) vargp);
	pthread_detach(pthread_self());
	free(vargp);

	rio_t rio;
	rio_readinitb(&rio, connfd);

	int status;
	Request* req = parse_request(rio, &status);
	dump_request(req);
	
	close(connfd);
	return NULL;
}