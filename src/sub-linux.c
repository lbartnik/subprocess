#define _GNU_SOURCE             /* See feature_test_macros(7) */

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>

#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>


#include "subprocess.h"

static const int BUFFER_SIZE = 1024;

static const int PIPE_READ  = 0;
static const int PIPE_WRITE = 1;

static const int TRUE = 1, FALSE = 0;



int full_error_message (char * _buffer, size_t _length)
{
  if (strerror_r(errno, _buffer, _length) == 0)
    return 0;
  return -1;
}


static int clock_millisec ()
{
  struct timespec current;
  clock_gettime(CLOCK_REALTIME, &current);
  return current.tv_sec * 1000 + current.tv_nsec / 1000000;
}

static int timed_read (int _fd, char * _buffer, size_t _count, int _timeout);

static int set_non_block (int _fd)
{
  return fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL) | O_NONBLOCK);
}

static int set_block (int _fd)
{
  return fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL) & (~O_NONBLOCK));
}

// this is to hide from CRAN that we call exit()
static void exit_on_failure ()
{
  void * process_handle = dlopen(NULL, RTLD_NOW);
  void * exit_handle = dlsym(process_handle, "exit");
  
  // it's hard to imagine a situation where this symbol would not be
  // present; regardless, we cause a SEGMENTATION error because the
  // child needs to die;
  // also, we use write because CRAN will warn about fprintf(stderr)
  if (!exit_handle) {
    const char * message = "could not dlopen() the exit() function, going to SEGFAULT\n";
    write(STDERR_FILENO, message, strlen(message));
    *(int*)exit_handle = 0;
  }
  
  typedef void (* exit_t)(int);
  exit_t exit_fun = (exit_t)exit_handle;
  exit_fun(EXIT_FAILURE);
}


// TODO prevent Ctrl+C from being passed to the child process

/**
 * In most cases, when a negative value is returned the calling function
 * can consult the value of errno.
 *
 * @return 0 on success and negative on an error.
 */
int spawn_process (process_handle_t * _handle, const char * _command, char *const _arguments[],
	               char *const _environment[], const char * _workdir, termination_mode_t _termination_mode)
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
  if ((pipe2(pipe_stdout, O_NONBLOCK) < 0)) {
    err = errno;
    close(pipe_stdin[PIPE_READ]);
    close(pipe_stdin[PIPE_WRITE]);
    errno = err;
    return -1;
  }

  if ((pipe2(pipe_stderr, O_NONBLOCK) < 0)) {
    err = errno;
    close(pipe_stdin[PIPE_READ]);
    close(pipe_stdin[PIPE_WRITE]);
    close(pipe_stdout[PIPE_READ]);
    close(pipe_stdout[PIPE_WRITE]);
    errno = err;
    return -1;
  }


  /* spawn a child */
  _handle->child_id = fork();

  if (_handle->child_id < 0) {
    return -1;
  }

  /* child should copy his ends of pipes and close his and parent's
   * ends of pipes */
  if (_handle->child_id == 0) {
    if (dup2(pipe_stdin[PIPE_READ], STDIN_FILENO) < 0) {
      perror("redirecting stdin failed, abortnig");
      exit_on_failure();
    }
    if (dup2(pipe_stdout[PIPE_WRITE], STDOUT_FILENO) < 0) {
      perror("redirecting stdout failed, abortnig");
      exit_on_failure();
    }
    if (dup2(pipe_stderr[PIPE_WRITE], STDERR_FILENO) < 0) {
      perror("redirecting stderr failed, abortnig");
      exit_on_failure();
    }

    /* redirection succeeded, now close all other descriptors */
    close(pipe_stdin[PIPE_READ]);
    close(pipe_stdin[PIPE_WRITE]);
    close(pipe_stdout[PIPE_READ]);
    close(pipe_stdout[PIPE_WRITE]);
    close(pipe_stderr[PIPE_READ]);
    close(pipe_stderr[PIPE_WRITE]);

    /* change directory */
    if (_workdir != NULL) {
      if (chdir(_workdir) < 0) {
        char message[BUFFER_SIZE];
        snprintf(message, sizeof(message), "could not change working directory to %s",
                 _workdir);
        perror(message);
        exit_on_failure();
      }
    }

    /* if termination mode is "group" start new session */
    if (_termination_mode == TERMINATION_GROUP) {
      if (setsid() == (pid_t)-1) {
        perror("could not start a new session");
        exit_on_failure();
      }
    }

    /* finally start the new process */
    execve(_command, _arguments, _environment);

    // TODO if we dup() STDERR_FILENO, we can print this message there
    //      rather then into the pipe
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "could not run command %s", _command);
    perror(message);

    exit_on_failure();
  }

  /* child is running */
  _handle->state = RUNNING;
  _handle->termination_mode = _termination_mode;

  /* close those that the parent doesn't need */
  close(pipe_stdin[PIPE_READ]);
  close(pipe_stdout[PIPE_WRITE]);
  close(pipe_stderr[PIPE_WRITE]);

  _handle->pipe_stdin  = pipe_stdin[PIPE_WRITE];
  _handle->pipe_stdout = pipe_stdout[PIPE_READ];
  _handle->pipe_stderr = pipe_stderr[PIPE_READ];

  /* reset the NONBLOCK on stdout-read and stderr-read descriptors */
  set_non_block(_handle->pipe_stdout);
  set_non_block(_handle->pipe_stderr);

  return 0;
}

int teardown_process (process_handle_t * _handle)
{
  if (!_handle || !_handle->child_id) {
    errno = ECHILD;
    return -1;
  }

  if (_handle->pipe_stdin) close(_handle->pipe_stdin);
  if (_handle->pipe_stdout) close(_handle->pipe_stdout);
  if (_handle->pipe_stderr) close(_handle->pipe_stderr);

  // TODO there might be a need to send a termination signal first
  process_poll(_handle, TRUE);

  return 0;
}


ssize_t process_write (process_handle_t * _handle, const void * _buffer, size_t _count)
{
  if (!_handle || !_handle->child_id) {
    errno = ECHILD;
    return -1;
  }

  return write(_handle->pipe_stdin, _buffer, _count);
}


ssize_t process_read (process_handle_t * _handle, pipe_t _pipe, void * _buffer, size_t _count, int _timeout)
{
  if (!_handle || !_handle->child_id) {
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

  // finite timeout
  if (_timeout > 0) {
    return timed_read(fd, _buffer, _count, _timeout);
  }

  // infinite timeout
  if (_timeout < 0) {
    set_block(fd);
    int rc = read(fd, _buffer, _count);
    set_non_block(fd);
    return rc;
  }

  // no timeout
  int rc = read(fd, _buffer, _count);
  if (rc < 0 && errno == EAGAIN) {
    /* stdin pipe is opened with O_NONBLOCK, so this means "would block" */
    errno = 0;
    *((char*)_buffer) = 0;
    return 0;
  }

  return rc;
}


int process_poll (process_handle_t * _handle, int _timeout)
{
  if (!_handle->child_id) {
    errno = ECHILD;
    return -1;
  }
  if (_handle->state != RUNNING) {
    return 0;
  }

  /* to wait or not to wait? */ 
  int options = 0;
  if (_timeout >= 0)
    options = WNOHANG;
  
  /* make the actual system call */
  int start = clock_millisec(), rc;
  do {
    rc = waitpid(_handle->child_id, &_handle->return_code, options);
  
    // there's been an error (<0)
    if (rc < 0) {
      return rc;
    }

    _timeout -= clock_millisec() - start;
  } while (rc == 0 && _timeout > 0);

  // the child is still running
  if (rc == 0) {
    return 0;
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


int process_send_signal(process_handle_t * _handle, int _signal)
{
  return kill(_handle->child_id, _signal);
}



static int termination_signal (process_handle_t * _handle, int _signal, int _timeout)
{
  if (_handle->state != RUNNING)
    return 0;

  pid_t addressee = (_handle->termination_mode == TERMINATION_CHILD_ONLY) ?
                      (_handle->child_id) : (-_handle->child_id);
  if (kill(addressee, _signal) < 0)
    return -1;

  return process_poll(_handle, _timeout);
}


int process_terminate (process_handle_t * _handle)
{
  return termination_signal(_handle, SIGTERM, 100);
}


int process_kill(process_handle_t * _handle)
{
  // this will terminate the child for sure so we can
  // wait until it happens
  return termination_signal(_handle, SIGKILL, -1);
}


/* --- library ------------------------------------------------------ */

int timed_read (int _fd, char * _buffer, size_t _count, int _timeout)
{
  // this should never be called with "infinite" timeout
  if (_timeout < 0)
    return -1;

  fd_set set;
  struct timeval timeout;

  FD_ZERO(&set);
  FD_SET(_fd, &set);

  int start = clock_millisec(), rc;
  do {
    _timeout -= clock_millisec() - start;
    timeout.tv_sec = _timeout/1000;
    timeout.tv_usec = (_timeout % 1000) * 1000;

    rc = select(_fd + 1, &set, NULL, NULL, &timeout);
    if (rc == -1 && errno != EINTR)
      return -1;
  
  } while(rc == 0 && _timeout > 0);

  // nothing to read
  if (rc == 0)
    return 0;

  return read(_fd, _buffer, _count);
}


/* --- test --------------------------------------------------------- */



#ifdef LINUX_TEST
int main (int argc, char ** argv)
{
  process_handle_t handle;
  char * const args[] = { NULL };
  char * const env[]  = { NULL };

  char buffer[BUFFER_SIZE];

  if (spawn_process(&handle, "/bin/bash", args, env, NULL) < 0) {
    perror("error in spawn_process()");
    exit(EXIT_FAILURE);
  }

  process_write(&handle, "echo A\n", 7);
  
  /* read is non-blocking so the child needs time to produce output */
  sleep(1);
  process_read(&handle, PIPE_STDOUT, buffer, sizeof(buffer), -1);

  printf("stdout: %s\n", buffer);

  teardown_process(&handle);

  return 0;
}
#endif /* LINUX_TEST */

