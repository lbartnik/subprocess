#include "subprocess.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <R.h>
#include <Rdefines.h>

// from "is_something.c"
int is_nonempty_string(SEXP _obj);
int is_single_string_or_NULL(SEXP _obj);
int is_single_integer(SEXP _obj);


#define BUFFER_SIZE 1024

/* --- library ------------------------------------------------------ */

static void C_child_process_finalizer(SEXP ptr);


static void Rf_perror (const char * _message)
{
  char message[BUFFER_SIZE];
  int offset = snprintf(message, sizeof(message), "%s: ", _message);
  
  if (full_error_message(message+offset, sizeof(message)-offset) < 0) {
    snprintf(message+offset, sizeof(message)-offset,
             "system error message could not be fetched");
  }

  Rf_error(message);
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

static SEXP allocate_single_int (int _value)
{
  SEXP ans;
  PROTECT(ans = allocVector(INTSXP, 1));
  INTEGER_DATA(ans)[0] = _value;
  UNPROTECT(1);
  return ans;
}

static SEXP allocate_TRUE ()
{
  SEXP ans;
  PROTECT(ans = allocVector(LGLSXP, 1));
  LOGICAL_DATA(ans)[0] = 1;
  UNPROTECT(1);
  return ans;
}


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


/* --- public API --------------------------------------------------- */

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
  termination_mode_t termination_mode = TERMINATION_GROUP;
  if (!strncmp(termination_mode_str, "child_only", 10)) {
    termination_mode = TERMINATION_CHILD_ONLY;
  }
  else if (strncmp(termination_mode_str, "group", 5)) {
    Rf_error("unknown value for `termination_mode`");
  }

  /* Calloc() handles memory allocation errors internally */
  process_handle_t * handle = (process_handle_t*)Calloc(1, process_handle_t);

  /* spawn the process */
  if (spawn_process(handle, command, arguments, environment, workdir, termination_mode) < 0) {
    Rf_perror("error while spawning a child process");
  }

  /* return an external pointer handle */
  SEXP ptr;
  PROTECT(ptr = R_MakeExternalPtr(handle, install("process_handle"), R_NilValue));
  R_RegisterCFinalizerEx(ptr, C_child_process_finalizer, TRUE);

  /* return the child process PID */
  SEXP ans;
  ans = PROTECT(allocVector(INTSXP, 1));
  INTEGER(ans)[0] = handle->child_id;
  setAttrib(ans, install("handle_ptr"), ptr);

  /* ptr, ans */
  UNPROTECT(2);
  return ans;
}


static void C_child_process_finalizer(SEXP ptr)
{
  if (!R_ExternalPtrAddr(ptr)) return;
  if (process_terminate(R_ExternalPtrAddr(ptr)) < 0) {
    Rf_perror("error while finalizing child process");
  }
  R_ClearExternalPtr(ptr); /* not really needed */
}


// TODO add wait/timeout
SEXP C_process_read (SEXP _handle, SEXP _pipe, SEXP _timeout)
{
  process_handle_t * process_handle = extract_process_handle(_handle);

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
  pipe_t which_pipe;
  if (!strncmp(pipe, "stdout", 6))
    which_pipe = PIPE_STDOUT;
  else if (!strncmp(pipe, "stderr", 6))
    which_pipe = PIPE_STDERR;
  else {
    Rf_error("unrecognized `pipe` value");
  }

  /* read into this buffer */
  char * buffer = (char*)Calloc(BUFFER_SIZE, char);
  process_read(process_handle, which_pipe, buffer, BUFFER_SIZE, timeout);

  SEXP ans;
  PROTECT(ans = allocVector(STRSXP, 1));
  SET_STRING_ELT(ans, 0, mkChar(buffer));

  /* ans */
  UNPROTECT(1);
  return ans;
}

SEXP C_process_write (SEXP _handle, SEXP _message)
{
  process_handle_t * process_handle = extract_process_handle(_handle);

  if (!is_nonempty_string(_message)) {
    Rf_error("`message` must be a single character value");
  }

  const char * message = translateChar(STRING_ELT(_message, 0));
  int ret = process_write(process_handle, message, strlen(message));

  return allocate_single_int(ret);
}


SEXP C_process_poll (SEXP _handle, SEXP _timeout)
{
  /* extract timeout */
  if (!is_single_integer(_timeout)) {
    Rf_error("`timeout` must be a single integer value");
  }

  int timeout = INTEGER_DATA(_timeout)[0];

  /* extract handle */
  process_handle_t * process_handle = extract_process_handle(_handle);

  /* check the process */
  if (process_poll(process_handle, timeout) < 0) {
    Rf_perror("process poll failed");
  }

  /* answer */
  SEXP ans;
  PROTECT(ans = allocVector(STRSXP, 1));

  if (process_handle->state == EXITED) {
    SET_STRING_ELT(ans, 0, mkChar("exited"));
  }
  else if (process_handle->state == TERMINATED) {
    SET_STRING_ELT(ans, 0, mkChar("terminated"));
  }
  else if (process_handle->state == RUNNING) {
    SET_STRING_ELT(ans, 0, mkChar("running"));
  }
  else {
    SET_STRING_ELT(ans, 0, mkChar("not-started"));
  }

  /* ans */
  UNPROTECT(1);
  return ans;
}


SEXP C_process_return_code (SEXP _handle)
{
  process_handle_t * process_handle = extract_process_handle(_handle);
  if (process_poll(process_handle, 0) < 0) {
    Rf_perror("process poll failed");
  }

  if (process_handle->state == EXITED || process_handle->state == TERMINATED)
    return allocate_single_int(process_handle->return_code);
  else
    return allocate_single_int(NA_INTEGER);
}


SEXP C_process_terminate (SEXP _handle)
{
  process_handle_t * process_handle = extract_process_handle(_handle);

  if (process_terminate(process_handle) < 0) {
    Rf_perror("error while terminating child process");
  }

  return allocate_TRUE();
}


SEXP C_process_kill (SEXP _handle)
{
  process_handle_t * process_handle = extract_process_handle(_handle);

  if (process_kill(process_handle) < 0) {
    Rf_perror("error while killing child process");
  }

  return allocate_TRUE();
}


SEXP C_process_send_signal (SEXP _handle, SEXP _signal)
{
  process_handle_t * process_handle = extract_process_handle(_handle);
  if (!is_single_integer(_signal)) {
    Rf_error("`signal` must be a single integer value");
  }

  int signal = INTEGER_DATA(_signal)[0];

  if (process_send_signal(process_handle, signal) < 0) {
    Rf_perror("error while sending a signal to child process");
  }

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
    Rf_perror("error while calling signal()");
  }

  return allocate_TRUE();
}

