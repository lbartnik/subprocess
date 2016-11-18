#' @useDynLib subprocess
NULL


#' Start a new child process.
#' 
#' @description
#' In Linux, the usual combination of \code{fork()} and \code{exec()}
#' is used to spawn a new child process. Standard streams are redirected
#' over regular unnamed \code{pipe}s.
#' 
#' In Windows a new process is spawned with \code{CreateProcess()} and
#' streams are redirected over unnamed pipes obtained with
#' \code{CreatePipe()}. However, because non-blocking (\emph{overlapped}
#' in Windows-speak) read/write is not supported for unnamed pipes,
#' two reader threads are created for each new child process. These
#' threads never touch memory allocated by R and thus they will not
#' interfere with R interpreter's memory management (garbage collection).
#' 
#' 
#' @details
#' \code{command} is always prepended to \code{arguments} so that the
#' child process can correcty recognize the name of its executable
#' via its \code{argv} vector. This is done automatically by
#' \code{spawn_process}.
#' 
#' \code{environment} can be passed as a \code{character} vector whose
#' elements take the form \code{"NAME=VALUE"}, a named \code{character}
#' vector or a named \code{list}.
#' 
#' \code{workdir} is the path to the directory where the new process is
#' ought to be started. \code{NULL} and \code{""} mean that working
#' directory is inherited from the parent.
#' 
#' @section Termination:
#' 
#' The \code{termination_mode} specifies what should happen when
#' \code{process_terminate()} or \code{process_kill()} is called on a
#' subprocess. If it is set to \code{TERMINATION_GROUP}, then the
#' termination signal is sent to the parent and all its descendants
#' (sub-processes). If termination mode is set to
#' \code{TERMINATION_CHILD_ONLY}, only the child process spawned
#' directly from the R session receives the signal.
#' 
#' In Windows this is implemented with the job API, namely
#' \code{CreateJobObject()}, \code{AssignProcessToJobObject()} and
#' \code{TerminateJobObject()}. In Linux, the child calls \code{setsid()}
#' after \code{fork()} but before \code{execve()}, and \code{kill()} is
#' called with the negate process id.
#' 
#' @param command Path to the executable.
#' @param arguments Optional arguments for the program.
#' @param environment Optional environment.
#' @param workdir Optional new working directory.
#' @param termination_mode Either \code{TERMINATION_GROUP} or
#'        \code{TERMINATION_CHILD_ONLY}.
#'
#' @return \code{spawn_process()} returns an object of the
#'         \emph{process handle} class.
#' @rdname spawn_process
#' 
#' @format \code{TERMINATION_GROUP} and \code{TERMINATION_CHILD_ONLY}
#'         are single \code{character} values.
#' 
#' @export
spawn_process <- function (command, arguments = character(), environment = character(),
                           workdir = "", termination_mode = TERMINATION_GROUP)
{
  command <- as.character(command)
  command <- normalizePath(command, mustWork = TRUE)

  # handle named environment
  if (!is.null(names(environment))) {
    if (any(names(environment) == "")) {
      stop("empty name(s) for environment variables", call. = FALSE)
    }
    environment <- paste(names(environment), as.character(environment), sep = '=')
  }
  
  # hand over to C
  handle <- .Call("C_process_spawn", command, c(command, as.character(arguments)),
                  as.character(environment), as.character(workdir),
                  as.character(termination_mode))

  structure(list(c_handle = handle, command = command, arguments = arguments),
            class = 'process_handle')
}


#' @param x Object to be printed or tested.
#' @param ... Other parameters passed to the \code{print} method.
#' 
#' @export
#' @rdname spawn_process
print.process_handle <- function (x, ...)
{
  cat('Process Handle\n')
  cat('command   : ', x$command, paste(x$arguments, collapse = ' '), '\n')
  cat('system id : ', as.integer(x$c_handle), '\n')
  cat('state     : ', process_poll(x), '\n')
  
  invisible(x)
}


#' @description \code{is_process_handle()} verifies that an object is a
#' valid \emph{process handle} as returned by \code{spawn_process()}.
#' 
#' @export
#' @rdname spawn_process
is_process_handle <- function (x)
{
  inherits(x, 'process_handle')
}


#' Terminating a Child Process.
#' 
#' @description
#' 
#' These functions give access to the state of the child process and to
#' its exit status (return code).
#' 
#' The \code{timeout} parameter can take one of three values:
#' \itemize{
#'   \item \code{0} which means no timeout
#'   \item \code{-1} which means "wait until there is data to read"
#'   \item a positive integer, which is the actual timeout in milliseconds
#' }
#' 
#' @details \code{process_poll} checks the state of the child process.
#' 
#' @param handle Process handle obtained from \code{spawn_process}.
#' @param timeout Optional timeout in milliseconds.
#' 
#' @return \code{process_poll} returns one of these values:
#' \code{"not-started"}. \code{"running"}, \code{"exited"},
#' \code{"terminated"}.
#' 
#' @name terminating
#' @rdname terminating
#' @export
#' 
#' @seealso \code{\link{spawn_process}}, \code{\link{process_read}}
#'          \code{\link{signals}}
#' 
process_poll <- function (handle, timeout = TIMEOUT_IMMEDIATE)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_poll", handle$c_handle, as.integer(timeout))
}


#' @details \code{process_return_code} complements \code{process_poll}
#' by giving access to the child process' exit status (return code). If
#' \code{process_poll} returns neither \code{"exited"} nor
#' \code{"terminated"}, \code{process_return_code} returns \code{NA}.
#' 
#' @rdname terminating
#' @export
#' 
process_return_code <- function (handle)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_return_code", handle$c_handle)
}


#' @details \code{process_wait} combined \code{process_poll} and
#' \code{process_return_code}. It firsts for the process to exit and
#' then returns its exit code.
#' 
#' @rdname terminating
#' @export
#' 
process_wait <- function (handle, timeout = TIMEOUT_INFINITE)
{
  stopifnot(is_process_handle(handle))
  process_poll(handle, timeout)
  process_return_code(handle)
}


#' @description \code{TIMEOUT_INFINITE} denotes an "infinite" timeout
#' (that is, wait until response is available) when waiting for an
#' operation to complete.
#'
#' @rdname terminating
#' @export
TIMEOUT_INFINITE  <- -1L


#' @description \code{TIMEOUT_IMMEDIATE} denotes an "immediate" timeout
#' (in other words, no timeout) when waiting for an operation to
#' complete.
#' 
#' @rdname terminating
#' @export
TIMEOUT_IMMEDIATE <-  0L


#' @description \code{TERMINATION_GROUP}: \code{process_terminate(handle)}
#' and \code{process_kill(handle)} deliver the signal to the child
#' process pointed to by \code{handle} and all of its descendants.
#' 
#' @rdname spawn_process
#' @export
TERMINATION_GROUP <- "group"


#' @description \code{TERMINATION_CHILD_ONLY}:
#' \code{process_terminate(handle)} and \code{process_kill(handle)}
#' deliver the signal only to the child process pointed to by
#' \code{handle} but to none of its descendants.
#' 
#' @rdname spawn_process
#' @export
TERMINATION_CHILD_ONLY <- "child-only"

