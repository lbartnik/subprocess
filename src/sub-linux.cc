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


static void set_block (int _fd) {
  if (fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL) & (~O_NONBLOCK)) < 0) {
    throw subprocess_exception("could not set pipe to non-blocking mode");
  }
}

static void set_non_block (int _fd) {
  if (fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL) | O_NONBLOCK) < 0) {
    throw subprocess_exception("could not set pipe to non-blocking mode");
  }
}


/* --- process_handle ----------------------------------------------- */

process_handle_t::process_handle_t ()
  : state(NOT_STARTED)
{

}


process_handle_t::~process_handle_t ()
{
  shutdown();
}


/* ------------------------------------------------------------------ */

/**
 * A helper class that simplifies error handling when opening multiple
 * pipes.
 */
struct pipe_holder {

  enum { PIPE_READ  = 0, PIPE_WRITE = 1 };

  int fds[2];

  int & read ()  { return fds[PIPE_READ]; }
  int & write () { return fds[PIPE_WRITE]; }

  /**
   * Zero the descriptor array and immediately try opening a (unnamed)
   * pipe().
   */
  pipe_holder () : fds{0, 0} {
    if (pipe(pipe_stdin) < 0) {
      throw subprocess_exception(errno, "could not create a pipe");
    }
  }

  /**
   * Will close both descriptors unless they're set to 0 from the outside.
   */
  ~pipe_holder () {
    if (fds[PIPE_READ]) close(fds[PIPE_READ]);
    if (fds[PIPE_WRITE]) close(fds[PIPE_WRITE]);
  }
};


/*
 * Duplicate handle and zero the original 
 */
inline dup_fd (int & _from, int & _to) {
  if (dup2(_from, _to) < 0) {
    perror("duplicating descriptor failed, abortnig");
    exit_on_failure();
  }
}


/**
 * In most cases, when a negative value is returned the calling function
 * can consult the value of errno.
 *
 * @return 0 on success and negative on an error.
 */
int process_handle_t::spawn_process (const char * _command, char *const _arguments[],
	               char *const _environment[], const char * _workdir,
                 termination_mode_type _termination_mode)
{
  if (state != NOT_STARTED) {
    throw subprocess_exception(EALREADY, "process already started");
  }

  // can be addressed with PIPE_STDIN, PIPE_STDOUT, PIPE_STDERR
  pipe_holder pipes[3];

  /* spawn a child */
  if ( (child_id = fork()) < 0) {
    throw subprocess_exception(errno, "could not spawn a process");
  }

  /* child should copy his ends of pipes and close his and parent's
   * ends of pipes */
  if (_handle->child_id == 0) {
    stringstream error_message;

    // this part is kept in C-style, no exceptions after forking
    // into a child
    dup_fd(pipes[PIPE_STDIN].read(), STDIN_FILENO);
    dup_fd(pipes[PIPE_STDOUT].write(), STDOUT_FILENO);
    dup_fd(pipes[PIPE_STDERR].write(), STDERR_FILENO);

    /* change directory */
    if (_workdir != NULL) {
      if (chdir(_workdir) < 0) {
        message << "could not change working directory to " << _workdir;
        perror(error_message.str().c_str());
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
    error_message << "could not run command " << _command;
    perror(error_message.str().c_str());

    exit_on_failure();
  }

  // child is now running
  state = RUNNING;
  termination_mode = _termination_mode;

  pipe_stdin  = pipes[PIPE_STDIN].write();
  pipe_stdout = pipes[PIPE_STDOUT].read();
  pipe_stderr = pipes[PIPE_STDERR].read();

  // reset the NONBLOCK on stdout-read and stderr-read descriptors
  set_non_block(pipe_stdout);
  set_non_block(pipe_stderr);

  // the very last step: set them to zero so that the destructor
  // doesn't close them
  pipes[PIPE_STDIN].write() = 0;
  pipes[PIPE_STDOUT].read() = 0;
  pipes[PIPE_STDERR].read() = 0;
}



/* --- process_handle::shutdown ------------------------------------- */

void process_handle_t::shutdown ()
{
  if (state == SHUTDOWN) {
    return;
  }
  if (!child_id) {
    throw subprocess_exception(ECHILD, "child does not exist");
  }

  /* all we need to do is close pipes */
  auto close_pipe = [](pipe_handle_type _pipe) {
    if (_pipe) close(_pipe);
  }

  close_pipe(pipe_stdin);
  close_pipe(pipe_stdout);
  close_pipe(pipe_stderr);

  /* closing pipes should let the child process exit */
  // TODO there might be a need to send a termination signal first
  process_poll(this, TRUE);

  state = SHUTDOWN;
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
      _handle.stdout_.clear();
    }
    if (_pipe & PIPE_STDERR) {
      put_fd(_handle.pipe_stderr);
      _handle.stderr_.clear();
    }
    
    struct timeval timeout;
    int start = clock_millisec(), timediff;
    ssize_t rc;

    do {
      timediff = _timeout - (clock_millisec() - start);

      // use max so that _timeout can be TIMEOUT_IMMEDIATE and yet
      // select can be tried at least once
      timeout.tv_sec = std::max(0, timediff/1000);
      timeout.tv_usec = std::max(0, (timediff % 1000) * 1000);
      
      rc = select(max_fd + 1, &set, NULL, NULL, &timeout);
      if (rc == -1 && errno != EINTR && errno != EAGAIN)
        return -1;
      
    } while(rc == 0 && timediff > 0);
    
    // nothing to read; if errno == EINTR try reading one last time
    if (rc == 0 || errno == EAGAIN)
      return 0;
    
    if (FD_ISSET(_handle.pipe_stdout, &set)) {
      rc = std::min(rc, _handle.stdout_.read(_handle.pipe_stdout, mbcslocale));
    }
    if (FD_ISSET(_handle.pipe_stderr, &set)) {
      rc = std::min(rc, _handle.stderr_.read(_handle.pipe_stderr, mbcslocale));
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
    enable_block_mode blocker_out(_handle.pipe_stdout);
    enable_block_mode blocker_err(_handle.pipe_stderr);
    rc = reader.timed_read(_handle, _pipe, 1000);
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

