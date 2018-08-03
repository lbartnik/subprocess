#' Communicating with a Child Process
#' 
#' `process_read()` reads data from one of the child process' streams,
#' *standard output* or *standard error output*, and returns it as a
#' `character` vector.
#' 
#' If `flush=TRUE` in `process_read()` then the invocation of the
#' underlying `read()` *system-call* will be repeated until the pipe
#' buffer is empty.
#' 
#' If `pipe` is set to either `PIPE_STDOUT` or `PIPE_STDERR`, the returned
#' value is a single list with a single key, `stdout` or `stderr`,
#' respectively. If `pipe` is set to `PIPE_BOTH` the returned `list`
#' contains both keys. Values in the list are `character` vectors of 0
#' or more elements, lines read from the respective output stream of the
#' child process.
#' 
#' For details on `timeout` see [terminating].
#' 
#' @param handle Process handle obtained from `spawn_process`.
#' @param pipe Output stream identifier: `PIPE_STDOUT`, `PIPE_STDERR` or
#'             `PIPE_BOTH`.
#' @param timeout Optional timeout in milliseconds.
#' @param flush If there is any data within the given `timeout`
#'              try again with `timeout=0` until C buffer is empty. 
#' 
#' @return `process_read` returns a `list` which contains either of or
#'         both keys: *stdout* and *stderr*; the value is in both cases
#'         a `character` vector which contains lines of child's output.
#' 
#' @format `PIPE_STDOUT`, `PIPE_STDERR` and `PIPE_BOTH` are single
#'         `character` values.
#'
#' @rdname readwrite
#' @name readwrite
#' @export
#' 
process_read <- function (handle, pipe = PIPE_BOTH, timeout = TIMEOUT_IMMEDIATE, flush = TRUE)
{
  stopifnot(is_process_handle(handle))
  output <- .Call("C_process_read", handle$c_handle,
                  as.character(pipe), as.integer(timeout))

  is_output <- function (x) {
    return(is.list(output) && all(vapply(output, is.character, logical(1))))
  }
  paste0_list <- function (x, y) {
    z <- lapply(names(x), function (n) paste0(x[[n]], y[[n]]))
    `names<-`(z, names(x))
  }

  # needs to be a list of character vectors
  if (!is_output(output)) return(output)

  # there is some output, maybe there will be more?
  if (isTRUE(flush)) {
    while (TRUE) {
      more <- .Call("C_process_read", handle$c_handle, as.character(pipe), TIMEOUT_IMMEDIATE)
      if (!is_output(more) || all(vapply(more, nchar, integer(1)) == 0))
        break
      output <- paste0_list(output, more)
    }
  }

  # replace funny line ending and break into multiple lines
  output <- lapply(output, function (single_stream) {
    if (!length(single_stream)) return(character())
    single_stream <- gsub("\r", "", single_stream, fixed = TRUE)
    strsplit(single_stream, "\n", fixed = TRUE)[[1]]
  })
  
  # if asked for only one pipe return the vector, not the list
  if (identical(pipe, PIPE_STDOUT) || identical(pipe, PIPE_STDERR)) {
    return(output[[pipe]])
  }
  
  # return a lits
  return(output)
}


#' @description `process_write()` writes data into child's
#' *standard input* stream.
#' 
#' @param message Input for the child process.
#' @return `process_write` returns the number of characters written.
#' 
#' @rdname readwrite
#' @name readwrite
#' @export
#' 
process_write <- function (handle, message)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_write", handle$c_handle, as.character(message))
}


#' @description `process_close_input()` closes the *write* end
#' of the pipe whose *read* end is the standard input stream of the
#' child process. This is a standard way to gracefully request the child
#' process to exit.
#'  
#' @rdname readwrite
#' @name readwrite
#' @export
#' 
process_close_input <- function (handle)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_close_input", handle$c_handle)
}



#' @description `PIPE_STDOUT`: read from child's standard output.
#' 
#' @rdname readwrite
#' @export
PIPE_STDOUT <- "stdout"


#' @description `PIPE_STDERR`: read from child's standard error
#' output.
#' 
#' @rdname readwrite
#' @export
PIPE_STDERR <- "stderr"


#' @description `PIPE_BOTH`: read from both child's output streams:
#' standard output and standard error output.
#' 
#' @rdname readwrite
#' @export
PIPE_BOTH <- "both"

