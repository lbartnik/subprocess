#ifndef SUBPROCESS_H_GUARD
#define SUBPROCESS_H_GUARD

#include "config-win.h"
#include "utf8.h"

#define BUFFER_SIZE 1024

#include <cerrno>
#include <cstring>
#include <vector>

using std::vector;



#ifdef SUBPROCESS_WINDOWS // Windows
  #include "win-reader.h"
  typedef HANDLE process_handle;
  typedef HANDLE pipe_handle;
#else // Linux
  #include <unistd.h>
  typedef pid_t process_handle;
  typedef int pipe_handle;
#endif

typedef enum { PIPE_STDIN = 0, PIPE_STDOUT = 1, PIPE_STDERR = 2, PIPE_BOTH = 3 } pipe_t;

typedef enum { NOT_STARTED, RUNNING, EXITED, TERMINATED, TORNDOWN } state_t;

typedef enum { TERMINATION_GROUP, TERMINATION_CHILD_ONLY } termination_mode_t;




/**
 * Pipe Output is a buffer for output stream.
 * 
 * This buffer comes with additional logic of handling a number of
 * bytes left from the previous read that did not constitute a
 * correct multi-byte character.
 */
struct pipe_output {
  
  static constexpr size_t buffer_size = 1024;
  
  struct leftover {
    leftover () : len(0) { }
    size_t len;
    char data[4];
    
    static_assert(sizeof(data) < buffer_size);
  };
  
  typedef vector<char> container_type;
  
  container_type contents;
  leftover left;

  /**
   * Throws if buffer is too small.
   */
  pipe_output () : contents(buffer_size, 0) { }

  const container_type::value_type * data () const { return contents.data(); }

  void clear () { contents[0] = 0; }
  
  /**
   * Read from file descriptor.
   * 
   * Will accommodate for previous leftover and will keep a single
   * byte to store 0 at the end of the input data. That guarantees
   * that R string can be correctly constructed from buffer's data
   * (R expects a ZERO at the end).
   * 
   * @param _fd Input file descriptor.
   * @param _mbcslocale Is this multi-byte character set? If so, verify
   *        string integrity after a successful read.
   */    
  ssize_t read (int _fd, bool _mbcslocale = false) {
    if (_mbcslocale) {
      memcpy(contents.data(), left.data, left.len);
    }
    else {
      left.len = 0;
    }
    
    ssize_t rc = ::read(_fd, contents.data() + left.len, contents.size() - left.len - 1);
    if (rc < 0) {
      return rc;
    }

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
        errno = EIO;
        return -1;
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
  
};




struct process_handle_t {
#ifdef SUBPROCESS_WINDOWS
  reader_t stdout_reader, stderr_reader;
  HANDLE process_job;
#endif

  // OS-specific handles
  process_handle child_handle;

  pipe_handle pipe_stdin,
              pipe_stdout,
              pipe_stderr;

  // platform-independent process data
  int child_id;
  state_t state;
  int return_code;

  /* how should the process be terminated */
  termination_mode_t termination_mode;
  
  /* stdout & stderr handling */
  pipe_output stdout, stderr;
};






int full_error_message(char * _buffer, size_t _length);


int spawn_process (process_handle_t * _handle, const char * _command, char *const _arguments[],
	               char *const _environment[], const char * _workdir, termination_mode_t _termination_mode);

int teardown_process (process_handle_t * _handle);

ssize_t process_write (process_handle_t * _handle, const void * _buffer, size_t _count);

ssize_t process_read (process_handle_t & _handle, pipe_t _pipe, int _timeout);

int process_poll (process_handle_t * _handle, int _timeout);

int process_terminate (process_handle_t * _handle);

int process_kill(process_handle_t * _handle);

int process_send_signal(process_handle_t * _handle, int _signal);

#endif /* SUBPROCESS_H_GUARD */

