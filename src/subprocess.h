#ifndef SUBPROCESS_H_GUARD
#define SUBPROCESS_H_GUARD

#include "config-os.h"

// mbcslocale
#include <Rdefines.h>
#include <R_ext/GraphicsEngine.h>
#include <R_ext/GraphicsDevice.h>


/* In Visual Studio std::vector gets messed up by definitions in "R.h" */
#ifdef _MSC_VER
#undef length
#endif

#include <cerrno>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>


namespace subprocess {

constexpr int BUFFER_SIZE = 1024;


using std::string;
using std::runtime_error;
using std::vector;



enum pipe_type { PIPE_STDIN = 0, PIPE_STDOUT = 1, PIPE_STDERR = 2, PIPE_BOTH = 3 };


constexpr int TIMEOUT_IMMEDIATE = 0;

constexpr int TIMEOUT_INFINITE = -1;


string strerror (int _code, const string & _message);

size_t consume_utf8 (const char * _input, size_t _length);


/**
 * A simple exception class.
 */
struct subprocess_exception : runtime_error {

  /**
   * Create a new exception object.
   *
   * The local version of strerror is used in constructor to generate
   * the final error message and store it in the exception object.
   *
   * @param _code Operating-system-specific error code.
   * @param _message User-provided error message.
   */
  subprocess_exception (int _code, const string & _message)
    : runtime_error(strerror(_code, _message)), code(_code)
  { }

  /** Operating-system-specific error code. */
  const int code;

  /**
   * Store error message in a buffer.
   */
   void store (char * _buffer, size_t _length) {
     snprintf(_buffer, _length, "%s", what());
   }
};



/**
 * Buffer for a single output stream.
 * 
 * This buffer comes with additional logic of handling a number of
 * bytes left from the previous read that did not constitute a
 * correct multi-byte character.
 */
struct pipe_writer {
  
  static constexpr size_t buffer_size = 1024;
  
  struct leftover {
    leftover () : len(0) { }
    size_t len;
    char data[4];
    
    static_assert(sizeof(pipe_writer::leftover::data) < buffer_size,
                  "buffer too small for multi-byte char support");
  };
  
  typedef vector<char> container_type;
  
  container_type contents;
  leftover left;

  /**
   * Throws if buffer is too small.
   */
  pipe_writer () : contents(buffer_size, 0) { }

  const container_type::value_type * data () const { return contents.data(); }

  void clear () { contents[0] = 0; }

  size_t os_read (pipe_handle_type _pipe)
  {
    char * buffer = contents.data() + left.len;
    size_t length = contents.size() - left.len - 1;

#ifdef SUBPROCESS_WINDOWS
    DWORD dwAvail = 0, nBytesRead;
    
    // if returns FALSE and error is "broken pipe", pipe is gone
    if (!::PeekNamedPipe(_pipe, NULL, 0, NULL, &dwAvail, NULL)) {
      if (::GetLastError() == ERROR_BROKEN_PIPE) return 0;
      throw subprocess_exception(::GetLastError(), "could not peek into pipe");
    }
  
    if (dwAvail == 0)
      return 0;

    dwAvail = std::min((size_t)dwAvail, length);
    if (!::ReadFile(_pipe, buffer, dwAvail, &nBytesRead, NULL)) {
      throw subprocess_exception(::GetLastError(), "could not read from pipe");
    }

    return static_cast<size_t>(nBytesRead);
#else /* SUBPROCESS_WINDOWS */
    int rc = ::read(_pipe, buffer, length);
    if (rc < 0) {
      throw subprocess_exception(errno, "could not read from pipe");
    }
    return static_cast<size_t>(rc);
#endif /* SUBPROCESS_WINDOWS */
  }
  
  
  /**
   * Read from pipe.
   * 
   * Will accommodate for previous leftover and will keep a single
   * byte to store 0 at the end of the input data. That guarantees
   * that R string can be correctly constructed from buffer's data
   * (R expects a ZERO at the end).
   * 
   * @param _fd Input pipe handle.
   * @param _mbcslocale Is this multi-byte character set? If so, verify
   *        string integrity after a successful read.
   */    
  size_t read (pipe_handle_type _fd, bool _mbcslocale = false);

}; /* pipe_writer */



/**
 * Process handle.
 *
 * The main class in the package. This is where a single process state
 * is stored and where API to interact with that child process is
 * provided.
 */
struct process_handle_t {

  enum process_state_type { NOT_STARTED, RUNNING, EXITED, TERMINATED, SHUTDOWN };

  enum termination_mode_type { TERMINATION_GROUP, TERMINATION_CHILD_ONLY };

#ifdef SUBPROCESS_WINDOWS
  HANDLE process_job;
#endif

  // OS-specific handles
  process_handle_type child_handle;

  pipe_handle_type pipe_stdin,
                   pipe_stdout,
                   pipe_stderr;

  // platform-independent process data
  int child_id;
  process_state_type state;
  int return_code;

  /* how should the process be terminated */
  termination_mode_type termination_mode;
  
  /* stdout & stderr handling */
  pipe_writer stdout_, stderr_;

  process_handle_t ();
  
  ~process_handle_t () throw ()
  {
    try {
      shutdown();
    }
    catch (...) {
      // TODO be silent or maybe show a warning?
    }
  }

  void spawn(const char * _command, char *const _arguments[],
	                   char *const _environment[], const char * _workdir,
                     termination_mode_type _termination_mode);

  void shutdown();

  size_t write(const void * _buffer, size_t _count);

  size_t read(pipe_type _pipe, int _timeout);

  void close_input ();

  void wait(int _timeout);

  void terminate();

  void kill();

  void send_signal(int _signal);

};



} /* namespace subprocess */


#endif /* SUBPROCESS_H_GUARD */

