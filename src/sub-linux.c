#define _GNU_SOURCE             /* See feature_test_macros(7) */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>


#include "subprocess.h"

static const int BUFFER_SIZE = 1024;

static const int PIPE_READ  = 0;
static const int PIPE_WRITE = 1;

static const int TRUE = 1, FALSE = 0;


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
  _handle->state = NOT_STARTED;

  /* redirect standard streams */
  if (pipe(pipe_stdin) < 0) {
    return -1;
  }

  /* if there is an error preserve errno and close anything opened so far */
  if ((pipe2(pipe_stdout, O_NONBLOCK) < 0) || (pipe2(pipe_stderr, O_NONBLOCK) < 0)) {
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

  /* child is running */
  _handle->state = RUNNING;

  /* close those that the parent doesn't need */
  close(pipe_stdin[PIPE_READ]);
  close(pipe_stdout[PIPE_WRITE]);
  close(pipe_stderr[PIPE_WRITE]);

  _handle->pipe_stdin  = pipe_stdin[PIPE_WRITE];
  _handle->pipe_stdout = pipe_stdout[PIPE_READ];
  _handle->pipe_stderr = pipe_stderr[PIPE_READ];

  /* reset the NONBLOCK on stdout-read and stderr-read descriptors */
  fcntl(_handle->pipe_stdout, F_SETFL, fcntl(_handle->pipe_stdout, F_GETFL) | O_NONBLOCK);
  fcntl(_handle->pipe_stderr, F_SETFL, fcntl(_handle->pipe_stderr, F_GETFL) | O_NONBLOCK);

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

  // TODO there might be a need to send a termination signal first
  process_poll(_handle, TRUE);

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

  int rc = read(fd, _buffer, _count);
  if (rc < 0 && errno == EAGAIN) {
    /* stdin pipe is opened with O_NONBLOCK, so this means "would block" */
    errno = 0;
    *((char*)_buffer) = 0;
    return 0;
  }

  return rc;
}

int process_poll (process_handle_t * _handle, int _wait)
{
  if (!_handle->child_pid) {
    errno = ECHILD;
    return -1;
  }
  if (_handle->state != RUNNING) {
    return 0;
  }

  /* to wait or not to wait? */ 
  int options = 0;
  if (_wait == 0)
    options = WNOHANG;
  
  /* make the actual system call */
  int rc = waitpid(_handle->child_pid, &_handle->return_code, options);
  
  // there's been an error (<0) or the child is still running (==0)
  if (rc <= 0) {
    return rc;
  }

  // the child has exited or has been terminated
  if (WIFEXITED(_handle->return_code)) {
    _handle->state = EXITED;
    _handle->return_code = WEXITSTATUS(_handle->return_code);
  }
  else if (WIFSIGNALED(_handle->return_code)) {
    _handle->state = TERMINATED;
    _handle->return_code = WTERMSIG(_handle->return_code);
  }
  
  return 0;
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

