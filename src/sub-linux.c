#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


const int PIPE_READ  = 0;
const int PIPE_WRITE = 1;

const int BUFFER_SIZE = 1024;



struct process_handle {
  pid_t child_pid;
  int return_code;

  int pipe_stdin[2],
      pipe_stdout[2],
      pipe_stderr[2];
};

typedef struct process_handle process_handle_t;


/**
 * In most cases, when a negative value is returned the calling function
 * can consult the value of errno.
 * 
 * @return 0 on success and negative on an error.
 */
int spawn_process (process_handle_t * _handle, const char * _command, const char ** _arguments, const char ** _environment)
{
  int err;

  memset(_handle, 0, sizeof(process_handle_t));

  /* redirect standard streams */
  if (pipe(_handle->pipe_stdin) < 0) {
    return -1;
  }
  /* if there is an error preserve errno and close anything opened so far */
  if ((pipe(_handle->pipe_stdout) < 0) || (pipe(_handle->pipe_stderr) < 0)) {
    err = errno;
    teardown_process(_handle);
    errno = err;
    return -1;
  }

  /* spawn a child */
  _handle->child_pid = fork();

  if (_handle->child_pid < 0) {
    return -1;
  }

  /* child should copy his ends of pipes and close his and parent's
   * ends of pipes */
  if (_handle->child_pid == 0) {
    if (dup2(_handle->pipe_stdin[PIPE_READ], STDIN_FILENO) < 0) {
      perror("redirecting stdin failed, abortnig");
      exit(EXIT_FAILURE);
    }
    if (dup2(_handle->pipe_stdout[PIPE_WRITE], STDOUT_FILENO) < 0) {
      perror("redirecting stdout failed, abortnig");
      exit(EXIT_FAILURE);
    }
    if (dup2(_handle->pipe_stderr[PIPE_WRITE], STDERR_FILENO) < 0) {
      perror("redirecting stderr failed, abortnig");
      exit(EXIT_FAILURE);
    }

    /* redirection succeeded, now close all other descriptors */
    close(_handle->pipe_stdin[PIPE_READ]);
    close(_handle->pipe_stdin[PIPE_WRITE]);
    close(_handle->pipe_stdout[PIPE_READ]);
    close(_handle->pipe_stdout[PIPE_WRITE]);
    close(_handle->pipe_stderr[PIPE_READ]);
    close(_handle->pipe_stderr[PIPE_WRITE]);

    /* finally start the new process */
    execve(_command, _arguments, _environment);

    // TODO if we dup() STDERR_FILENO, we can print this message there
    //      rather then into the pipe
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "could not run command %s", _command);
    perror(message);

    exit(EXIT_FAILURE);
  }
}

int teardown_process (process_handle_t * _handle)
{
  if (_handle->child_pid > 0) {
    // TODO there might be a need to send a termination signal first
    waitpid(_handle->child_pid, &_handle->return_code, 0);
  }
}


int main (int argc, char ** argv)
{
  process_handle_t handle;
  const char * args[] = { NULL };
  const char * env[]  = { NULL };

  if (spawn_process(&handle, "/bin/bash", args, env) < 0) {
    perror("error in spawn_process()");
    exit(EXIT_FAILURE);
  }

  return 0;
}

