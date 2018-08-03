#' Sending signals to the child process.
#' 
#' @param handle Process handle obtained from `spawn_process()`.
#' 
#' @seealso [spawn_process()]
#' 
#' @format An object of class `list`.
#' @rdname signals
#' @name signals
NULL


#' @description Operating-System-level signals that can be sent via
#' [process_send_signal] are defined in the `subprocess::signals`` list.
#' It is a list that is generated when the package is loaded and it
#' contains only signals supported by the current platform (Windows or
#' Linux).
#' 
#' All signals, both supported and not supported by the current
#' platform, are also exported under their names. If a given signal
#' is not supported on the current platform, then its value is set to
#' `NA`.
#' 
#' Calling `process_kill()` and `process_terminate()` invokes
#' the appropriate OS routine (`waitpid()` or
#' `WaitForSingleObject()`, closing the process handle, etc.) that
#' effectively lets the operating system clean up after the child
#' process. Calling `process_send_signal()` is not accompanied by
#' such clean-up and if the child process exits it needs to be followed
#' by a call to [process_wait()].
#' 
#' @details
#' In Windows, signals are delivered either only to the child process or
#' to the child process and all its descendants. This behavior is
#' controlled by the `termination_mode` argument of the
#' [subprocess::spawn_process()] function. Setting it to
#' `TERMINATION_GROUP` results in signals being delivered to the
#' child and its descendants.
#' 
#' @rdname signals
#' @export
#' 
#' @examples
#' \dontrun{
#' # send the SIGKILL signal to bash
#' h <- spawn_process('bash')
#' process_signal(h, signals$SIGKILL)
#' process_signal(h, SIGKILL)
#' 
#' # is SIGABRT supported on the current platform?
#' is.na(SIGABRT)
#' }
#' 
signals <- character()


#' @description `process_terminate()` on Linux sends the
#' `SIGTERM` signal to the process pointed to by `handle`.
#' On Windows it calls `TerminateProcess()`.
#' 
#' @rdname signals
#' @export
#' 
process_terminate <- function (handle)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_terminate", handle$c_handle)
}


#' @description `process_kill()` on Linux sends the `SIGKILL`
#' signal to `handle`. On Windows it is an alias for
#' `process_terminate()`.
#' 
#' @rdname signals
#' @export
#' 
process_kill <- function (handle)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_kill", handle$c_handle)
}


#' @description `process_send_signal()` sends an OS-level
#' `signal` to `handle`. In Linux all standard signal
#' numbers are supported. On Windows supported signals are
#' `SIGTERM`, `CTRL_C_EVENT` and `CTRL_BREAK_EVENT`.
#' Those values will be available via the `signals` list which
#' is also attached in the package namespace.
#' 
#' @param signal Signal number, one of `names(signals)`.
#' 
#' @rdname signals
#' @export
#' 
#' @examples
#' \dontrun{
#' # Windows
#' process_send_signal(h, SIGTERM)
#' process_send_signal(h, CTRL_C_EVENT)
#' process_send_signal(h, CTRL_BREAK_EVENT)
#' }
#' 
process_send_signal <- function (handle, signal)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_send_signal", handle$c_handle, as.integer(signal))
}


#' @export
#' @rdname signals
SIGABRT <- NA

#' @export
#' @rdname signals
SIGALRM <- NA

#' @export
#' @rdname signals
SIGCHLD <- NA

#' @export
#' @rdname signals
SIGCONT <- NA

#' @export
#' @rdname signals
SIGFPE <- NA

#' @export
#' @rdname signals
SIGHUP <- NA

#' @export
#' @rdname signals
SIGILL <- NA

#' @export
#' @rdname signals
SIGINT <- NA

#' @export
#' @rdname signals
SIGKILL <- NA

#' @export
#' @rdname signals
SIGPIPE <- NA

#' @export
#' @rdname signals
SIGQUIT <- NA

#' @export
#' @rdname signals
SIGSEGV <- NA

#' @export
#' @rdname signals
SIGSTOP <- NA

#' @export
#' @rdname signals
SIGTERM <- NA

#' @export
#' @rdname signals
SIGTSTP <- NA

#' @export
#' @rdname signals
SIGTTIN <- NA

#' @export
#' @rdname signals
SIGTTOU <- NA

#' @export
#' @rdname signals
SIGUSR1 <- NA

#' @export
#' @rdname signals
SIGUSR2 <- NA

#' @export
#' @rdname signals
CTRL_C_EVENT <- NA

#' @export
#' @rdname signals
CTRL_BREAK_EVENT <- NA


#' A helper function used in vignette.
#' 
#' @param signal Signal number.
#' @param handler Either `"default"` or `"ignore"`.
#'
signal <- function (signal, handler)
{
  .Call("C_signal", as.integer(signal), as.character(handler))
}
