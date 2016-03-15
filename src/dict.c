#include "libhttp.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <libmill.h>
#include <stdio.h>

http_error_t http_field_init(http_field_t* field,
							 const char* name, const char* value) {
	field->name = strdup(name);
	field->value = strdup(value);
	if(!field->name || !field->value) {
		free(field->name);
		free(field->value);
		return HTTP_ERROR_MEMORY;
	}
	return HTTP_OK;
}

void http_field_dispose(http_field_t* field) {
	free(field->name);
	free(field->value);
	free(field);
}

http_error_t http_field_parse(http_field_t* field, const char* string) {
	char* name = httpu_read_until(&string, ": ");
	if(!name)
		return HTTP_ERROR_SYNTAX;
	char* value = httpu_read_until(&string, "\r\n");
	if(!value || *string != '\0') {
		free(name);
		free(value);
		return HTTP_ERROR_SYNTAX;
	}
	field->name = name;
	field->value = value;
	return HTTP_OK;
}

char* http_field_to_string(const http_field_t* field,
						   const char* line_ending) {
	size_t length = (strlen(field->name) +
					 strlen(field->value) +
					 strlen(line_ending));

	char* s = malloc(length + 10);
	if(!s)
		return NULL;
	sprintf(s, "%s: %s%s", field->name, field->value, line_ending);
	return s;
}



/**
 * Returns -1 on error.
 */
http_error_t http_dict_init(http_dict_t* dict) {
	dict->allocated = 8;
	dict->count = 0;
	dict->fields = malloc(sizeof(http_field_t) * dict->allocated);
	if(!dict->fields) {
		return HTTP_ERROR_MEMORY;
	}
	return HTTP_OK;
}

void http_dict_dispose(http_dict_t* dict) {
	for(size_t i = 0; i < dict->count; i++) {
		http_field_dispose(dict->fields + i);
	}
	free(dict->fields);
}

static void dict_grow(http_dict_t* dict) {
	dict->allocated *= 2;
	dict->fields = realloc(dict->fields,
						   sizeof(http_field_t) * dict->allocated);
}

static http_error_t dict_add_field(http_dict_t* dict,
								   http_field_t* field) {
	if(dict->allocated == dict->count) {
		dict_grow(dict);
		if(!dict->fields) {
			return HTTP_ERROR_MEMORY;
		}
		assert(dict->allocated > dict->count);
	}
	dict->fields[dict->count] = *field;
	dict->count++;
	return HTTP_OK;
}

static http_error_t dict_add_field_strings(http_dict_t* dict,
										   const char* name,
										   const char* value) {
	http_field_t field;
	http_error_t e = http_field_init(&field, name, value);
	if(e != HTTP_OK)
		return e;
	e = dict_add_field(dict, &field);
	if(e != HTTP_OK) {
		http_field_dispose(&field);
		return e;
	}
	return HTTP_OK;
}

http_error_t http_dict_set(http_dict_t* dict,
						   const char* name, const char* value) {
	http_field_t* field = http_dict_get_field(dict, name);

	if(!field) {
		return dict_add_field_strings(dict, name, value);
	}

	return http_field_init(field, name, value);
}

bool http_dict_contains(const http_dict_t* dict, const char* name) {
	return http_dict_get_field(dict, name) != NULL;
}

const char* http_dict_get(const http_dict_t* dict, const char* name) {
	http_field_t* field = http_dict_get_field(dict, name);
	return field->name;
}

http_field_t* http_dict_get_field(const http_dict_t* dict, const char* name) {
	for(size_t i = 0; i < dict->count; i++) {
		http_field_t* field = dict->fields + i;
		if(strcmp(field->name, name) == 0)
			return field;
	}
	return NULL;
}

http_error_t http_dict_recv(http_dict_t* dict, tcpsock socket) {
	http_dict_init(dict);

	while(true) {
		char* line = http_recv_line(socket);
		if(!line) {
			http_dict_dispose(dict);
			return HTTP_ERROR_SYNTAX;
		}
		if(strcmp(line, "\r\n") == 0) {
			free(line);
			break;
		}
		http_field_t field;
		http_error_t e = http_field_parse(&field, line);
		free(line);
		if(e != HTTP_OK) {
			http_dict_dispose(dict);
			return e;
		}
		e = dict_add_field(dict, &field);
		if(e != HTTP_OK) {
			http_dict_dispose(dict);
			return e;
		}
	}
	return HTTP_OK;
}

char* http_dict_to_string(const http_dict_t* dict, const char* line_ending) {
	char* s = strdup("");
	if(!s)
		return NULL;

	for(size_t i = 0; i < dict->count; i++) {
		const http_field_t* field = dict->fields + i;
		char* field_string = http_field_to_string(field, line_ending);
		if(!field_string) {
			free(s);
			return NULL;
		}
		s = realloc(s, strlen(s) + strlen(field_string) + 1);
		if(!s) {
			free(field_string);
			return NULL;
		}
		strcat(s, field_string);
		free(field_string);
	}
	return s;
}
