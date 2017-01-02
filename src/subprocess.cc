/** @file subprocess.cc
 *
 *  Process handle - platform independent code.
 *  @author Lukasz A. Bartnik <l.bartnik@gmail.com>
 */

#include "subprocess.h"

#include <cstdlib>

#define MB_PARSE_ERROR ((size_t)-1)
#define MB_INCOMPLETE  ((size_t)-2)



namespace subprocess {


size_t pipe_writer::read (pipe_handle_type _fd, bool _mbcslocale) {
  if (_mbcslocale) {
    memcpy(contents.data(), left.data, left.len);
  }
  else {
    left.len = 0;
  }
  
  size_t rc = os_read(_fd);

  // end with 0 to make sure R can create a string out of the data block
  rc += left.len;
  contents[rc] = 0;

  // if there is a partial multi-byte character at the end, keep
  // it around for the next read attempt
  if (_mbcslocale) {
    left.len = 0;
    
    // check if all bytes are correct UTF8 content
    size_t consumed = consume_utf8(contents.data(), rc);
    if (consumed == MB_PARSE_ERROR || (rc - consumed > 4)) {
      throw subprocess_exception(EIO, "malformed multibyte string");
    }
    if (consumed < (size_t)rc) {
      left.len = rc-consumed;
      memcpy(left.data, contents.data()+consumed, left.len);
      contents[consumed] = 0;
      rc = consumed;
    }
  }

  return rc;
}


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



} /* namespace subprocess */
