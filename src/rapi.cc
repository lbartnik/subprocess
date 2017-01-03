/** @file rapi.cc
 *
 * Implementation of functions exported to R from the shared library.
 *
 * @author Lukasz A. Bartnik <l.bartnik@gmail.com>
 */

#include "rapi.h"
#include "subprocess.h"

#include <cstdio>
#include <cstring>
#include <functional>

#include <signal.h>


/* Windows defined TRUE but it's an enum in R */
#ifdef TRUE
#undef TRUE
#endif

#include <R.h>
#include <Rdefines.h>


using namespace subprocess;

/* --- library ------------------------------------------------------ */

/* defined at the end of this file */
static int is_nonempty_string(SEXP _obj);
static int is_single_string_or_NULL(SEXP _obj);
static int is_single_integer(SEXP _obj);

static void C_child_process_finalizer(SEXP ptr);

static char ** to_C_array (SEXP _array);

static void free_C_array (char ** _array);

static SEXP allocate_single_bool (bool _value);

static SEXP allocate_TRUE () { return allocate_single_bool(true); }
//static SEXP allocate_FALSE () { return allocate_single_bool(false); }


/* --- error handling ----------------------------------------------- */

/*
 * This is how we make sure there are no non-trivial destructors
 * (including the exception object itself )that need to be called
 * before longjmp() that Rf_error() calls.
 */
template<typename F, typename ... Args>
inline typename std::result_of<F(Args...)>::type
try_run (F _f, Args ... _args)
{
  char try_buffer[BUFFER_SIZE] = { 0 };
  bool exception_caught = false;
  auto bound = std::bind(_f, _args...);

  try {
    return bound();
  }
  catch (subprocess_exception & e) {
     exception_caught = true;
     e.store(try_buffer, sizeof(try_buffer) - 1);
  }

  // if exception has been caught here we can do a long jump
  if (exception_caught) {
    Rf_error("%s", try_buffer);
  }

  // it will never reach this line but the compiler doesn't know this
  return bound();
}


/* --- public R API ------------------------------------------------- */

static process_handle_t * extract_process_handle (SEXP _handle)
{
  SEXP ptr = getAttrib(_handle, install("handle_ptr"));
  if (ptr == R_NilValue) {
    Rf_error("`handle_ptr` attribute not found");
  }

  void * c_ptr = R_ExternalPtrAddr(ptr);
  if (!c_ptr) {
    Rf_error("external C pointer is NULL");
  }

  return (process_handle_t*)c_ptr;
}


SEXP C_process_spawn (SEXP _command, SEXP _arguments, SEXP _environment, SEXP _workdir, SEXP _termination_mode)
{
  /* basic argument sanity checks */
  if (!is_nonempty_string(_command)) {
	  Rf_error("`command` must be a non-empty string");
  }
  if (!isString(_arguments)) {
    Rf_error("invalid value for `arguments`");
  }
  if (!isString(_environment)) {
    Rf_error("invalid value for `environment`");
  }
  if (!is_single_string_or_NULL(_workdir)) {
    Rf_error("`workdir` must be a non-empty string");
  }
  if (!is_nonempty_string(_termination_mode)) {
    Rf_error("`termination_mode` must be a non-emptry string");
  }

  /* translate into C */
  const char * command = translateChar(STRING_ELT(_command, 0));

  char ** arguments   = to_C_array(_arguments);
  char ** environment = to_C_array(_environment);
  
  /* if environment if empty, simply ignore it */
  if (!environment || !*environment) {
    // allocated with Calloc() but Free() is still needed
    Free(environment);
    environment = NULL;
  }

  /* if workdir is NULL or an empty string, inherit from parent */
  const char * workdir = NULL;
  if (_workdir != R_NilValue) {
	  workdir = translateChar(STRING_ELT(_workdir, 0));
    if (strlen(workdir) == 0) {
      workdir = NULL;
    }
  }

  /* see if termination mode is set properly */
  const char * termination_mode_str = translateChar(STRING_ELT(_termination_mode, 0));
  process_handle_t::termination_mode_type termination_mode = process_handle_t::TERMINATION_GROUP;
  if (!strncmp(termination_mode_str, "child_only", 10)) {
    termination_mode = process_handle_t::TERMINATION_CHILD_ONLY;
  }
  else if (strncmp(termination_mode_str, "group", 5)) {
    Rf_error("unknown value for `termination_mode`");
  }

  /* Calloc() handles memory allocation errors internally */
  process_handle_t * handle = (process_handle_t*)Calloc(1, process_handle_t);
  handle = new (handle) process_handle_t();

  /* spawn the process */
  try_run(&process_handle_t::spawn, handle, command, arguments, environment, workdir, termination_mode);

  /* return an external pointer handle */
  SEXP ptr;
  PROTECT(ptr = R_MakeExternalPtr(handle, install("process_handle"), R_NilValue));
  R_RegisterCFinalizerEx(ptr, C_child_process_finalizer, TRUE);

  /* return the child process PID */
  SEXP ans;
  ans = PROTECT(allocVector(INTSXP, 1));
  INTEGER(ans)[0] = handle->child_id;
  setAttrib(ans, install("handle_ptr"), ptr);

  /* free temporary memory */
  free_C_array(arguments);
  free_C_array(environment);

  /* ptr, ans */
  UNPROTECT(2);
  return ans;
}


static void C_child_process_finalizer(SEXP ptr)
{
  process_handle_t * handle = (process_handle_t*)R_ExternalPtrAddr(ptr);
  if (!handle) return;

  // it might be necessary to terminate the process first
  auto try_terminate = [&handle] {
    try {
      // refresh the handle and try terminating if the child
      // is still running
      handle->wait(TIMEOUT_IMMEDIATE);
      handle->terminate();
    }
    catch (subprocess_exception) {
      handle->~process_handle_t();
      Free(handle);
      throw;
    }
  };

  // however termination goes, close pipe handles and free memory
  try_run(try_terminate);
  handle->~process_handle_t();
  Free(handle);

  R_ClearExternalPtr(ptr); /* not really needed */
}



// TODO add wait/timeout
SEXP C_process_read (SEXP _handle, SEXP _pipe, SEXP _timeout)
{
  process_handle_t * handle = extract_process_handle(_handle);

  if (!is_nonempty_string(_pipe)) {
    Rf_error("`pipe` must be a single character value");
  }
  if (!is_single_integer(_timeout)) {
    Rf_error("`timeout` must be a single integer value");
  }

  /* extract timeout */
  int timeout = INTEGER_DATA(_timeout)[0];

  /* determine which pipe */
  const char * pipe = translateChar(STRING_ELT(_pipe, 0));
  pipe_type which_pipe;
  
  if (!strncmp(pipe, "stdout", 6))
    which_pipe = PIPE_STDOUT;
  else if (!strncmp(pipe, "stderr", 6))
    which_pipe = PIPE_STDERR;
  else if (!strncmp(pipe, "both", 4))
    which_pipe = PIPE_BOTH;
  else {
    Rf_error("unrecognized `pipe` value");
  }
  
  try_run(&process_handle_t::read, handle, which_pipe, timeout); 

  /* produce the result - a list of one or two elements */
  SEXP ans, nms;
  PROTECT(ans = allocVector(VECSXP, 2));
  PROTECT(nms = allocVector(STRSXP, 2));

  SET_VECTOR_ELT(ans, 0, ScalarString(mkChar(handle->stdout_.data())));
  SET_STRING_ELT(nms, 0, mkChar("stdout"));

  SET_VECTOR_ELT(ans, 1, ScalarString(mkChar(handle->stderr_.data())));
  SET_STRING_ELT(nms, 1, mkChar("stderr"));

  /* set names */
  setAttrib(ans, R_NamesSymbol, nms);

  /* ans, nms */
  UNPROTECT(2);
  return ans;
}


SEXP C_process_close_input (SEXP _handle)
{
  process_handle_t * handle = extract_process_handle(_handle);
  try_run(&process_handle_t::close_input, handle);
  return allocate_TRUE();  
}


SEXP C_process_write (SEXP _handle, SEXP _message)
{
  process_handle_t * handle = extract_process_handle(_handle);

  if (!is_nonempty_string(_message)) {
    Rf_error("`message` must be a single character value");
  }

  const char * message = translateChar(STRING_ELT(_message, 0));
  size_t ret = try_run(&process_handle_t::write, handle, message, strlen(message)); 

  return allocate_single_int((int)ret);
}


SEXP C_process_wait (SEXP _handle, SEXP _timeout)
{
  /* extract timeout */
  if (!is_single_integer(_timeout)) {
    Rf_error("`timeout` must be a single integer value");
  }

  int timeout = INTEGER_DATA(_timeout)[0];

  /* extract handle */
  process_handle_t * handle = extract_process_handle(_handle);

  /* check the process */
  try_run(&process_handle_t::wait, handle, timeout);

  return C_process_return_code(_handle);
}


SEXP C_process_return_code (SEXP _handle)
{
  /* extract handle */
  process_handle_t * handle = extract_process_handle(_handle);

  if (handle->state == process_handle_t::EXITED ||
      handle->state == process_handle_t::TERMINATED)
    return allocate_single_int(handle->return_code);
  else
    return allocate_single_int(NA_INTEGER);
}


SEXP C_process_state (SEXP _handle)
{
  process_handle_t * handle = extract_process_handle(_handle);

  /* refresh the handle */
  try_run(&process_handle_t::wait, handle, TIMEOUT_IMMEDIATE);

  /* answer */
  SEXP ans;
  PROTECT(ans = allocVector(STRSXP, 1));

  if (handle->state == process_handle_t::EXITED) {
    SET_STRING_ELT(ans, 0, mkChar("exited"));
  }
  else if (handle->state == process_handle_t::TERMINATED) {
    SET_STRING_ELT(ans, 0, mkChar("terminated"));
  }
  else if (handle->state == process_handle_t::RUNNING) {
    SET_STRING_ELT(ans, 0, mkChar("running"));
  }
  else {
    SET_STRING_ELT(ans, 0, mkChar("not-started"));
  }

  /* ans */
  UNPROTECT(1);
  return ans;
}


SEXP C_process_terminate (SEXP _handle)
{
  process_handle_t * handle = extract_process_handle(_handle);
  try_run(&process_handle_t::terminate, handle);
  return allocate_TRUE();
}


SEXP C_process_kill (SEXP _handle)
{
  process_handle_t * handle = extract_process_handle(_handle);
  try_run(&process_handle_t::kill, handle);
  return allocate_TRUE();
}


SEXP C_process_send_signal (SEXP _handle, SEXP _signal)
{
  process_handle_t * handle = extract_process_handle(_handle);
  if (!is_single_integer(_signal)) {
    Rf_error("`signal` must be a single integer value");
  }

  int signal = INTEGER_DATA(_signal)[0];
  try_run(&process_handle_t::send_signal, handle, signal);

  return allocate_TRUE();
}


SEXP C_known_signals ()
{
  SEXP ans;
  SEXP ansnames;

  #define ADD_SIGNAL(i, name) do {              \
    INTEGER_DATA(ans)[i] = name;                \
    SET_STRING_ELT(ansnames, i, mkChar(#name)); \
  } while (0);                                  \


#ifdef SUBPROCESS_WINDOWS
  PROTECT(ans = allocVector(INTSXP, 3));
  PROTECT(ansnames = allocVector(STRSXP, 3));

  ADD_SIGNAL(0, SIGTERM);
  ADD_SIGNAL(1, CTRL_C_EVENT);
  ADD_SIGNAL(2, CTRL_BREAK_EVENT);

#else /* Linux */
  PROTECT(ans = allocVector(INTSXP, 19));
  PROTECT(ansnames = allocVector(STRSXP, 19));

  ADD_SIGNAL(0, SIGHUP)
  ADD_SIGNAL(1, SIGINT)
  ADD_SIGNAL(2, SIGQUIT)
  ADD_SIGNAL(3, SIGILL)
  ADD_SIGNAL(4, SIGABRT)
  ADD_SIGNAL(5, SIGFPE)
  ADD_SIGNAL(6, SIGKILL)
  ADD_SIGNAL(7, SIGSEGV)
  ADD_SIGNAL(8, SIGPIPE)
  ADD_SIGNAL(9, SIGALRM)
  ADD_SIGNAL(10, SIGTERM)
  ADD_SIGNAL(11, SIGUSR1)
  ADD_SIGNAL(12, SIGUSR2)
  ADD_SIGNAL(13, SIGCHLD)
  ADD_SIGNAL(14, SIGCONT)
  ADD_SIGNAL(15, SIGSTOP)
  ADD_SIGNAL(16, SIGTSTP)
  ADD_SIGNAL(17, SIGTTIN)
  ADD_SIGNAL(18, SIGTTOU)
#endif

  setAttrib(ans, R_NamesSymbol, ansnames);
  
  /* ans, ansnames */
  UNPROTECT(2);
  return ans;
}


/* --- hidden calls ------------------------------------------------- */


/* this is an access interface to system call signal(); it is used
 * in the introduction vignette */
SEXP C_signal (SEXP _signal, SEXP _handler)
{
  if (!is_single_integer(_signal)) {
    error("`signal` needs to be an integer");
  }
  if (!is_nonempty_string(_handler)) {
    error("`handler` needs to be a single character value");
  }
  
  const char * handler = translateChar(STRING_ELT(_handler, 0));
  if (!strncmp(handler, "ignore", 6) && !strncmp(handler, "default", 7)) {
    error("`handler` can be either \"ignore\" or \"default\"");
  }
  
  int sgn = INTEGER_DATA(_signal)[0];
  typedef void (*sighandler_t)(int);
  sighandler_t hnd = (strncmp(handler, "ignore", 6) ? SIG_DFL : SIG_IGN);
  
  if (signal(sgn, hnd) == SIG_ERR) {
    Rf_error("error while calling signal()");
  }

  return allocate_TRUE();
}


/* --- library functions -------------------------------------------- */

static int is_single_string (SEXP _obj)
{
  return isString(_obj) && (LENGTH(_obj) == 1);
}

static int is_nonempty_string (SEXP _obj)
{
  return is_single_string(_obj) && (strlen(translateChar(STRING_ELT(_obj, 0))) > 0);
}

static int is_single_string_or_NULL (SEXP _obj)
{
  return is_single_string(_obj) || (_obj == R_NilValue);
}

static int is_single_integer (SEXP _obj)
{
  return isInteger(_obj) && (LENGTH(_obj) == 1);
}


static char ** to_C_array (SEXP _array)
{
  char ** ret = (char**)Calloc(LENGTH(_array) + 1, char **);
  for (int i=0; i<LENGTH(_array); ++i) {
    const char * element = translateChar(STRING_ELT(_array, i));
    char * new_element = (char*)Calloc(strlen(element) + 1, char);
    memcpy(new_element, element, strlen(element)+1);
    ret[i] = new_element;
  }

  /* that's how execve() will know where does the array end */
  ret[LENGTH(_array)] = NULL;

  return ret;
}

static void free_C_array (char ** _array)
{
  if (!_array) return;
  for (char ** ptr = _array; *ptr; ++ptr) {
    Free(*ptr);
  }
  Free(_array);
}

SEXP allocate_single_int (int _value)
{
  SEXP ans;
  PROTECT(ans = allocVector(INTSXP, 1));
  INTEGER_DATA(ans)[0] = _value;
  UNPROTECT(1);
  return ans;
}

static SEXP allocate_single_bool (bool _value)
{
  SEXP ans;
  PROTECT(ans = allocVector(LGLSXP, 1));
  LOGICAL_DATA(ans)[0] = _value;
  UNPROTECT(1);
  return ans;
}
