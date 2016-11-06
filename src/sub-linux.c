#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "subprocess.h"

const int BUFFER_SIZE = 1024;

// TODO prevent Ctrl+C from being passed to the child process

/**
 * In most cases, when a negative value is returned the calling function
 * can consult the value of errno.
 * 
 * @return 0 on success and negative on an error.
 */
int spawn_process (process_handle_t * _handle, const char * _command, char * const _arguments[], char * const _environment[])
{
  int err;
  int pipe_stdin[2], pipe_stdout[2], pipe_stderr[2];

  memset(_handle, 0, sizeof(process_handle_t));

  /* redirect standard streams */
  if (pipe(pipe_stdin) < 0) {
    return -1;
  }
  /* if there is an error preserve errno and close anything opened so far */
  if ((pipe(pipe_stdout) < 0) || (pipe(pipe_stderr) < 0)) {
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
    if (dup2(pipe_stdin[PIPE_READ], STDIN_FILENO) < 0) {
      perror("redirecting stdin failed, abortnig");
      exit(EXIT_FAILURE);
    }
    if (dup2(pipe_stdout[PIPE_WRITE], STDOUT_FILENO) < 0) {
      perror("redirecting stdout failed, abortnig");
      exit(EXIT_FAILURE);
    }
    if (dup2(pipe_stderr[PIPE_WRITE], STDERR_FILENO) < 0) {
      perror("redirecting stderr failed, abortnig");
      exit(EXIT_FAILURE);
    }

    /* redirection succeeded, now close all other descriptors */
    close(pipe_stdin[PIPE_READ]);
    close(pipe_stdin[PIPE_WRITE]);
    close(pipe_stdout[PIPE_READ]);
    close(pipe_stdout[PIPE_WRITE]);
    close(pipe_stderr[PIPE_READ]);
    close(pipe_stderr[PIPE_WRITE]);

    /* finally start the new process */
    execve(_command, _arguments, _environment);

    // TODO if we dup() STDERR_FILENO, we can print this message there
    //      rather then into the pipe
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "could not run command %s", _command);
    perror(message);

    exit(EXIT_FAILURE);
  }

  /* close those that the parent doesn't need */
  close(pipe_stdin[PIPE_READ]);
  close(pipe_stdout[PIPE_WRITE]);
  close(pipe_stderr[PIPE_WRITE]);

  _handle->pipe_stdin  = pipe_stdin[PIPE_WRITE];
  _handle->pipe_stdout = pipe_stdout[PIPE_READ];
  _handle->pipe_stderr = pipe_stderr[PIPE_READ];

  return 0;
}

int teardown_process (process_handle_t * _handle)
{
  if (!_handle || !_handle->child_pid) {
    errno = ECHILD;
    return -1;
  }

  close(_handle->pipe_stdin);
  close(_handle->pipe_stdout);
  close(_handle->pipe_stderr);

  if (_handle->child_pid > 0) {
    // TODO there might be a need to send a termination signal first
    waitpid(_handle->child_pid, &_handle->return_code, 0);
  }

  return 0;
}

ssize_t process_write (process_handle_t * _handle, const void * _buffer, size_t _count)
{
  if (!_handle || !_handle->child_pid) {
    errno = ECHILD;
    return -1;
  }

  return write(_handle->pipe_stdin, _buffer, _count);
}

ssize_t process_read (process_handle_t * _handle, pipe_t _pipe, void * _buffer, size_t _count)
{
  if (!_handle || !_handle->child_pid) {
    errno = ECHILD;
    return -1;
  }

  int fd;
  if (_pipe == PIPE_STDOUT)
    fd = _handle->pipe_stdout;
  else if (_pipe == PIPE_STDERR)
    fd = _handle->pipe_stderr;
  else {
    errno = EINVAL;
    return -1;
  }

  return read(fd, _buffer, _count);
}


#ifdef LINUX_TEST
int main (int argc, char ** argv)
{
  process_handle_t handle;
  char * const args[] = { NULL };
  char * const env[]  = { NULL };

  char buffer[BUFFER_SIZE];

  if (spawn_process(&handle, "/bin/bash", args, env) < 0) {
    perror("error in spawn_process()");
    exit(EXIT_FAILURE);
  }

  process_write(&handle, "echo A\n", 7);
  process_read(&handle, PIPE_STDOUT, buffer, sizeof(buffer));

  printf("stdout: %s\n", buffer);

  teardown_process(&handle);

  return 0;
}
#endif /* LINUX_TEST */

