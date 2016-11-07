#include "subprocess.h"

#include <stdio.h>
#include <string.h>

#include <R.h>
#include <Rdefines.h>


static const size_t BUFFER_SIZE = 1024;

/* --- library ------------------------------------------------------ */

static void C_child_process_finalizer(SEXP ptr);


static void Rf_perror (const char * _message)
{
  char message[BUFFER_SIZE];
  int offset = snprintf(message, sizeof(message), "%s: ", _message);
  
  full_error_message(message+offset, sizeof(message)-offset);
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

SEXP C_process_spawn (SEXP _command, SEXP _arguments, SEXP _environment)
{
  /* basic argument sanity checks */
  if (!isString(_command) || (LENGTH(_command) != 1) || !strlen(translateChar(STRING_ELT(_command, 0)))) {
	  Rf_error("`command` must be a non-empty string");
  }
  if (!isString(_arguments)) {
    Rf_error("invalid value for `arguments`");
  }
  if (!isString(_environment)) {
    Rf_error("invalid value for `environment`");
  }

  /* translate into C */
  const char * command = translateChar(STRING_ELT(_command, 0));

  char ** arguments   = to_C_array(_arguments);
  char ** environment = to_C_array(_environment);

  /* Calloc() handles memory allocation errors internally */
  process_handle_t * handle = (process_handle_t*)Calloc(1, process_handle_t);

  /* spawn the process */
  if (spawn_process(handle, command, arguments, environment) < 0) {
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
  if(!R_ExternalPtrAddr(ptr)) return;
  teardown_process(R_ExternalPtrAddr(ptr));
  R_ClearExternalPtr(ptr); /* not really needed */
}

SEXP C_process_read (SEXP _handle, SEXP _pipe)
{
  process_handle_t * process_handle = extract_process_handle(_handle);

  if (!isString(_pipe) || (LENGTH(_pipe) != 1)) {
    Rf_error("`pipe` must be a single character value");
  }

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
  process_read(process_handle, which_pipe, buffer, BUFFER_SIZE);

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

  if (!isString(_message) || (LENGTH(_message) != 1)) {
    Rf_error("`message` must be a single character value");
  }

  const char * message = translateChar(STRING_ELT(_message, 0));
  process_write(process_handle, message, strlen(message));

  return R_NilValue;
}


SEXP C_process_poll (SEXP _handle)
{
  process_handle_t * process_handle = extract_process_handle(_handle);
  if (process_poll(process_handle, 0) < 0) {
    Rf_perror("process poll failed");
  }

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


SEXP C_process_end (SEXP _handle)
{
  process_handle_t * process_handle = extract_process_handle(_handle);

  if (teardown_process(process_handle) < 0) {
    Rf_perror("error while shutting down child process");
  }

  SEXP ans;
  PROTECT(ans = allocVector(LGLSXP, 1));
  LOGICAL_DATA(ans)[0] = TRUE;

  /* ans */
  UNPROTECT(1);
  return ans;
}
