#include "libhttp.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libmill.h>

char* http_recv_line(tcpsock socket) {
	char* line = calloc(HTTP_MAX_LINE_LENGTH + 1, 1);
	if(!line) {
		return NULL;
	}

	size_t length = tcprecvuntil(socket, line, HTTP_MAX_LINE_LENGTH,
								 "\n", 1,
								 now() + 1000);

	if(errno != 0 || strlen(line) != length) {
		free(line);
		return NULL;
	}

	return line;
}



http_error_t http_version_parse(http_version_t* version,
								const char** string_pointer) {
	const char* string = *string_pointer;
	if(strlen(string) < 3)
		return HTTP_ERROR_SYNTAX;

	bool valid = (isdigit(string[0]) &&
				  string[1] == '.' &&
				  isdigit(string[2]));
	if(!valid)
		return HTTP_ERROR_SYNTAX;

	version->maj = string[0] - '0';
	version->min = string[2] - '0';
	*string_pointer += 3;
	return HTTP_OK;
}

char* http_version_to_string(const http_version_t* version) {
	char buffer[100];
	sprintf(buffer, "%u.%u", version->maj, version->min);
	return strdup(buffer);
}



http_error_t http_header_init(http_header_t* header) {
	http_error_t e = http_dict_init(&header->dict);
	if(e != HTTP_OK)
		return e;
	header->version.min = 1;
	header->version.maj = 1;
	return HTTP_OK;
}

void http_header_dispose(http_header_t* header) {
	http_dict_dispose(&header->dict);
}



static char* request_first_line_to_string(const http_request_t* request) {
	char* version = http_version_to_string(&request->header.version);
	if(!version) {
		return NULL;
	}
	size_t size = (strlen(request->method) +
				   strlen(request->uri) +
				   strlen(version));
	char* line = malloc(size + 10);
	if(!line) {
		free(version);
		return NULL;
	}
	sprintf(line, "%s %s HTTP/%s", request->method, request->uri, version);
	free(version);
	return line;
}

char* http_request_to_string(const http_request_t* request,
							 const char* line_ending) {
	char* first_line = request_first_line_to_string(request);
	char* dict = http_dict_to_string(&request->header.dict, line_ending);
	if(!first_line || !dict) {
		free(first_line);
		free(dict);
		return NULL;
	}
	char* s = malloc(strlen(first_line) + strlen(dict) +
					 strlen(line_ending) * 2 + 1);
	if(s)
		sprintf(s, "%s%s%s%s", first_line, line_ending, dict, line_ending);
	free(first_line);
	free(dict);
	return s;
}

static void request_print(const http_request_t* request) {
	char* s = http_request_to_string(request, "\n");
	printf("%s", s);
	free(s);
}

static http_error_t request_parse_http_version(http_request_t* request,
											   const char** string_pointer) {
	if(httpu_skip_string(string_pointer, "HTTP/") != 0)
		return HTTP_ERROR_SYNTAX;
	return http_version_parse(&request->header.version, string_pointer);
}

static http_error_t request_parse_first_line(http_request_t* request,
											 const char* line) {
	request->method = httpu_read_until(&line, " ");
	if(!request->method) {
		return HTTP_ERROR_SYNTAX;
	}
	request->uri = httpu_read_until(&line, " ");
	if(!request->uri) {
		free(request->method);
		return HTTP_ERROR_SYNTAX;
	}

	http_error_t e = request_parse_http_version(request, &line);
	if(e != HTTP_OK) {
		free(request->method);
		free(request->uri);
		return HTTP_ERROR_SYNTAX;
	}
	return HTTP_OK;
}

http_error_t http_request_recv(http_request_t* request, tcpsock socket) {
	char* line = http_recv_line(socket);
	if(!line) {
		return HTTP_ERROR_SYNTAX;
	}

	http_error_t e = http_header_init(&request->header);
	if(e != HTTP_OK) {
		free(line);
		return e;
	}

	e = request_parse_first_line(request, line);
	free(line);
	if(e != HTTP_OK) {
		http_header_dispose(&request->header);
		return e;
	}

	http_dict_t dict;
	e = http_dict_recv(&dict, socket);
	if(e != HTTP_OK) {
		http_header_dispose(&request->header);
		return e;
	}

	http_dict_dispose(&request->header.dict);
	request->header.dict = dict;

	return HTTP_OK;
}

void http_request_dispose(http_request_t* request) {
	free(request->method);
	free(request->uri);
	http_header_dispose(&request->header);
}

/*
static http_error_t request_parse_first_line(http_request_t* request,
											 const char* line) {
	bool res = false;

	char* method = 0;
	// HTTP method must be less than 10 chars long
	size_t szm = httpu_substr_delim(&method, line, 10, " ", 1);
	if(szm == 0 || szm >= 10) {
		goto exit;
	}

	line += szm + 1;

	char* uri = 0;
	// HTTP URI must be less than 2048 chars long
	size_t szu = httpu_substr_delim(&uri, line, 2048, " ", 1);
	if(szu == 0 || szu >= 2048) {
		goto exit;
	}

	line += szu + 1;

	char* proto = 0;
	size_t szp = httpu_substr_delim(&proto, line, 8 + 1, "\r\n", 2);
	if(szp != 8) {
		goto exit;
	}

	res = true;
	req->vmaj = line[5] - '0';
	req->vmin = line[7] - '0';
	req->method = strdup(method);
	req->uri = strdup(uri);

exit:
	free(method);
	free(uri);
	free(proto);

	return res;
}

void http_response_format(http_response_t* res, char** buf) {
	const char* line = "HTTP/%d.%d %d %s\r\n";
	const char* header = "%s: %s\r\n";

	char* str = 0;

	char* lineb = malloc(100);
	if(!lineb) {
		return;
	}

	sprintf(lineb, line, res->vmaj, res->vmin, res->status, httpu_status_str(res->status));

	str = calloc(1, strlen(lineb) + 2 + 1);
	if(!str) {
		return;
	}

	memcpy(str, lineb, strlen(lineb));
	free(lineb);

	for(size_t i = 0; i < res->headers.count; ++i) {
		char* n = *(res->headers.names + i);
		char* v = *(res->headers.values + i);

		size_t hl = strlen(n) + 2 + strlen(v) + 2;
		char* headerb = malloc(hl + 1);

		if(!headerb) {
			errno = ENOMEM;
			return;
		}

		sprintf(headerb, header, n, v);

		str = realloc(str, strlen(str) + strlen(headerb) + 1);
		if(!str) {
			errno = ENOMEM;
			return;
		}

		strcat(str, headerb);
		free(headerb);
	}

	char* ends = "\r\n";
	strcat(str, ends);

	if(res->body && res->bodylen > 0) {
		char* body = malloc(res->bodylen + 1);
		memcpy(body, res->body, res->bodylen);
		body[res->bodylen] = 0;

		str = realloc(str, strlen(str) + res->bodylen + 1);
		strcat(str, body);
		free(body);
	}

	*buf = str;
}
*/

coroutine void client(http_server_t serv, tcpsock socket) {
	http_request_t request;
	http_error_t e = http_request_recv(&request, socket);
	if(e != HTTP_OK) {
		fprintf(stderr, "client(): Error\n");
	} else {
		printf("client(): OK\n");
		request_print(&request);
		http_request_dispose(&request);
	}
}

bool http_listen(http_server_t server, const char* addrs, int port,
				 int backlog) {
	ipaddr addr = iplocal(addrs, port, 0);
	if(errno != 0) {
		return false;
	}

	tcpsock tcp = tcplisten(addr, backlog);
	if(errno != 0) {
		return false;
	}

	while(true) {
		tcpsock socket = tcpaccept(tcp, -1);
		if(!socket || errno != 0) {
			continue;
		}

		go(client(server, socket));
	}

	return true;
}

const char* http_get_status_string(int status) {
	switch(status) {
		case 100:
			return "Continue";
		case 101:
			return "Switching Protocols";
		case 102:
			return "Processing";
		case 200:
			return "OK";
		case 201:
			return "Created";
		case 202:
			return "Accepted";
		case 203:
			return "Non-Authoritative Information";
		case 204:
			return "No Content";
		case 205:
			return "Reset Content";
		case 206:
			return "Partial Content";
		case 207:
			return "Multi-Status";
		case 210:
			return "Content Different";
		case 226:
			return "IM Used";
		case 300:
			return "Multiple Choices";
		case 301:
			return "Moved Permanently";
		case 302:
			return "Moved Temporarily";
		case 303:
			return "See Other";
		case 304:
			return "Not Modified";
		case 305:
			return "Use Proxy";
		case 306:
			return "Reserved";
		case 307:
			return "Temporary Redirect";
		case 308:
			return "Permanent Redirect";
		case 310:
			return "Too many Redirects";
		case 400:
			return "Bad Request";
		case 401:
			return "Unauthorized";
		case 402:
			return "Payment Required";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 406:
			return "Not Acceptable";
		case 407:
			return "Proxy Authentication Required";
		case 408:
			return "Request Time-out";
		case 409:
			return "Conflict";
		case 410:
			return "Gone";
		case 411:
			return "Length Required";
		case 412:
			return "Precondition Failed";
		case 413:
			return "Request Entity Too Large";
		case 414:
			return "Request-URI Too Long";
		case 415:
			return "Unsupported Media Type";
		case 416:
			return "Requested range unsatisfiable";
		case 417:
			return "Expectation failed";
		case 418:
			return "I’m a teapot";
		case 421:
			return "Bad mapping";
		case 422:
			return "Unprocessable entity";
		case 423:
			return "Locked";
		case 424:
			return "Method failure";
		case 425:
			return "Unordered Collection";
		case 426:
			return "Upgrade Required";
		case 428:
			return "Precondition Required";
		case 429:
			return "Too Many Requests";
		case 431:
			return "Request Header Fields Too Large";
		case 449:
			return "Retry With";
		case 450:
			return "Blocked by Windows Parental Controls";
		case 451:
			return "Unavailable For Legal Reasons";
		case 456:
			return "Unrecoverable Error";
		case 500:
			return "Internal Server Error";
		case 501:
			return "Not Implemented";
		case 502:
			return "Bad Gateway";
		case 503:
			return "Service Unavailable";
		case 504:
			return "Gateway Time-out";
		case 505:
			return "HTTP Version not supported";
		case 506:
			return "Variant also negociate";
		case 507:
			return "Insufficient storage";
		case 508:
			return "Loop detected";
		case 509:
			return "Bandwidth Limit Exceeded";
		case 510:
			return "Not extended";
		case 511:
			return "Network authentication required";
		case 520:
			return "Web server is returning an unknown error";
		default:
			return "";
	}
}
