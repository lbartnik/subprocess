#' @useDynLib subprocess, .registration = TRUE
NULL


#' Start a new child process.
#'
#' @description
#' In Linux, the usual combination of `fork()` and `exec()`
#' is used to spawn a new child process. Standard streams are redirected
#' over regular unnamed `pipe`s.
#'
#' In Windows a new process is spawned with `CreateProcess()` and
#' streams are redirected over unnamed pipes obtained with
#' `CreatePipe()`. However, because non-blocking (*overlapped*
#' in Windows-speak) read/write is not supported for unnamed pipes,
#' two reader threads are created for each new child process. These
#' threads never touch memory allocated by R and thus they will not
#' interfere with R interpreter's memory management (garbage collection).
#'
#'
#' @details
#' `command` is always prepended to `arguments` so that the
#' child process can correcty recognize the name of its executable
#' via its `argv` vector. This is done automatically by
#' `spawn_process`.
#'
#' `environment` can be passed as a `character` vector whose
#' elements take the form `"NAME=VALUE"`, a named `character`
#' vector or a named `list`.
#'
#' `workdir` is the path to the directory where the new process is
#' ought to be started. `NULL` and `""` mean that working
#' directory is inherited from the parent.
#'
#' @section Termination:
#'
#' The `termination_mode` specifies what should happen when
#' `process_terminate()` or `process_kill()` is called on a
#' subprocess. If it is set to `TERMINATION_GROUP`, then the
#' termination signal is sent to the parent and all its descendants
#' (sub-processes). If termination mode is set to
#' `TERMINATION_CHILD_ONLY`, only the child process spawned
#' directly from the R session receives the signal.
#'
#' In Windows this is implemented with the job API, namely
#' `CreateJobObject()`, `AssignProcessToJobObject()` and
#' `TerminateJobObject()`. In Linux, the child calls `setsid()`
#' after `fork()` but before `execve()`, and `kill()` is
#' called with the negate process id.
#'
#' @param command Path to the executable.
#' @param arguments Optional arguments for the program.
#' @param environment Optional environment.
#' @param workdir Optional new working directory.
#' @param termination_mode Either `TERMINATION_GROUP` or
#'        `TERMINATION_CHILD_ONLY`.
#'
#' @return `spawn_process()` returns an object of the
#'         *process handle* class.
#' @rdname spawn_process
#'
#' @format `TERMINATION_GROUP` and `TERMINATION_CHILD_ONLY`
#'         are single `character` values.
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

  if(!(is.null(workdir) || identical(workdir, ""))){
    workdir <- normalizePath(workdir, mustWork = TRUE)
  }
  # hand over to C
  handle <- .Call("C_process_spawn", command, c(command, as.character(arguments)),
                  as.character(environment), as.character(workdir),
                  as.character(termination_mode))

  structure(list(c_handle = handle, command = command, arguments = arguments),
            class = 'process_handle')
}


#' @param x Object to be printed or tested.
#' @param ... Other parameters passed to the `print` method.
#'
#' @export
#' @rdname spawn_process
print.process_handle <- function (x, ...)
{
  cat('Process Handle\n')
  cat('command   : ', x$command, ' ', paste(x$arguments, collapse = ' '), '\n', sep = '')
  cat('system id : ', as.integer(x$c_handle), '\n', sep = '')
  cat('state     : ', process_state(x), '\n', sep = '')

  invisible(x)
}


#' @description `is_process_handle()` verifies that an object is a
#' valid *process handle* as returned by `spawn_process()`.
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
#' The `timeout` parameter can take one of three values:
#' \itemize{
#'   \item `0` which means no timeout
#'   \item `-1` which means "wait until there is data to read"
#'   \item a positive integer, which is the actual timeout in milliseconds
#' }
#'
#' @details `process_wait()` checks the state of the child process
#' by invoking the system call `waitpid()` or
#' `WaitForSingleObject()`.
#'
#' @param handle Process handle obtained from `spawn_process`.
#' @param timeout Optional timeout in milliseconds.
#'
#' @return `process_wait()` returns an `integer` exit code
#' of the child process or `NA` if the child process has not exited
#' yet. The same value can be accessed by `process_return_code()`.
#'
#' @name terminating
#' @rdname terminating
#' @export
#'
#' @seealso [spawn_process()], [process_read()]
#'          [signals()]
#'
process_wait <- function (handle, timeout = TIMEOUT_INFINITE)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_wait", handle$c_handle, as.integer(timeout))
}


#' @details `process_state()` refreshes the handle by calling
#' `process_wait()` with no timeout and returns one of these
#' values: `"not-started"`. `"running"`, `"exited"`,
#' `"terminated"`.
#'
#' @rdname terminating
#' @export
#'
process_state <- function (handle)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_state", handle$c_handle)
}


#' @details `process_return_code()` gives access to the value
#' returned also by `process_wait()`. It does not invoke
#' `process_wait()` behind the scenes.
#'
#' @rdname terminating
#' @export
#'
process_return_code <- function (handle)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_return_code", handle$c_handle)
}


#' Check if process with a given id exists.
#'
#' @param x A process handle returned by [spawn_process] or a OS-level process id.
#' @return `TRUE` if process exists, `FALSE` otherwise.
#'
#' @export
#'
process_exists <- function (x)
{
  if (is_process_handle(x)) {
    x <- x$c_handle
  }

  isTRUE(.Call("C_process_exists", as.integer(x)))
}


#' @description `TIMEOUT_INFINITE` denotes an "infinite" timeout
#' (that is, wait until response is available) when waiting for an
#' operation to complete.
#'
#' @rdname terminating
#' @export
TIMEOUT_INFINITE  <- -1L


#' @description `TIMEOUT_IMMEDIATE` denotes an "immediate" timeout
#' (in other words, no timeout) when waiting for an operation to
#' complete.
#'
#' @rdname terminating
#' @export
TIMEOUT_IMMEDIATE <-  0L


#' @description `TERMINATION_GROUP`: `process_terminate(handle)`
#' and `process_kill(handle)` deliver the signal to the child
#' process pointed to by `handle` and all of its descendants.
#'
#' @rdname spawn_process
#' @export
TERMINATION_GROUP <- "group"


#' @description `TERMINATION_CHILD_ONLY`:
#' `process_terminate(handle)` and `process_kill(handle)`
#' deliver the signal only to the child process pointed to by
#' `handle` but to none of its descendants.
#'
#' @rdname spawn_process
#' @export
TERMINATION_CHILD_ONLY <- "child_only"

