#ifndef MY_STRING_H_
#define MY_STRING_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "arena.h"

#define sizeof(x)    (ptrdiff_t)sizeof(x)
#define countof(a)   (sizeof(a) / sizeof(*(a)))
#define lengthof(s)  (countof(s) - 1)

#define str(s)    {(char *)s, lengthof(s)}
#define string(s)    (string)str(s)

#define MAX_FILE_LEN 255

typedef struct string_ {
    char *data;
    ptrdiff_t len;
} string;

typedef struct string_buffer_ {
    string str;
    ptrdiff_t cap;
} string_buffer;

// Copy the provided string data and return a new string.
string string_new_sized(char *data, ptrdiff_t len, arena *a);

// Copy the null-terminated string data and return a new string (without the terminator).
string string_new(char *data, arena *a);

// Formats the null-terminated c-string as a string (without the terminator).
string string_cast(char *data);

char *string_to_cstr(string st, arena *a);

string string_after(string str, ptrdiff_t idx);

string_buffer string_buffer_new(ptrdiff_t cap, arena *a);

void string_buffer_clear(string_buffer *buf);

ptrdiff_t string_buffer_remaining(string_buffer *buf);

// Returns number of characters appended.
ptrdiff_t string_buffer_add_str(string_buffer *buf, string str);

// Returns number of characters appended.
ptrdiff_t string_buffer_add_strn(string_buffer *buf, string str, ptrdiff_t maxlen);

// Returns number of characters appended.
ptrdiff_t string_buffer_add_int(string_buffer *buf, int32_t val);

// Returns whether the string was successfully terminated.
bool string_buffer_terminate(string_buffer *buf);

#endif // MY_STRING_H_
