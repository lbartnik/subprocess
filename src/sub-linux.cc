//#define _GNU_SOURCE             /* See feature_test_macros(7) */

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <string>
#include <sstream>

#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <dlfcn.h>

#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>

#include "config-os.h"
#include "subprocess.h"


#ifdef SUBPROCESS_MACOS
#include <mach/clock.h>
#include <mach/mach.h>
#endif

/* for some reason environ is unaccessible via unistd.h on Solaris
 * this is redundant in non-Solaris builds but fixes Solaris */
extern char ** environ;

#ifdef TRUE
#undef TRUE
#endif

#define TRUE 1

// a way to ignore a return value even when gcc warns about it
template<typename T> inline void ignore_return_value (T _t) {}

namespace subprocess {


/*
 * Append system error message to user error message.
 */
string strerror (int _code, const string & _message)
{
  std::stringstream message;

  vector<char> buffer(BUFFER_SIZE, 0);
  if (strerror_r(_code, buffer.data(), buffer.size()-1) == 0) {
    message << _message << ": " << buffer.data();
  }
  else {
    message << _message << ": system error message could not be fetched, errno = " << _code;
  }

  return message.str();
}




// MacOS implementation according to
// http://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x/6725161#6725161
static time_t clock_millisec ()
{
  struct timespec current;

#ifdef SUBPROCESS_MACOS // OS X does not have clock_gettime, use clock_get_time
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

  long long current_time = static_cast<long long>(current.tv_sec) * 1000L +
                           static_cast<long long>(current.tv_nsec / 1000000L);
  return static_cast<time_t>(current_time);
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


/* --- wrappers for Linux system API -------------------------------- */


/*
 * Duplicate handle and zero the original
 */
inline void dup2 (int _from, int _to) {
  if (::dup2(_from, _to) < 0) {
    throw subprocess_exception(errno, "duplicating descriptor failed");
  }
}

inline void close (int & _fd) {
  if (::close(_fd) < 0) {
    throw subprocess_exception(errno, "could not close descriptor");
  }
  _fd = HANDLE_CLOSED;
}

inline void chdir (const string & _path) {
  if (::chdir(_path.c_str()) < 0) {
    throw subprocess_exception(errno, "could not change working directory to " + _path);
  }
}

inline void setsid () {
  if (::setsid() == (pid_t)-1) {
    throw subprocess_exception(errno, "could not start a new session");
  }
}

static void set_block (int _fd) {
  if (fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL) & (~O_NONBLOCK)) < 0) {
    throw subprocess_exception(errno, "could not set pipe to non-blocking mode");
  }
}

static void set_non_block (int _fd) {
  if (fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL) | O_NONBLOCK) < 0) {
    throw subprocess_exception(errno, "could not set pipe to non-blocking mode");
  }
}


/* --- process_handle ----------------------------------------------- */

process_handle_t::process_handle_t ()
  : child_handle(0),
    pipe_stdin(HANDLE_CLOSED), pipe_stdout(HANDLE_CLOSED),
    pipe_stderr(HANDLE_CLOSED), state(NOT_STARTED)
{ }


/* ------------------------------------------------------------------ */

/**
 * A helper class that simplifies error handling when opening multiple
 * pipes.
 */
struct pipe_holder {

  enum pipe_end { READ  = 0, WRITE = 1 };

  int fds[2];

  int & operator [] (pipe_end _i) { return fds[_i]; }

  /**
   * Zero the descriptor array and immediately try opening a (unnamed)
   * pipe().
   */
  pipe_holder () : fds{HANDLE_CLOSED, HANDLE_CLOSED} {
    if (pipe(fds) < 0) {
      throw subprocess_exception(errno, "could not create a pipe");
    }
  }

  /**
   * Will close both descriptors unless they're set to 0 from the outside.
   */
  ~pipe_holder () {
    if (fds[READ] != HANDLE_CLOSED) close(fds[READ]);
    if (fds[WRITE] != HANDLE_CLOSED) close(fds[WRITE]);
  }
};


/**
 * In most cases, when a negative value is returned the calling function
 * can consult the value of errno.
 *
 * @return 0 on success and negative on an error.
 */
void process_handle_t::spawn (const char * _command, char *const _arguments[],
	               char *const _environment[], const char * _workdir,
                 termination_mode_type _termination_mode)
{
  if (state != NOT_STARTED) {
    throw subprocess_exception(EALREADY, "process already started");
  }

  int rc = 0;
  // can be addressed with PIPE_STDIN, PIPE_STDOUT, PIPE_STDERR
  pipe_holder pipes[3];

  /* spawn a child */
  if ( (child_id = fork()) < 0) {
    throw subprocess_exception(errno, "could not spawn a process");
  }

  /* child should copy his ends of pipes and close his and parent's
   * ends of pipes */
  if (child_id == 0) {
    try {
      dup2(pipes[PIPE_STDIN][pipe_holder::READ], STDIN_FILENO);
      dup2(pipes[PIPE_STDOUT][pipe_holder::WRITE], STDOUT_FILENO);
      dup2(pipes[PIPE_STDERR][pipe_holder::WRITE], STDERR_FILENO);

      close(pipes[PIPE_STDIN][pipe_holder::READ]);
      close(pipes[PIPE_STDIN][pipe_holder::WRITE]);
      close(pipes[PIPE_STDOUT][pipe_holder::READ]);
      close(pipes[PIPE_STDOUT][pipe_holder::WRITE]);
      close(pipes[PIPE_STDERR][pipe_holder::READ]);
      close(pipes[PIPE_STDERR][pipe_holder::WRITE]);

      /* change directory */
      if (_workdir != NULL) {
        chdir(_workdir);
      }

      /* if termination mode is "group" start new session */
      if (_termination_mode == TERMINATION_GROUP) {
        setsid();
      }

      /* if environment is empty, use parent's environment */
      if (!_environment) {
        _environment = environ;
      }

      /* finally start the new process */
      execve(_command, _arguments, _environment);

      // TODO if we dup() STDERR_FILENO, we can print this message there
      //      rather then into the pipe
      perror((string("could not run command ") + _command).c_str());
    }
    catch (subprocess_exception & e) {
      // we do not name stderr explicitly because CRAN doesn't like it
      ignore_return_value(::write(2, e.what(), strlen(e.what())));
      exit_on_failure();
    }
  }

  // child is now running
  state = RUNNING;
  termination_mode = _termination_mode;

  pipe_stdin  = pipes[PIPE_STDIN][pipe_holder::WRITE];
  pipe_stdout = pipes[PIPE_STDOUT][pipe_holder::READ];
  pipe_stderr = pipes[PIPE_STDERR][pipe_holder::READ];

  // reset the NONBLOCK on stdout-read and stderr-read descriptors
  set_non_block(pipe_stdout);
  set_non_block(pipe_stderr);

  // the very last step: set them to zero so that the destructor
  // doesn't close them
  pipes[PIPE_STDIN][pipe_holder::WRITE] = HANDLE_CLOSED;
  pipes[PIPE_STDOUT][pipe_holder::READ] = HANDLE_CLOSED;
  pipes[PIPE_STDERR][pipe_holder::READ] = HANDLE_CLOSED;

  // update process state
  wait(TIMEOUT_IMMEDIATE);
}



/* --- process_handle::shutdown ------------------------------------- */

void process_handle_t::shutdown ()
{
  if (state != RUNNING) {
    return;
  }
  if (!child_id) {
    throw subprocess_exception(ECHILD, "child does not exist");
  }

  /* all we need to do is close pipes */
  auto close_pipe = [](pipe_handle_type _pipe) {
    if (_pipe != HANDLE_CLOSED) close(_pipe);
    _pipe = HANDLE_CLOSED;
  };

  close_pipe(pipe_stdin);
  close_pipe(pipe_stdout);
  close_pipe(pipe_stderr);

  /* closing pipes should let the child process exit */
  // TODO there might be a need to send a termination signal first
  wait(TIMEOUT_IMMEDIATE);
  kill();
  wait(TIMEOUT_INFINITE);

  state = SHUTDOWN;
}


/* --- process::write ----------------------------------------------- */


size_t process_handle_t::write (const void * _buffer, size_t _count)
{
  if (!child_id) {
    throw subprocess_exception(ECHILD, "child does not exist");
  }

  ssize_t ret = ::write(pipe_stdin, _buffer, _count);
  if (ret < 0) {
    throw subprocess_exception(errno, "could not write to child process");
  }

  return static_cast<size_t>(ret);
}


/* --- process::read ------------------------------------------------ */


struct enable_block_mode {
  int fd;

  enable_block_mode (int _fd) : fd (_fd) {
    set_block(fd);
  }

  ~enable_block_mode () {
    set_non_block(fd);
  }
};


ssize_t timed_read (process_handle_t & _handle, pipe_type _pipe, int _timeout)
{
  struct pollfd fds[2] { { .fd = -1 }, { .fd = -1 } };

  if (_pipe & PIPE_STDOUT) {
    fds[0].fd = _handle.pipe_stdout;
    fds[0].events = POLLIN;
    _handle.stdout_.clear();
  }

  if (_pipe & PIPE_STDERR) {
    fds[1].fd = _handle.pipe_stderr;
    fds[1].events = POLLIN;
    _handle.stderr_.clear();
  }

  time_t start = clock_millisec(), timediff = _timeout;
  ssize_t rc;

  do {
    rc = poll(fds, 2, timediff);
    timediff = _timeout - (clock_millisec() - start);

    // interrupted or kernel failed to allocate internal resources
    if (rc < 0 && (errno == EINTR || errno == EAGAIN)) {
      rc = 0;
    }
  } while (rc == 0 && timediff > 0);

  // nothing to read
  if (rc == 0) {
    return 0;
  }

  // TODO if an error occurs in the first read() it will be lost
  if (fds[0].fd != -1 && fds[0].revents == POLLIN) {
    rc = std::min(rc, (ssize_t)_handle.stdout_.read(_handle.pipe_stdout, mbcslocale));
  }
  if (fds[1].fd != -1 && fds[1].revents == POLLIN) {
    rc = std::min(rc, (ssize_t)_handle.stderr_.read(_handle.pipe_stderr, mbcslocale));
  }

  return rc;
}


size_t process_handle_t::read (pipe_type _pipe, int _timeout)
{
  if (!child_id) {
    throw subprocess_exception(ECHILD, "child does not exist");
  }

  ssize_t rc = timed_read(*this, _pipe, _timeout);

  if (rc < 0) {
    throw subprocess_exception(errno, "could not read from child process");
  }

  return static_cast<size_t>(rc);
}


/* --- process::close_input ----------------------------------------- */

void process_handle_t::close_input ()
{
  if (pipe_stdin == HANDLE_CLOSED) {
    throw subprocess_exception(EALREADY, "child's standard input already closed");
  }

  close(pipe_stdin);
  pipe_stdin = HANDLE_CLOSED;
}


/* --- process::wait ------------------------------------------------ */


void process_handle_t::wait (int _timeout)
{
  if (!child_id) {
    throw subprocess_exception(ECHILD, "child does not exist");
  }
  if (state != RUNNING) {
    return;
  }

  /* to wait or not to wait? */
  int options = 0;
  if (_timeout >= 0) {
    options = WNOHANG;
  }

  /* make the actual system call */
  int start = clock_millisec(), rc;
  do {
    rc = waitpid(child_id, &return_code, options);

    // there's been an error (<0)
    if (rc < 0) {
      throw subprocess_exception(errno, "waitpid() failed");
    }

    _timeout -= clock_millisec() - start;
  } while (rc == 0 && _timeout > 0);

  // the child is still running
  if (rc == 0) {
     return;
  }

  // the child has exited or has been terminated
  if (WIFEXITED(return_code)) {
    state = process_handle_t::EXITED;
    return_code = WEXITSTATUS(return_code);
  }
  else if (WIFSIGNALED(return_code)) {
    state = process_handle_t::TERMINATED;
    return_code = WTERMSIG(return_code);
  }
  else {
    throw subprocess_exception(0, "process did not exit nor was terminated");
  }
}


/* --- process::signal ---------------------------------------------- */


void process_handle_t::send_signal(int _signal)
{
  if (!child_id) {
    throw subprocess_exception(ECHILD, "child does not exist");
  }
  int rc = ::kill(child_id, _signal);

  if (rc < 0) {
    throw subprocess_exception(errno, "could not post signal to child process");
  }
}


/* --- process::terminate & process::kill --------------------------- */


static void termination_signal (process_handle_t & _handle, int _signal, int _timeout)
{
  if (_handle.state != process_handle_t::RUNNING)
    return;

  pid_t addressee = (_handle.termination_mode == process_handle_t::TERMINATION_CHILD_ONLY) ?
                      (_handle.child_id) : (-_handle.child_id);
  if (::kill(addressee, _signal) < 0) {
    throw subprocess_exception(errno, "system kill() failed");
  }

  _handle.wait(_timeout);
}


void process_handle_t::terminate ()
{
  termination_signal(*this, SIGTERM, 100);
}


void process_handle_t::kill()
{
  // this will terminate the child for sure so we can
  // wait until it happens
  termination_signal(*this, SIGKILL, TIMEOUT_INFINITE);
}

/* --- process_exists ----------------------------------------------- */

bool process_exists (const pid_type & _pid)
{
  return ::kill(_pid, 0) == 0;
}

} /* namespace subprocess */


/* --- test --------------------------------------------------------- */



#ifdef LINUX_TEST

using namespace subprocess;

int main (int argc, char ** argv)
{
  process_handle_t handle;
  char * const args[] = { NULL };
  char * const env[]  = { NULL };

  handle.spawn("/bin/bash", args, env, NULL, process_handle_t::TERMINATION_GROUP);

  process_write(&handle, "echo A\n", 7);

  /* read is non-blocking so the child needs time to produce output */
  sleep(1);
  process_read(handle, PIPE_STDOUT, TIMEOUT_INFINITE);

  printf("stdout: %s\n", handle.stdout_.data());

  handle.shutdown();

  return 0;
}
#endif /* LINUX_TEST */

