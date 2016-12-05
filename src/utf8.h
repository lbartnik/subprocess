#ifndef UTF8_HEADER_GUARD
#define UTF8_HEADER_GUARD

#include <stdlib.h>

#define MB_PARSE_ERROR ((size_t)-1)
#define MB_INCOMPLETE  ((size_t)-2)


// mbcslocale
#include <Rdefines.h>
#include <R_ext/GraphicsEngine.h>
#include <R_ext/GraphicsDevice.h>


// utf8.c
size_t consume_utf8 (const char * _input, size_t _length);


#endif /* UTF8_HEADER_GUARD */
