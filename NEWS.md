# subprocess 0.8.2

* fixes in test cases for `testthat` 2.0

# subprocess 0.8.1

* explicitly register native symbols

* Ctrl+C works in Windows

* multiple fixes in test code

# subprocess 0.8.0

* support for Mac OS

* shared read from both stdout and stderr of the child process

* new `process_close_input()` call to close the write end of child's
  standard input pipe; this in most cases will let the child know it
  should exit

* renamed `process_poll()` to `process_wait()`; add `process_state()`

* converted shared library to C++

* bugfix: group terminate in Windows


# subprocess 0.7.4

* initial submission to CRAN; basic API works in Linux and Windows
