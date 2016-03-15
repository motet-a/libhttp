#include "libhttp.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <libmill.h>


static void skip_until(const char** string_pointer, char c) {
	while(**string_pointer && **string_pointer != c)
		(*string_pointer)++;
}

bool httpu_starts_with(const char* string, const char* prefix) {
	return strncmp(prefix, string, strlen(prefix)) == 0;
}

int httpu_skip_string(const char** string_pointer, const char* string) {
	if(httpu_starts_with(*string_pointer, string)) {
		*string_pointer += strlen(string);
		return 0;
	}
	return -1;
}

char* httpu_read_until(const char** string_pointer,
					  const char* end_string) {
	const char* key_begin = *string_pointer;
	skip_until(string_pointer, end_string[0]);
	const char* key_end = *string_pointer;
	if(key_begin == key_end)
		return NULL;
	if(httpu_skip_string(string_pointer, end_string) == -1) {
		*string_pointer = key_begin;
		return NULL;
	}
	char* key = strndup(key_begin, key_end - key_begin);
	if(!key)
		*string_pointer = key_begin;
	return key;
}
