#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#include "utf8.h"

static int min (int a, int b) { return a<b ? a : b; }


size_t consume_utf8 (const char * _input, size_t _length)
{
  wchar_t wc;
  size_t used, consumed = 0;
  mbstate_t mb_st;
  
  if (!mbsinit(&mb_st)) memset(&mb_st,0,sizeof(mb_st));

  // if used > 0 we can just skip that many bytes and move on  
  while (_length > consumed) {
    used = mbrtowc(&wc, _input, min(MB_CUR_MAX, _length-consumed), &mb_st);
    // two situations when an error in encountered
    if (used == MB_INCOMPLETE) {
      return consumed;
    }
    if (used == MB_PARSE_ERROR) {
      return MB_PARSE_ERROR;
    }
    // correctly consumed multi-byte character
    if (used) {
      _input   += used;
      consumed += used;
    }
    // zero bytes consumed
    else {
      break;
    }
  }

  return consumed;
}
