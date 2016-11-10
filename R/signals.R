#' Sending signals to the child process.
#' 
#' @param handle Process handle obtained from \code{spawn_process()}.
#' 
#' @seealso \code{\link{spawn_process}}
#' 
#' @format An object of class \code{list}.
#' @rdname signals
#' @name signals
NULL


#' @description Operating System-level signals that can be used with
#' \code{\link[subprocess]{process_send_signal}} are defined in the
#' \code{signals} list which is generated automatically when package is
#' loaded. The list is also attached upon package load so signals are
#' available directly under their names.
#' 
#' @rdname signals
#' @export
#' 
#' @examples
#' \dontrun{
#' h <- spawn_process('bash')
#' process_signal(h, signals$SIGKILL)
#' process_signal(h, SIGKILL)
#' }
#' 
signals <- character()


#' @description \code{process_terminate} on Linux sends the
#' \code{SIGTERM} signal to the process pointed to by \code{handle}.
#' On Windows it calls \code{TerminateProcess()}.
#' 
#' @rdname signals
#' @export
#' 
process_terminate <- function (handle)
{
  .Call("C_process_terminate", handle)
}


#' @description \code{process_kill} on Linux sends the \code{SIGKILL}
#' signal to \code{handle}. On Windows it is an alias for
#' \code{process_terminate}.
#' 
#' @rdname signals
#' @export
#' 
process_kill <- function (handle)
{
  .Call("C_process_kill", handle)
}


#' @description \code{process_send_signal} sends an OS-level
#' \code{signal} to \code{handle}. In Linux all standard signal
#' numbers are supported. On Windows supported signals are
#' \code{SIGTERM}, \code{CTRL_C_EVENT} and \code{CTRL_BREAK_EVENT}.
#' Those values will be available via the \code{signals} list which
#' is also attached in the package namespace.
#' 
#' @param signal Signal number, one of \code{names(signals)}.
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
  .Call("C_process_terminate", handle, as.integer(signal))
}
