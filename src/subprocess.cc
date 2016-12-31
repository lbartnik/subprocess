/** @file subprocess.cc
 *
 *  Process handle - platform independent code.
 *  @author Lukasz A. Bartnik <l.bartnik@gmail.com>
 */

#include "subprocess.h"

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


} /* namespace subprocess */
