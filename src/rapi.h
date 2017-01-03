/** @file rapi.h
 *
 *  Functions exported to R from the shared library.
 *  @author Lukasz A. Bartnik <l.bartnik@gmail.com>
 */

#ifndef RAPI_H_GUARD
#define RAPI_H_GUARD

#include <Rinternals.h>
#include "config-os.h"


#ifdef __cplusplus
extern "C" {
#endif


EXPORT SEXP C_process_spawn(SEXP _command, SEXP _arguments, SEXP _environment, SEXP _workdir, SEXP _termination_mode);

EXPORT SEXP C_process_read(SEXP _handle, SEXP _pipe, SEXP _timeout);

EXPORT SEXP C_process_close_input (SEXP _handle);

EXPORT SEXP C_process_write(SEXP _handle, SEXP _message);

EXPORT SEXP C_process_wait(SEXP _handle, SEXP _timeout);

EXPORT SEXP C_process_return_code(SEXP _handle);

EXPORT SEXP C_process_state(SEXP _handle);

EXPORT SEXP C_process_terminate(SEXP _handle);

EXPORT SEXP C_process_kill(SEXP _handle);

EXPORT SEXP C_process_send_signal(SEXP _handle, SEXP _signal);

EXPORT SEXP C_known_signals();


SEXP allocate_single_int (int _value);


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* RAPI_H_GUARD */

