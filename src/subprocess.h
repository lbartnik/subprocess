#ifndef SUBPROCESS_H_GUARD
#define SUBPROCESS_H_GUARD


#ifdef WIN64
  #include <windows.h>
  #undef ERROR // R.h already defines this
  typedef HANDLE process_handle;
  typedef HANDLE pipe_handle;

  #include "win-reader.h"
#else // Linux
  #include <unistd.h>
  typedef pid_t process_handle;
  typedef int pipe_handle;
#endif

typedef enum { PIPE_STDIN, PIPE_STDOUT, PIPE_STDERR } pipe_t;

typedef enum { NOT_STARTED, RUNNING, EXITED, TERMINATED } state_t;

struct process_handle {
#ifdef WIN64
  reader_t stdout_reader, stderr_reader;
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
};

typedef struct process_handle process_handle_t;

void full_error_message (char * _buffer, size_t _length);


int spawn_process (process_handle_t * _handle, const char * _command, char *const _arguments[], char *const _environment[]);

int teardown_process (process_handle_t * _handle);

ssize_t process_write (process_handle_t * _handle, const void * _buffer, size_t _count);

ssize_t process_read (process_handle_t * _handle, pipe_t _pipe, void * _buffer, size_t _count);

int process_poll (process_handle_t * _handle, int _wait);

#endif /* SUBPROCESS_H_GUARD */

