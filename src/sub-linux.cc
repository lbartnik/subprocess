//#define _GNU_SOURCE             /* See feature_test_macros(7) */

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>

#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>

#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>

// for some reason is accessible via unistd.h
extern char ** environ;
#endif


#include "subprocess.h"

// this cannot be a "const int" - it's only C++
#define BUFFER_SIZE 1024

#ifdef TRUE
#undef TRUE
#endif

#define TRUE 1


static const int PIPE_READ  = 0;
static const int PIPE_WRITE = 1;



int full_error_message (char * _buffer, size_t _length)
{
  if (strerror_r(errno, _buffer, _length) == 0)
    return 0;
  return -1;
}



// MacOS implementation according to
// http://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x/6725161#6725161
static time_t clock_millisec ()
{
  struct timespec current;

#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
  clock_serv_t cclock;
  mach_timespec_t mts;
  host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
  clock_get_time(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);
  current.tv_sec = mts.tv_sec;
  current.tv_nsec = mts.tv_nsec;
#else // Linux
  clock_gettime(CLOCK_REALTIME, &current);
#endif

  return (int)current.tv_sec * 1000 + (int)(current.tv_nsec / 1000000);
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
    ssize_t ret = write(STDERR_FILENO, message, strlen(message));
    *(int*)exit_handle = 0;
    ++ret; // hide compiler warning
  }
  
  typedef void (* exit_t)(int);
  exit_t exit_fun = (exit_t)exit_handle;
  exit_fun(EXIT_FAILURE);
}


static int set_block (int _fd) {
  return fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL) & (~O_NONBLOCK));
}

static int set_non_block (int _fd) {
  return fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL) | O_NONBLOCK);
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
  int pipe_stdin[2] = { 0, 0 }, pipe_stdout[2] = { 0, 0 }, pipe_stderr[2] = { 0, 0 };

  // we initialize them to 0 to everythin non-zero should be closed;
  // preserve the errno value, too
  #define CLOSE_ALL_PIPES                                          \
    do {                                                           \
      err = errno;                                                 \
      if (pipe_stdin[PIPE_READ])   close(pipe_stdin[PIPE_READ]);   \
      if (pipe_stdin[PIPE_WRITE])  close(pipe_stdin[PIPE_WRITE]);  \
      if (pipe_stdout[PIPE_READ])  close(pipe_stdout[PIPE_READ]);  \
      if (pipe_stdout[PIPE_WRITE]) close(pipe_stdout[PIPE_WRITE]); \
      if (pipe_stderr[PIPE_READ])  close(pipe_stderr[PIPE_READ]);  \
      if (pipe_stderr[PIPE_WRITE]) close(pipe_stderr[PIPE_WRITE]); \
      errno = err;                                                 \
   } while (0);                                                    \

  _handle->state = NOT_STARTED;

  /* redirect standard streams */
  if (pipe(pipe_stdin) < 0) {
    return -1;
  }

  /* if there is an error preserve errno and close anything opened so far */
  if ((pipe(pipe_stdout) < 0)) {
    CLOSE_ALL_PIPES
    return -1;
  }

  if ((pipe(pipe_stderr) < 0)) {
    CLOSE_ALL_PIPES
    return -1;
  }

  if (set_non_block(pipe_stdout[PIPE_READ]) < 0 ||
      set_non_block(pipe_stderr[PIPE_READ]) < 0)
  {
    CLOSE_ALL_PIPES
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
    CLOSE_ALL_PIPES

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
    
    /* if environment is empty, use parent's environment */
    if (!_environment) {
      _environment = environ;
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
  if (_handle->state == TORNDOWN) {
    return 0;
  }
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


/* --- process_read ------------------------------------------------- */


struct enable_block_mode {
  int fd;

  enable_block_mode (int _fd) : fd (_fd) {
    set_block(fd);
  }
  
  ~enable_block_mode () {
    set_non_block(fd);
  }
};


struct select_reader {
  
  fd_set set;
  int max_fd;
  
  select_reader () : max_fd(0) { }
  
  void put_fd (int _fd) {
    FD_SET(_fd, &set);
    max_fd = std::max(max_fd, _fd);
  }
  
  ssize_t timed_read (process_handle_t & _handle, pipe_t _pipe, int _timeout)
  {
    // this should never be called with "infinite" timeout
    if (_timeout < 0)
      return -1;
    
    FD_ZERO(&set);
    if (_pipe & PIPE_STDOUT) {
      put_fd(_handle.pipe_stdout);
    }
    if (_pipe & PIPE_STDERR) {
      put_fd(_handle.pipe_stderr);
    }
    
    struct timeval timeout;
    int start = clock_millisec();
    ssize_t rc;

    do {
      int timediff = _timeout - (clock_millisec() - start);
      if (timediff < 0) break;

      timeout.tv_sec = timediff/1000;
      timeout.tv_usec = (timediff % 1000) * 1000;
      
      rc = select(max_fd + 1, &set, NULL, NULL, &timeout);
      if (rc == -1 && errno != EINTR && errno != EAGAIN)
        return -1;
      
    } while(rc == 0);
    
    // nothing to read; if errno == EINTR try reading one last time
    if (rc == 0 || errno == EAGAIN)
      return 0;
    
    if (FD_ISSET(_handle.pipe_stdout, &set)) {
      rc = std::min(rc, _handle.stdout.read(_handle.pipe_stdout, mbcslocale));
    }
    if (FD_ISSET(_handle.pipe_stderr, &set)) {
      rc = std::min(rc, _handle.stderr.read(_handle.pipe_stderr, mbcslocale));
    }
    
    return rc;
  }
};




ssize_t process_read (process_handle_t & _handle, pipe_t _pipe, int _timeout)
{
  if (!_handle.child_id) {
    errno = ECHILD;
    return -1;
  }

  ssize_t rc;
  select_reader reader;

  // infinite timeout
  if (_timeout < 0) {
    enable_block_mode(_handle.pipe_stdout);
    enable_block_mode(_handle.pipe_stderr);
    rc = reader.timed_read(_handle, _pipe, 0);
  }
  // finite or no timeout
  else {
    rc = reader.timed_read(_handle, _pipe, _timeout);

    if (rc < 0 && errno == EAGAIN) {
      /* stdin pipe is opened with O_NONBLOCK, so this means "would block" */
      errno = 0;
      return 0;
    }
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

