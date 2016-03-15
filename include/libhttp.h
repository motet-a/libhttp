#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <libmill.h>

// HTTP API

#define HTTP_MAX_LINE_LENGTH 8000
#define HTTP_MAX_FIELD_COUNT 100

/**
 * A lot of functions of the library returns an http_error_t.
 * If a function returns something other than HTTP_OK, it
 * encountered an error.
 */
typedef enum {
	HTTP_OK,
	HTTP_ERROR_MEMORY,
	HTTP_ERROR_SYNTAX,
} http_error_t;

typedef struct {
	char* name;
	char* value;
} http_field_t;

/**
 * A dictionnary mapping names to values.
 */
typedef struct {
	http_field_t* fields;
	size_t count;
	size_t allocated;
} http_dict_t;

typedef struct {
	int maj;
	int min;
} http_version_t;

typedef struct {
	http_version_t version;
	http_dict_t dict;
} http_header_t;

typedef struct {
	char* method;
	char* uri;

	http_header_t header;
} http_request_t;

typedef struct {
	int status;
	http_header_t header;

	char* body;
	size_t body_length;
} http_response_t;

typedef http_response_t* (*http_request_cb)(http_request_t*);

typedef struct {
	http_request_cb on_request;
} http_server_t;


/**
 * Reads a line from the given socket.
 *
 * Returns a malloced string or NULL on error.
 */
char* http_recv_line(tcpsock socket);

http_error_t http_field_init(http_field_t* field,
							 const char* name, const char* value);

void http_field_dispose(http_field_t* field);

http_error_t http_field_parse(http_field_t* field, const char* string);

char* http_field_to_string(const http_field_t* field, const char* line_ending);


http_error_t http_dict_init(http_dict_t* dict);
void http_dict_dispose(http_dict_t* dict);
http_error_t http_dict_set(http_dict_t* dict,
						   const char* name, const char* value);

bool http_dict_contains(const http_dict_t* dict, const char* name);

/**
 * Returns the value associated to the given name.
 *
 * Returns NULL if the given name is not found in the dictionnary.
 */
const char* http_dict_get(const http_dict_t* dict, const char* name);

/**
 * Returns the field associated to the given name.
 *
 * Returns NULL if the given name is not found in the dictionnary.
 */
http_field_t* http_dict_get_field(const http_dict_t* dict, const char* name);

/**
 * Reads HTTP header fields from the given socket.
 *
 * The fields are stored into the given `http_dict_t`.
 * Reads until the character sequence CR LF CR LF is encountered.
 */
http_error_t http_dict_recv(http_dict_t* dict, tcpsock socket);

char* http_dict_to_string(const http_dict_t* dict, const char* line_ending);



http_error_t http_version_parse(http_version_t* version,
								const char** string_pointer);
char* http_version_to_string(const http_version_t* version);



/** Creates a new HTTP/1.1 empty header */
http_error_t http_header_init(http_header_t* header);

void http_header_dispose(http_header_t* header);

http_error_t http_header_set(http_header_t* header,
							 const char* name, const char* value);



http_error_t http_request_init(http_request_t* request, const char* uri);
http_error_t http_request_recv(http_request_t* request, tcpsock socket);
void http_request_dispose(http_request_t* request);

http_error_t http_response_init(http_response_t* response, int status);
void http_response_dispose(http_response_t* res);

bool http_listen(http_server_t, const char* addr, int port, int backlog);

const char* http_get_status_string(int status);



bool httpu_starts_with(const char* string, const char* prefix);

int httpu_skip_string(const char** string_pointer, const char* string);

/**
 * Reads until `end_string` is encountered, and reads `end_string`.
 * Returns a malloced string on success.
 * Returns NULL if `end` is not encountered or on error.
 */
char* httpu_read_until(const char** string_pointer,
					   const char* end_string);
