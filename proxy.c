#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_LINE 8192

/* You won't lose style points for including this long line in your code */
static char *user_agent_hdr = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";

char* clone_string(char* str) {
	/* size_t size = strlen(str) + 1;
	char* copied = malloc(size);
	memcpy(copied, str, size);
	return copied;*/
	return strdup(str);
}

unsigned expect(char** ptr_ptr, const char* cmp) {
	char* start_ptr = *ptr_ptr;
	if (start_ptr == NULL)
		return 0;

	size_t len = strlen(cmp);
	if (!strncmp(start_ptr, cmp, len)) {
		*ptr_ptr = start_ptr + len;
		return 1;
	}

	return 0;
}

int advance_till(char** ptr_ptr, const char* delimiter) {
	char* start_ptr = *ptr_ptr;

	if (start_ptr == NULL)
		return 0;

	size_t delim_len = strlen(delimiter);
	int consume = 0;

	while (1) {
		if (**ptr_ptr == '\0')
			return 0;

		if (!strncmp(*ptr_ptr, delimiter, delim_len)) {
			return consume;
		}

		consume++;
		(*ptr_ptr)++;
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

#define MAX_HEADERS 128
typedef struct {
	int method;
	char* path;
	int version;
	Header* headers;
	unsigned header_length;
	char* body;
} Request;

typedef struct {
	char* host;
	unsigned port;
	char* path;
	// There's no query, hash, ...
} URL;

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

#define APPEND_HEADER 4
int parse_header(rio_t* rio, Header** headers, int* status) {
	Header header[MAX_HEADERS];

	int i = 0;
	for (; i < MAX_HEADERS; i++) {
		char line_buffer[MAX_LINE];

		if (rio_readlineb(rio, line_buffer, MAX_LINE) <= 0) {
			i--;
			break;
		}

		char* line = line_buffer;

		if (expect(&line, "\r\n"))
			break;

		header[i].key = match(&line, ":");
		if (!expect(&line, ": ")) {
			*status = 400;
			free(header[i].key);
			i--;
			break;
		}

		header[i].value = match(&line, "\r");
		if (!expect(&line, "\r\n")) {
			*status = 431;
			free(header[i].key);
			free(header[i].value);
			i--;
			break;
		}
	}

	size_t header_size = sizeof(Header) * (i + APPEND_HEADER);
	*headers = malloc(header_size);
	memcpy(*headers, header, header_size);

	return i;
}

Request* parse_request(rio_t* rio, int* status) {
	char line_buffer[MAX_LINE];
	if (rio_readlineb(rio, line_buffer, MAX_LINE) <= 0) {
		*status = 400;
		return NULL;
	}

	char* line = line_buffer;

	// Parse method, host, path and http version
	unsigned parse_success = 1;
	char* method = match(&line, " ");
	parse_success &= expect(&line, " ");

	char* url = match(&line, " ");
	parse_success &= expect(&line, " ");
	char* version = match(&line, "\r");
	parse_success &= expect(&line, "\r\n");

	if (!parse_success) {
		*status = 400;
		return NULL;
	}

	Request* request = malloc(sizeof(Request));
	request->method = parse_method(method);
	free(method);

	if (request->method == METHOD_UNKNOWN) {
		*status = 405;
		return NULL;
	}

	request->path = url;
	request->version = parse_version(version);
	free(version);

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

void parse_url(char* url_str, URL* url) {
	char** ptr = &url_str;
	if (expect(ptr, "http://")) {
		char* host_with_port_original = match(ptr, "/");
		char* host_with_port = host_with_port_original;
		url->host = match(&host_with_port, ":");
		if (expect(&host_with_port, ":")) {
			// Parse port
			int last_errno = errno;
			url->port = strtol(host_with_port, NULL, 10);
			errno = last_errno;

			if (!url->port || url->port < 0 || url->port > 65535)
				url->port = 80;
		} else {
			url->port = 80;
		}

		free(host_with_port_original);
	} else {
		url->host = NULL;
		url->port = 0;
	}

	url->path = clone_string(*ptr);
}

void add_header(Header* headers, unsigned* header_size, char* key, char* value) {
	if (value == NULL || key == NULL)
		return;

	// Should clone each key and value, so that they can easily freed
	for (int i = 0; i < *header_size; i++) {
		if(!strcmp(headers[i].key, key)) {
			headers[i].value = clone_string(value);
			return;
		}
	}

	headers[*header_size].key = clone_string(key);
	headers[*header_size].value = clone_string(value);
	(*header_size)++;
}

URL* transform_request(Request* req) {
	URL* url = malloc(sizeof(URL));
	parse_url(req->path, url);
	free(req->path);
	add_header(req->headers, &req->header_length, "Host", url->host);
	add_header(req->headers, &req->header_length, "User-Agent", user_agent_hdr);
	add_header(req->headers, &req->header_length, "Connection", "close");
	add_header(req->headers, &req->header_length, "Proxy-Connection", "close");
	req->path = clone_string(url->path);
	return url;
}

void free_request(Request* req) {
	for (int i = 0; i < req->header_length; i++) {
		free(req->headers[i].key);
		free(req->headers[i].value);
	}

	free(req->headers);
	free(req->path);
	free(req);
}

void free_url(URL* url) {
	if (url->host)
		free(url->host);

	free(url->path);
	free(url);
}

void dump_request(Request* req, int status) {
	if (status != 200) {
		printf("Failed to parse request with status %d!\n", status);
		return;
	}

	printf("======== HTTP REQUEST ========\n");
	printf("Version: %d\n", req->version);
	printf("Method: %d\n", req->method);
	printf("Path: %s\n", req->path);
	printf("Headers ------\n");
	for (int i = 0; i < req->header_length; i++) {
		printf("Key: %s, Value: %s\n", req->headers[i].key, req->headers[i].value);
	}
	printf("\n");
}

void dump_url(URL* url) {
	printf("======== URL ========\n");
	printf("Host: %s\n", url->host);
	printf("Path: %s\n", url->path);
	printf("Port: %d\n", url->port);
	printf("\n");
}

void write_text(int fd, const char* string) {
	rio_writen(fd, (void*) string, strlen(string));
}

int proxy_request(int fd, Request* req, URL* url) {
	char port[6], buff[4096];
	sprintf(port, "%d", url->port);

	int clientfd = open_clientfd(url->host, port);
	if (clientfd < 0)
		return -1;

	write_text(clientfd, "GET");
	write_text(clientfd, " ");
	write_text(clientfd, url->path);
	write_text(clientfd, " HTTP/1.0\r\n");
	for (int i = 0; i < req->header_length; i++) {
		write_text(clientfd, req->headers[i].key);
		write_text(clientfd, ": ");
		write_text(clientfd, req->headers[i].value);
		write_text(clientfd, "\r\n");
	}
	write_text(clientfd, "\r\n");

	int read;
	while ((read = rio_readn(clientfd, buff, 4096)) > 0) {
		if (rio_writen(fd, buff, read) < 0) {
			close(clientfd);
			return -2;
		}
	}

	close(clientfd);

	if (read < 0) {
		return -2;
	}

	return 0;
}

void response_deny(int fd, int status) {
	// The reason can be changed
	const char* response_format =
		"HTTP/1.0 %d Unknown Error\r\n"
  		"Server: Nenw Proxy\r\n"
  		"Connection: close\r\n"
		"Content-Length: 7\r\n"
  		"\r\n"
		"Error\r\n";

	char response[strlen(response_format) + 1];
	sprintf(response, response_format, status);
	rio_writen(fd, response, strlen(response));
}

#define DEBUG 1
void* receive_thread(void* vargp) {
	int connfd = *((int*) vargp);
	pthread_detach(pthread_self());
	free(vargp);

	rio_t rio;
	rio_readinitb(&rio, connfd);

	int status;
	Request* req = parse_request(&rio, &status);
	URL* url = transform_request(req);
#ifdef DEBUG
	dump_request(req, status);
	dump_url(url);
#endif
	if (!url->host) {
		status = 404;
	}

	if (status == 200) {
		int result = proxy_request(connfd, req, url);
		if (result == -1) {
			status = 500;
		}
	}

	if (status != 200) {
		response_deny(connfd, status);
	}

	free_request(req);
	free_url(url);
	close(connfd);
	return NULL;
}

int main(int argc, char** argv) {
	struct sockaddr_in clientaddr;
	unsigned clientlen = sizeof(clientaddr);
	pthread_t tid;

	if (argc < 1) {
		printf("Usage: proxy [port]\n");
		return 1;
	}

	int listenfd = open_listenfd(argv[1]);
	printf("Listening on %s...\n", argv[1]);

	while (1) {
		int* connfdp = malloc(sizeof(int));
		*connfdp = accept(listenfd, (SA*) &clientaddr, &clientlen);
		pthread_create(&tid, NULL, receive_thread, connfdp);
	}

	return 0;
}
