#ifndef SUBPROCESS_H_GUARD
#define SUBPROCESS_H_GUARD


#ifdef WIN64
#include <windows.h>
#undef ERROR
#else // Linux
#include <unistd.h>
#endif


typedef enum { PIPE_STDIN, PIPE_STDOUT, PIPE_STDERR } pipe_t;

typedef enum { NOT_STARTED, RUNNING, EXITED, TERMINATED } state_t;

struct process_handle {
  // OS-specific child process handle
#ifdef WIN64
  HANDLE child_handle;
#else // Linux
  pid_t child_pid;
#endif

  int child_id; // child process identifier
  state_t state;
  int return_code;

  int pipe_stdin,
      pipe_stdout,
      pipe_stderr;
};

typedef struct process_handle process_handle_t;


int spawn_process (process_handle_t * _handle, const char * _command, char *const _arguments[], char *const _environment[]);

int teardown_process (process_handle_t * _handle);

ssize_t process_write (process_handle_t * _handle, const void * _buffer, size_t _count);

ssize_t process_read (process_handle_t * _handle, pipe_t _pipe, void * _buffer, size_t _count);

int process_poll (process_handle_t * _handle, int _wait);

#endif /* SUBPROCESS_H_GUARD */

