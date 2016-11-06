#include "subprocess.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <R.h>
#include <Rdefines.h>


static const size_t BUFFER_SIZE = 1024;


static void C_child_process_finalizer(SEXP ptr);


void Rf_perror (const char * _message)
{
  char message[BUFFER_SIZE];
  int offset = snprintf(message, sizeof(message), "%s: ", _message);
  strerror_r(errno, message+offset, sizeof(message)-offset);
  Rf_error(message);
}

char ** to_C_array (SEXP _array)
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



SEXP C_spawn_process (SEXP _command, SEXP _arguments, SEXP _environment)
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
    Rf_perror("erro while spawning a child process");
  }

  /* return an external pointer handle */
  SEXP ptr;
  PROTECT(ptr = R_MakeExternalPtr(handle, install("process_handle"), R_NilValue));
  R_RegisterCFinalizerEx(ptr, C_child_process_finalizer, TRUE);

  /* return the child process PID */
  SEXP ans;
  ans = PROTECT(allocVector(INTSXP, 1));
  INTEGER(ans)[0] = handle->child_pid;
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
  SEXP ptr = getAttrib(_handle, install("handle_ptr"));
  if (ptr == R_NilValue) {
    Rf_error("`handle_ptr` attribute not found");
  }
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

  process_handle_t * process_handle = (process_handle_t*)R_ExternalPtrAddr(ptr);
  process_read(process_handle, which_pipe, buffer, BUFFER_SIZE);

  SEXP ans;
  PROTECT(ans = allocVector(STRSXP, 1));
  SET_STRING_ELT(ans, 0, mkChar(buffer));

  return ans;
}

SEXP C_process_write (SEXP _handle, SEXP _message)
{
  SEXP ptr = getAttrib(_handle, install("handle_ptr"));
  if (ptr == R_NilValue) {
    Rf_error("`handle_ptr` attribute not found");
  }
  if (!isString(_message) || (LENGTH(_message) != 1)) {
    Rf_error("`message` must be a single character value");
  }

  const char * message = translateChar(STRING_ELT(_message, 0));

  process_handle_t * process_handle = (process_handle_t*)R_ExternalPtrAddr(ptr);
  process_write(process_handle, message, strlen(message));

  return R_NilValue;
}


