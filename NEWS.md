# subprocess 0.8.0

* added support for Mac OS

* enabled shared read from both stdout and stderr of the child process

* added process_close_input() call to close the write end of child's
  standard input pipe; this in most cases will let the child know it
  should exit

* converted shared library to C++

# subprocess 0.7.4

* initial submission to CRAN; basic API works in Linux and Windows
