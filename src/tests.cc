/** @file tests.cc
 *
 *  Native C++ unit tests.
 *  @author Lukasz A. Bartnik <l.bartnik@gmail.com>
 */

#include "subprocess.h"
#include "rapi.h"

#include <R.h>
#include <Rdefines.h>


#define expect_equal(a, b)                                     \
  do {                                                         \
    if (a != b) {                                              \
      Rf_warning(#a " not equal to " #b);                      \
      errors += 1;                                             \
    }                                                          \
  } while (0);                                                 \



using subprocess::consume_utf8;


// ---------------------------------------------------------------------

extern "C" SEXP test_consume_utf8 ()
{
  int errors = 0;
  if (!mbcslocale) {
    return allocate_single_int(0);
  }

  // correct ASCII input
  expect_equal(consume_utf8("", 0), 0);
  expect_equal(consume_utf8("a", 1), 1);
  expect_equal(consume_utf8("ab", 2), 2);
  expect_equal(consume_utf8("abc", 3), 3);
  expect_equal(consume_utf8("abcd", 4), 4);
  
  // multi-byte UTF8 characters; whole and split in the middle
  // https://en.wikipedia.org/wiki/UTF-8#Examples

  // ¢ (https://en.wikipedia.org/wiki/Cent_(currency)#Symbol)
  expect_equal(consume_utf8("a\xC2\xA2", 3), 3);
  expect_equal(consume_utf8("a\xC2", 2), 1);
  
  // € (https://en.wikipedia.org/wiki/Euro_sign)
  expect_equal(consume_utf8("a\xE2\x82\xAC", 4), 4);
  expect_equal(consume_utf8("a\xE2\x82", 3), 1);
  expect_equal(consume_utf8("a\xE2", 2), 1);

  // 𐍈 https://en.wikipedia.org/wiki/Hwair

 expect_equal(consume_utf8("a\xF0\x90\x8D\x88", 5), 5);
  expect_equal(consume_utf8("a\xF0\x90\x8D", 4), 1);
  expect_equal(consume_utf8("a\xF0\x90", 3), 1);
  expect_equal(consume_utf8("a\xF0", 2), 1);
  expect_equal(consume_utf8("a\xF0", 2), 1);

  return allocate_single_int(errors);
}
