// RegisteringDynamic Symbols

#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#define NO_SYSTEM_API
#include "config-os.h"
#include "rapi.h"


static const R_CallMethodDef callMethods[]  = {
  { "C_process_spawn",        (DL_FUNC) &C_process_spawn,        5 },
  { "C_process_read",         (DL_FUNC) &C_process_read,         3 },
  { "C_process_close_input",  (DL_FUNC) &C_process_close_input,  1 },
  { "C_process_write",        (DL_FUNC) &C_process_write,        2 },
  { "C_process_wait",         (DL_FUNC) &C_process_wait,         2 },
  { "C_process_return_code",  (DL_FUNC) &C_process_return_code,  1 },
  { "C_process_state",        (DL_FUNC) &C_process_state,        1 },
  { "C_process_terminate",    (DL_FUNC) &C_process_terminate,    1 },
  { "C_process_kill",         (DL_FUNC) &C_process_kill,         1 },
  { "C_process_send_signal",  (DL_FUNC) &C_process_send_signal,  2 },
  { "C_known_signals",        (DL_FUNC) &C_known_signals,        0 },
  { "C_signal",               (DL_FUNC) &C_signal,               2 },
  { NULL, NULL, 0 }
};


void R_init_subprocess(DllInfo* info) {
  R_registerRoutines(info, NULL, callMethods, NULL, NULL);
  R_useDynamicSymbols(info, TRUE);
}

