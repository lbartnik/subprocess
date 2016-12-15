#ifndef SUBPROCESS_H_GUARD
#define SUBPROCESS_H_GUARD

#include "config-win.h"

#define BUFFER_SIZE 1024


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

struct leftover {
  size_t len;
  char data[4];
};

struct process_handle {
#ifdef SUBPROCESS_WINDOWS
  reader_t stdout_reader, stderr_reader;
  HANDLE process_job;
#else
  struct leftover stdout_left, stderr_left;
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
};

typedef struct process_handle process_handle_t;

int full_error_message(char * _buffer, size_t _length);


int spawn_process (process_handle_t * _handle, const char * _command, char *const _arguments[],
	               char *const _environment[], const char * _workdir, termination_mode_t _termination_mode);

int teardown_process (process_handle_t * _handle);

ssize_t process_write (process_handle_t * _handle, const void * _buffer, size_t _count);


struct pipe_output {
  char stdout[BUFFER_SIZE], stderr[BUFFER_SIZE];
};

ssize_t process_read (process_handle_t * _handle, pipe_t _pipe, struct pipe_output * _output, int _timeout);

int process_poll (process_handle_t * _handle, int _timeout);

int process_terminate (process_handle_t * _handle);

int process_kill(process_handle_t * _handle);

int process_send_signal(process_handle_t * _handle, int _signal);

#endif /* SUBPROCESS_H_GUARD */

